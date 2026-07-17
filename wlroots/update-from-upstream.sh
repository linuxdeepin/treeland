#!/usr/bin/env bash
# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#
# Update vendored wlroots under 3rdparty/wlroots via git subtree (NON-SQUASH).
#
# Why subtree (not squash)?
#   - `git blame 3rdparty/wlroots/<file>` keeps original upstream authors
#   - Full upstream history stays reachable from the treeland branch
#   - Local treeland commits on 3rdparty/wlroots are preserved across pulls
#
# NEVER use `git fetch --depth=1` here: shallow fetches break blame/history.
#
# Usage:
#   wlroots/update-from-upstream.sh                 # pull current REF in UPSTREAM
#   wlroots/update-from-upstream.sh 0.19.4          # tag / branch / SHA
#   wlroots/update-from-upstream.sh --dry-run 0.19.4
#   wlroots/update-from-upstream.sh --force 0.19.4  # allow dirty worktree
#
# After a successful pull the script updates wlroots/UPSTREAM and bumps
# WLROOTS_VERSION* in wlroots/CMakeLists.txt, then stages those metadata
# files. The subtree merge commit is created by git itself — review and
# amend/commit metadata as needed.
#
# Local patches:
#   Commit them normally under 3rdparty/wlroots before running this script.
#   `git subtree pull` three-way-merges your commits with the new upstream.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
PREFIX_REL="3rdparty/wlroots"
PREFIX="${REPO_ROOT}/${PREFIX_REL}"
UPSTREAM_FILE="${SCRIPT_DIR}/UPSTREAM"
CMAKE_FILE="${SCRIPT_DIR}/CMakeLists.txt"

DRY_RUN=0
FORCE=0
NEW_REF=""

usage() {
    sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

log() { printf '==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage 0 ;;
        --dry-run) DRY_RUN=1; shift ;;
        --force) FORCE=1; shift ;;
        -*) die "unknown option: $1" ;;
        *)
            [[ -z "${NEW_REF}" ]] || die "unexpected extra argument: $1"
            NEW_REF=$1
            shift
            ;;
    esac
done

[[ -f "${UPSTREAM_FILE}" ]] || die "missing ${UPSTREAM_FILE}"
[[ -d "${PREFIX}" ]] || die "missing vendored tree ${PREFIX}"

# shellcheck disable=SC1090
source <(grep -E '^(SOURCE_URL|REF|COMMIT|VERSION)=' "${UPSTREAM_FILE}")

[[ -n "${SOURCE_URL:-}" ]] || die "SOURCE_URL not set in UPSTREAM"
[[ -n "${COMMIT:-}" ]] || die "COMMIT not set in UPSTREAM"
BASE_COMMIT=${COMMIT}
BASE_REF=${REF:-}

if [[ -z "${NEW_REF}" ]]; then
    [[ -n "${BASE_REF}" ]] || die "no REF in UPSTREAM and no ref argument given"
    NEW_REF=${BASE_REF}
fi

cd "${REPO_ROOT}"

if [[ "${FORCE}" -ne 1 ]]; then
    dirty=$(git status --porcelain -- \
        "${PREFIX_REL}" "wlroots/UPSTREAM" "wlroots/CMakeLists.txt" || true)
    if [[ -n "${dirty}" ]]; then
        die "working tree has local changes under ${PREFIX_REL} / wlroots metadata.
Commit or stash them first (local patches must be commits for subtree pull),
or pass --force.
${dirty}"
    fi
fi

# Full fetch — never --depth=1 (would destroy blame/history)
log "fetch (full history) ${SOURCE_URL} ${NEW_REF}"
git fetch --no-tags "${SOURCE_URL}" "${NEW_REF}"
NEW_COMMIT=$(git rev-parse --verify 'FETCH_HEAD^{commit}')

if [[ "${NEW_COMMIT}" == "${BASE_COMMIT}" ]]; then
    log "already at ${NEW_COMMIT} (${NEW_REF}); nothing to do"
    exit 0
fi

log "base   ${BASE_COMMIT}"
log "theirs ${NEW_COMMIT} (${NEW_REF})"

read_version_from_commit() {
    local commit=$1
    local ver
    ver=$(git show "${commit}:meson.build" \
        | sed -n "s/.*version: *'\\([^']*\\)'.*/\\1/p" | head -n1)
    [[ -n "${ver}" ]] || die "could not parse version from ${commit}:meson.build"
    printf '%s\n' "${ver}"
}

NEW_VERSION=$(read_version_from_commit "${NEW_COMMIT}")
log "upstream version string: ${NEW_VERSION}"

if [[ "${DRY_RUN}" -eq 1 ]]; then
    log "dry-run: would run:"
    printf '    git subtree pull --prefix=%s %s %s\n' \
        "${PREFIX_REL}" "${SOURCE_URL}" "${NEW_REF}"
    log "dry-run: would set UPSTREAM COMMIT=${NEW_COMMIT} REF=${NEW_REF} VERSION=${NEW_VERSION}"
    log "dry-run: would bump wlroots/CMakeLists.txt version macros"
    git diff --stat "${BASE_COMMIT}" "${NEW_COMMIT}" | tail -n 20 || true
    exit 0
