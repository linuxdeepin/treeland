#!/usr/bin/env python3
# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

"""Interactively control Treeland's system effect level and per-user animation/effect thresholds.

Only Python's standard library and the installed dde-dconfig CLI are needed.
Copy this file to the target machine and run: python3 animation-control.py
"""

import argparse
import json
import os
from pathlib import Path
import pwd
import re
import shlex
import shutil
import subprocess
import sys

APP = "org.deepin.dde.treeland"
GLOBAL_RESOURCE = APP
USER_RESOURCE = APP + ".user"

EFFECT_LEVELS = ("none", "low", "medium", "high")
LEVEL_VALUES = {"none": 0, "low": 1, "medium": 2, "high": 3}
LEVEL_LABELS = {
    "none": "无（0，全部关闭）",
    "low": "低（1）",
    "medium": "中（2）",
    "high": "高（3）",
}
THRESHOLD_LABELS = {
    0: "0 (none 始终关闭)",
    1: "1 (low 起开启)",
    2: "2 (medium 起开启)",
    3: "3 (仅 high 开启)",
}

ANIMATION_LABELS = {
    'lockscreen': '登录与锁屏界面',
    'windowOpen': '窗口打开动画',
    'windowClose': '窗口关闭动画',
    'launchpadOpenFullscreen': '启动器打开动画',
    'launchpadCloseFullscreen': '启动器关闭动画',
    'layerShellOpen': '图层窗口打开动画',
    'layerShellClose': '图层窗口关闭动画',
    'maximize': '最大化动画',
    'fullscreen': '全屏动画',
    'restoreWindow': '窗口还原动画',
    'minimize': '最小化动画',
    'restoreMinimized': '最小化恢复动画',
    'showDesktop': '显示桌面动画',
    'restoreShowDesktop': '退出显示桌面动画',
    'launchpadCover': '启动器覆盖动画',
    'wallpaperScale': '壁纸缩放动画',
    'workspaceSwitch': '工作区切换动画',
    'workspaceVisibility': '工作区显隐动画',
    'workspaceBounce': '工作区边界回弹动画',
    'taskSwitcher': '任务切换',
    'multitaskview': '多任务视图',
    'outputRotation': '主输出旋转动画',
    'mirrorOutputRotation': '镜像输出旋转动画',
    'wallpaperCrossFade': '壁纸交叉渐变动画',
    'dockPreview': 'Dock 预览',
    'captureToolbar': '截图工具条',
    'edgeTilePreview': '边缘平铺预览',
}

LEVEL_EFFECT_LABELS = {
    'windowCornerRadiusEnabled': '窗口圆角',
    'windowShadowEnabled': '窗口阴影',
    'windowBorderEnabled': '窗口描边',
    'windowTransparencyEnabled': '窗口透明度修饰',
    'backgroundEffectEnabled': '背景特效',
}

USER_EFFECT_LABELS = {
    'glassEnabled': '液态玻璃效果',
    'lockscreenWallpaperDimEnabled': '锁屏壁纸压暗',
}

GROUPS = (
    ("动画档位阈值", USER_RESOURCE, "level", ANIMATION_LABELS),
    ("特效档位阈值", USER_RESOURCE, "level", LEVEL_EFFECT_LABELS),
    ("独立特效开关", USER_RESOURCE, "bool", USER_EFFECT_LABELS),
)

ITEMS = {
    key: (resource, kind, label)
    for _, resource, kind, mapping in GROUPS
    for key, label in mapping.items()
}


def run(command):
    result = subprocess.run(command, text=True, capture_output=True, timeout=20)
    if result.returncode:
        raise RuntimeError((result.stderr or result.stdout).strip() or
                           f"命令失败：{shlex.join(command)}")
    return result.stdout.strip()


def run_dconfig(command):
    result = subprocess.run(command, text=True, capture_output=True, timeout=20)
    stdout = result.stdout.strip()
    stderr = result.stderr.strip()
    if result.returncode:
        raise RuntimeError(stderr or stdout or f"命令失败：{shlex.join(command)}")
    if stderr and ("value error" in stderr.lower() or "non-existent" in stderr.lower()
                   or "error" in stderr.lower()):
        raise RuntimeError(
            f"{stderr}\n提示：若刚更新了 /usr/share/dsg/configs/ 下的 JSON，"
            "请先重启 dde-dconfig-daemon（sudo systemctl restart dde-dconfig-daemon）再重试。"
        )
    return stdout


