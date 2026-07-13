// SPDX-FileCopyrightText: 2017-2023 wlroots contributors
// SPDX-License-Identifier: MIT
//
// Vendored from wlroots 0.19.3 (commit 88a869855742281c98c22cab9641b317b8d065ef)
// Source path: util/env.c
// Modifications: Vendored for waylib sgrenderer. Initial vendor, no functional changes.

#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "util/env.h"

bool env_parse_bool(const char *option) {
	const char *env = getenv(option);
	if (env) {
		wlr_log(WLR_INFO, "Loading %s option: %s", option, env);
	}

	if (!env || strcmp(env, "0") == 0) {
		return false;
	} else if (strcmp(env, "1") == 0) {
		return true;
	}

	wlr_log(WLR_ERROR, "Unknown %s option: %s", option, env);
	return false;
}

size_t env_parse_switch(const char *option, const char **switches) {
	const char *env = getenv(option);
	if (env) {
		wlr_log(WLR_INFO, "Loading %s option: %s", option, env);
	} else {
		return 0;
	}

	for (ssize_t i = 0; switches[i]; i++) {
		if (strcmp(env, switches[i]) == 0) {
			return i;
		}
	}

	wlr_log(WLR_ERROR, "Unknown %s option: %s", option, env);
	return 0;
}
