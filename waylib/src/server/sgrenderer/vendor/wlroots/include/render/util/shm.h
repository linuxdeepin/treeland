// SPDX-FileCopyrightText: 2017-2023 wlroots contributors
// SPDX-License-Identifier: MIT
//
// Vendored from wlroots 0.19.3 (commit 88a869855742281c98c22cab9641b317b8d065ef)
// Source path: include/util/shm.h
// Modifications: Vendored for waylib sgrenderer. Initial vendor, no functional changes.

#ifndef UTIL_SHM_H
#define UTIL_SHM_H

#include <stdbool.h>
#include <stddef.h>

int create_shm_file(void);
int allocate_shm_file(size_t size);
bool allocate_shm_file_pair(size_t size, int *rw_fd, int *ro_fd);

#endif