def parse_bool(output, key):
    """Accept JSON or a single CLI boolean; never infer false from an error."""
    try:
        value = json.loads(output)
    except ValueError:
        value = None
    if type(value) is bool:
        return value
    if isinstance(value, dict):
        for name in (key, "value"):
            if type(value.get(name)) is bool:
                return value[name]
    words = re.findall(r"\b(true|false)\b", output, re.IGNORECASE)
    if len(words) == 1:
        return words[0].lower() == "true"
    raise RuntimeError(f"无法解析 {key} 的布尔值：{output!r}")


def parse_int(output, key):
    """Accept JSON or a single CLI integer."""
    try:
        value = json.loads(output)
    except ValueError:
        value = None
    if type(value) is int:
        return value
    if isinstance(value, dict):
        for name in (key, "value"):
            if type(value.get(name)) is int:
                return value[name]
    nums = re.findall(r"-?\b\d+\b", output)
    if len(nums) == 1:
        return int(nums[0])
    raise RuntimeError(f"无法解析 {key} 的整数值：{output!r}")


def parse_threshold(token):
    lowered = token.lower()
    if lowered in LEVEL_VALUES:
        return LEVEL_VALUES[lowered]
    if lowered.isdigit():
        value = int(lowered)
        if 0 <= value <= 3:
            return value
    raise RuntimeError(f"不支持的阈值：{token}（可选：none/0, low/1, medium/2, high/3）")


def format_threshold(value):
    return THRESHOLD_LABELS.get(value, str(value))


def is_enabled(level, threshold):
    level_val = LEVEL_VALUES.get(level, LEVEL_VALUES["high"])
    return threshold > LEVEL_VALUES["none"] and level_val >= threshold


def compositor_owners():
    owners = set()
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if (entry / "comm").read_text().strip() == "treeland":
                owners.add(pwd.getpwuid(entry.stat().st_uid).pw_name)
        except (OSError, KeyError):
            continue
    return sorted(owners)


def desktop_users():
    if not shutil.which("loginctl"):
        return []
    users = set()
    try:
        sessions = run(["loginctl", "list-sessions", "--no-legend", "--no-pager"])
        for line in sessions.splitlines():
            fields = line.split()
            if not fields:
                continue
            properties = run(["loginctl", "show-session", fields[0],
                              "-p", "Name", "-p", "Type", "-p", "Class", "-p", "Active"])
            values = dict(line.split("=", 1) for line in properties.splitlines()
                          if "=" in line)
            if (values.get("Active") == "yes"
                    and values.get("Type") in ("wayland", "x11")
                    and values.get("Class") in ("user", "user-early")):
                if values.get("Name"):
                    users.add(values["Name"])
    except (RuntimeError, subprocess.TimeoutExpired):
        pass
    return sorted(users)


def choose_user(explicit, candidates, question):
    if explicit:
        pwd.getpwnam(explicit)
        return explicit
    if len(candidates) == 1:
        return candidates[0]
    if not sys.stdin.isatty():
        raise RuntimeError(question + "；请通过命令行参数指定。")
    if candidates:
        print("候选账户：" + "、".join(candidates))
    value = input(question + "：").strip()
    pwd.getpwnam(value)
    return value


