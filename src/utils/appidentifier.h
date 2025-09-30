// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QString>
#include <sys/types.h>

// 提供根据进程 pid 获取 deepin 应用 appId 的工具函数。
// 逻辑：
//  1. 使用 Linux pidfd_open 获取 pid 描述符
//  2. 通过 D-Bus org.deepin.dde.ApplicationManager1 Identify(pidfd) 获取 appId
//  3. 若失败返回 std::nullopt
//  4. 调用方可做缓存
//
// 线程：建议只在主线程/有 QDBus 事件循环环境中调用。
// 异步需求后续可再扩展。
namespace Treeland::Utils {

// 基于已经获取的 pidfd 直接进行 Identify 查询，避免重复 pidfd_open。
// 注意：传入的 pidfd 由调用方持有，本函数内部会 dup 一份（CLOEXEC），不会关闭原始 fd。
// 失败返回空字符串
QString appIdFromPidfd(int pidfd);

}