fi

# Prefer merge message that records the pin for humans.
MERGE_MSG="Merge wlroots ${NEW_REF} (${NEW_COMMIT}) into ${PREFIX_REL}

Update vendored wlroots via git subtree (non-squash) so blame/history
are preserved. Local commits under ${PREFIX_REL} are kept by the merge.

Log: 更新 vendored wlroots 至 ${NEW_REF}
"

log "git subtree pull --prefix=${PREFIX_REL} (non-squash)"
# `git subtree pull` = fetch + subtree merge. We already fetched; use merge
# with the resolved commit to avoid a second fetch and to pin the exact SHA.
if ! git subtree merge --prefix="${PREFIX_REL}" -m "${MERGE_MSG}" "${NEW_COMMIT}"; then
    die "subtree merge reported conflicts under ${PREFIX_REL}.
Resolve them, then:
  1. git add ${PREFIX_REL}
  2. git commit  # finish the merge
  3. Update wlroots/UPSTREAM:
       REF=${NEW_REF}
       COMMIT=${NEW_COMMIT}
       VERSION=${NEW_VERSION}
  4. Bump WLROOTS_VERSION* in wlroots/CMakeLists.txt
  5. Review wlroots/cmake/* against new meson source lists"
fi

# Refresh pin metadata
cat > "${UPSTREAM_FILE}" <<EOF
# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

# Vendored wlroots upstream pin (machine-readable + human-readable).
# Updated by: wlroots/update-from-upstream.sh
#
# History model: NON-SQUASH git subtree under 3rdparty/wlroots so
# \`git blame 3rdparty/wlroots/<file>\` shows original upstream authors.
# Treeland-local patches are normal commits on top of the subtree.
#
# SOURCE_URL  - git remote to fetch from
# REF         - last requested ref (tag, branch, or commit-ish)
# COMMIT      - full SHA of pure upstream tip currently merged
# VERSION     - project version string from upstream meson.build

SOURCE_URL=${SOURCE_URL}
REF=${NEW_REF}
COMMIT=${NEW_COMMIT}
VERSION=${NEW_VERSION}
EOF

if [[ -f "${CMAKE_FILE}" ]]; then
    major=${NEW_VERSION%%.*}
    rest=${NEW_VERSION#*.}
    minor=${rest%%.*}
    patch=${rest#*.}
    if [[ "${patch}" == "${rest}" ]]; then
        patch=0
    else
        patch=${patch%%-*}
    fi
    if ! [[ "${patch}" =~ ^[0-9]+$ ]]; then
        patch=0
    fi
    # Only rewrite top-level numeric assignments (not CACHE INTERNAL lines).
    sed -i -E \
        -e "s/^set\\(WLROOTS_VERSION [0-9][^)]*\\)/set(WLROOTS_VERSION ${NEW_VERSION})/" \
        -e "s/^set\\(WLROOTS_VERSION_MAJOR [0-9]+\\)/set(WLROOTS_VERSION_MAJOR ${major})/" \
        -e "s/^set\\(WLROOTS_VERSION_MINOR [0-9]+\\)/set(WLROOTS_VERSION_MINOR ${minor})/" \
        -e "s/^set\\(WLROOTS_VERSION_PATCH [0-9]+\\)/set(WLROOTS_VERSION_PATCH ${patch})/" \
        -e "s/^# CMake build for vendored wlroots .*/# CMake build for vendored wlroots ${NEW_VERSION}./" \
        "${CMAKE_FILE}"
fi

log "stage metadata"
git add -- "wlroots/UPSTREAM" "wlroots/CMakeLists.txt"
if ! git diff --cached --quiet -- "wlroots/UPSTREAM" "wlroots/CMakeLists.txt"; then
    git commit -m "$(cat <<EOF
chore(wlroots): pin UPSTREAM to ${NEW_REF} (${NEW_VERSION})

Record pure upstream tip ${NEW_COMMIT} after subtree merge and bump
CMake version macros.

Log: 更新 wlroots UPSTREAM pin 至 ${NEW_REF}
Influence: 仅元数据/版本号；源码已由 subtree merge 提交。
EOF
)"
fi

log "done"
printf '\nSubtree-merged wlroots %s (%s) into %s.\n' \
    "${NEW_REF}" "${NEW_COMMIT}" "${PREFIX_REL}"
printf 'Blame check:\n'
printf '  git blame -L 1,5 -- %s/meson.build\n' "${PREFIX_REL}"
printf 'Next:\n'
printf '  - Review wlroots/cmake/* if meson source lists changed\n'
printf '  - cmake --preset default && cmake --build --preset default --target waylib-wlroots\n'