class Controller:
    def __init__(self, owner, user):
        self.owner = owner
        self.user = user
        self.uid = pwd.getpwnam(user).pw_uid
        self.prefix = []
        if os.geteuid() != pwd.getpwnam(owner).pw_uid:
            if not shutil.which("sudo"):
                raise RuntimeError(f"需要以 {owner} 身份调用配置工具，请安装 sudo 或切换账户。")
            self.prefix = ["sudo", "-u", owner, "--"]

    def command(self, action, key, *extra):
        resource, _, _ = ITEMS[key]
        return self.prefix + ["dde-dconfig", action, "-u", str(self.uid), "-a", APP,
                              "-r", resource, "-k", key, *extra]

    def level_command(self, action, *extra):
        return self.prefix + ["dde-dconfig", action, "-a", APP,
                              "-r", GLOBAL_RESOURCE, "-k", "effectLevel", *extra]

    def get_level(self):
        output = run_dconfig(self.level_command("get"))
        try:
            value = json.loads(output)
        except ValueError:
            value = None
        if isinstance(value, str):
            return value
        if isinstance(value, dict):
            for name in ("effectLevel", "value"):
                if isinstance(value.get(name), str):
                    return value[name]
        matches = re.findall(r"\b(none|low|medium|high)\b", output, re.IGNORECASE)
        if len(matches) == 1:
            return matches[0].lower()
        raise RuntimeError(f"无法解析 effectLevel 的值：{output!r}")

    def set_level(self, level):
        if level not in EFFECT_LEVELS:
            raise RuntimeError(f"不支持的档位：{level}（可选：{', '.join(EFFECT_LEVELS)}）")
        run_dconfig(self.level_command("set", "-v", json.dumps(level)))
        actual = self.get_level()
        if actual != level:
            raise RuntimeError("effectLevel 写入后读回不一致，可能受只读策略限制。")
        print(f"effectLevel（系统动效与特效档位）：{actual}（{LEVEL_LABELS[actual]}），配置已读回确认。")

    def get(self, key):
        _, kind, _ = ITEMS[key]
        output = run_dconfig(self.command("get", key))
        return parse_int(output, key) if kind == "level" else parse_bool(output, key)

    def set(self, key, value):
        _, kind, label = ITEMS[key]
        if kind == "level":
            if type(value) is not int or value < 0 or value > 3:
                raise RuntimeError(f"{key} 的阈值必须在 0-3 之间：{value}")
            serialized = str(value)
        else:
            serialized = str(bool(value)).lower()
        run_dconfig(self.command("set", key, "-v", serialized))
        actual = self.get(key)
        if actual != value:
            raise RuntimeError(f"{key} 写入后读回不一致，可能受只读策略限制。")
        if kind == "level":
            print(f"{key}（{label}）：阈值 {format_threshold(actual)}，配置已读回确认。")
        else:
            print(f"{key}（{label}）：{'开启' if actual else '关闭'}，配置已读回确认。")

    def set_enabled_for_level(self, key, enabled, level):
        _, kind, _ = ITEMS[key]
        if kind == "bool":
            self.set(key, enabled)
            return
        level_val = LEVEL_VALUES.get(level, LEVEL_VALUES["high"])
        if enabled and level_val == LEVEL_VALUES["none"]:
            raise RuntimeError("当前系统档位为 none（全部关闭），请先切换到 low/medium/high 再开启单项。")
        updated = level_val if enabled else LEVEL_VALUES["none"]
        self.set(key, updated)

    def snapshot(self):
        return {key: self.get(key) for key in ITEMS}


def show(level, values):
    print(f"\n当前系统档位：{level}（{LEVEL_LABELS.get(level, '未知，回退 high')}）")
    index = 1
    for title, _, kind, mapping in GROUPS:
        print(f"\n[{title}]")
        if kind == "level":
            print(" 编号  当前  最低档位            配置键                         功能")
            for key, label in mapping.items():
                threshold = values[key]
                active = "开" if is_enabled(level, threshold) else "关"
                print(f" {index:>2}    {active}    {format_threshold(threshold):<19} {key:<30} {label}")
                index += 1
        else:
            print(" 编号  状态  配置键                         功能")
            for key, label in mapping.items():
                print(f" {index:>2}    {'开' if values[key] else '关'}    {key:<30} {label}")
                index += 1
    print()


def resolve(token):
    keys = list(ITEMS)
    if token.isdigit() and 1 <= int(token) <= len(keys):
        return keys[int(token) - 1]
    if token in ITEMS:
        return token
    raise RuntimeError(f"未知编号或配置键：{token}")


def resolve_targets(token):
    lowered = token.lower()
    if lowered == "all":
        return list(ITEMS)
    if lowered in ("animation", "animations", "anim"):
        return list(ANIMATION_LABELS)
    if lowered in ("effect", "effects"):
        return list(LEVEL_EFFECT_LABELS) + list(USER_EFFECT_LABELS)
    return [resolve(token)]


