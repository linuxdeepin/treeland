#!/usr/bin/env python3

# Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import os
import glob

def normalize_name(name):
    """标准化名称，处理下划线"""
    return name.replace('_', '')

def get_qwlroots_wrapped():
    """获取qwlroots已封装的类型"""
    wrapped = {}

    # types目录
    types_files = glob.glob("src/types/qw*.h")
    wrapped['types'] = set()
    for f in types_files:
        name = os.path.basename(f).removeprefix('qw').removesuffix('.h')  # 去掉qw前缀和.h后缀
        wrapped['types'].add(normalize_name(name))

    # util目录
    util_files = glob.glob("src/util/qw*.h")
    wrapped['util'] = set()
    for f in util_files:
        name = os.path.basename(f).removeprefix('qw').removesuffix('.h')
        wrapped['util'].add(normalize_name(name))

    # render目录
    render_files = glob.glob("src/render/qw*.h")
    wrapped['render'] = set()
    for f in render_files:
        name = os.path.basename(f).removeprefix('qw').removesuffix('.h')
        wrapped['render'].add(normalize_name(name))

    # interfaces目录
    interface_files = glob.glob("src/interfaces/qw*.h")
    wrapped['interfaces'] = set()
    for f in interface_files:
        name = os.path.basename(f).removeprefix('qw').removesuffix('.h')
        wrapped['interfaces'].add(normalize_name(name))

    return wrapped

def get_wlroots_available():
    """获取wlroots可用的类型"""
    available = {}

    # types目录
    types_files = glob.glob("wlroots/include/wlr/types/wlr_*.h")
    available['types'] = set()
    for f in types_files:
        name = os.path.basename(f)[4:-2]  # 去掉wlr_前缀和.h后缀
        available['types'].add(normalize_name(name))

    # util目录
    util_files = glob.glob("wlroots/include/wlr/util/*.h")
    available['util'] = set()
    for f in util_files:
        name = os.path.basename(f)[:-2]
        available['util'].add(normalize_name(name))

    # render目录
    render_files = glob.glob("wlroots/include/wlr/render/*.h")
    available['render'] = set()
    for f in render_files:
        name = os.path.basename(f)[:-2]
        if name.startswith('wlr_'):
            name = name[4:]  # 去掉wlr_前缀
        available['render'].add(normalize_name(name))

    # interfaces目录
    interface_files = glob.glob("wlroots/include/wlr/interfaces/*.h")
    available['interfaces'] = set()
    for f in interface_files:
        name = os.path.basename(f)[:-2]
        if name.startswith('wlr_'):
            name = name[4:]  # 去掉wlr_前缀
        available['interfaces'].add(normalize_name(name))

    return available

def analyze_coverage():
    wrapped = get_qwlroots_wrapped()
    available = get_wlroots_available()

    print("# qwlroots 封装覆盖率分析 (基于 wlroots 0.19.0)")
    print()

    total_wrapped = 0
    total_available = 0

    for category in ['types', 'util', 'render', 'interfaces']:
        wrapped_set = wrapped.get(category, set())
        available_set = available.get(category, set())

        missing = available_set - wrapped_set

        print(f"## {category.title()} 目录")
        print(f"- 已封装: {len(wrapped_set)}")
        print(f"- 可用总数: {len(available_set)}")
        if len(available_set) > 0:
            print(f"- 覆盖率: {len(wrapped_set)/len(available_set)*100:.1f}%")
        else:
            print(f"- 覆盖率: N/A")

        if missing:
            print(f"- 未封装的类型:")
            for item in sorted(missing):
                print(f"  - {item}")
        else:
            print("- ✅ 所有类型都已封装")
        print()

        total_wrapped += len(wrapped_set)
        total_available += len(available_set)

    print(f"## 总体覆盖率")
    print(f"- 总已封装: {total_wrapped}")
    print(f"- 总可用: {total_available}")
    if total_available > 0:
        print(f"- 总覆盖率: {total_wrapped/total_available*100:.1f}%")

if __name__ == "__main__":
    analyze_coverage()