HELP = """命令：
  level                      查看当前系统档位
  level none|low|medium|high 切换当前系统档位（none=0, low=1, medium=2, high=3）
  set 3 none|low|medium|high 设置编号 3 从哪个档位起开启（或 0/1/2/3；0=始终关闭）
  3                          在当前档位下切换编号 3 的开关
  off 3 / on 3               在当前档位下关闭／打开编号 3
  off all / on all           在当前档位下关闭／打开全部动画与特效
  off anim / on anim         在当前档位下关闭／打开全部动画
  off effect / on effect     在当前档位下关闭／打开全部特效
  list                       刷新当前状态
  restore                    恢复本次启动时记录的档位与配置值
  help                       查看帮助
  quit                       退出（保留当前设置）
"""


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--user", help="被测试桌面登录用户名，默认自动识别")
    parser.add_argument("--compositor-user", help="treeland 进程属主，默认自动识别")
    args = parser.parse_args(argv)
    if not shutil.which("dde-dconfig"):
        raise RuntimeError("未找到 dde-dconfig；请在已安装本次 Treeland 的目标机器运行。")
    owner = choose_user(args.compositor_user, compositor_owners(), "treeland 进程属主用户名")
    candidates = desktop_users()
    if not candidates and owner != "dde":
        candidates = [owner]
    user = choose_user(args.user, candidates, "被测试的桌面登录用户名（不是 SSH 用户名）")
    controller = Controller(owner, user)
    print(f"进程属主：{owner}；被测桌面用户：{user}（UID {controller.uid}）")
    print("仅修改配置，不重启 Treeland。设置会保存；请输入 restore 恢复本次初始值。")
    print("请在动画结束后修改，再触发功能观察效果；读回成功不代表视觉效果已通过验收。")
    original_level = controller.get_level()
    original = controller.snapshot()
    show(original_level, original)
    print(HELP)
    while True:
        try:
            parts = shlex.split(input("开关> "))
            if not parts:
                continue
            action = parts[0].lower()
            if action in ("quit", "q", "exit"):
                break
            if action in ("help", "h", "?"):
                print(HELP)
            elif action == "level" and len(parts) == 1:
                current = controller.get_level()
                print(f"当前系统档位：{current}（{LEVEL_LABELS.get(current, '未知，回退 high')}）")
            elif action == "level" and len(parts) == 2:
                controller.set_level(parts[1].lower())
            elif action == "set" and len(parts) == 3:
                threshold = parse_threshold(parts[2])
                targets = resolve_targets(parts[1])
                level_targets = [key for key in targets if ITEMS[key][1] == "level"]
                bool_targets = [key for key in targets if ITEMS[key][1] != "level"]
                if not level_targets:
                    raise RuntimeError(f"{bool_targets[0]} 是布尔开关，请用 on/off。")
                if bool_targets:
                    print(f"提示：已跳过布尔开关（{', '.join(bool_targets)}），请用 on/off 单独控制。")
                for key in level_targets:
                    controller.set(key, threshold)
            elif action in ("list", "ls", "status") and len(parts) == 1:
                show(controller.get_level(), controller.snapshot())
            elif action == "restore" and len(parts) == 1:
                failures = []
                try:
                    controller.set_level(original_level)
                except (RuntimeError, subprocess.TimeoutExpired) as error:
                    failures.append(f"effectLevel: {error}")
                for key, value in original.items():
                    try:
                        controller.set(key, value)
                    except (RuntimeError, subprocess.TimeoutExpired) as error:
                        failures.append(f"{key}: {error}")
                if failures:
                    raise RuntimeError("部分配置恢复失败：\n  " + "\n  ".join(failures))
                print("已恢复本次启动时的值。")
            elif action in ("on", "off") and len(parts) == 2:
                current = controller.get_level()
                for key in resolve_targets(parts[1]):
                    controller.set_enabled_for_level(key, action == "on", current)
            elif len(parts) == 1:
                current = controller.get_level()
                key = resolve(parts[0])
                _, kind, _ = ITEMS[key]
                if kind == "level":
                    is_on = is_enabled(current, controller.get(key))
                    controller.set_enabled_for_level(key, not is_on, current)
                else:
                    controller.set(key, not controller.get(key))
            else:
                raise RuntimeError("命令格式不正确；输入 help 查看用法。")
        except (RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
            print(f"错误：{error}", file=sys.stderr)
        except (EOFError, KeyboardInterrupt):
            print()
            break
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, KeyError, ValueError, OSError, subprocess.TimeoutExpired) as error:
        print(f"错误：{error}", file=sys.stderr)
        sys.exit(1)
