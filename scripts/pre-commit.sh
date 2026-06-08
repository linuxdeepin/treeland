#!/usr/bin/env bash
# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#
# Pre-commit hook: check staged C/C++ files with git-clang-format
# Only checks the staged diff rather than the entire file for efficiency.
#
# Installation:
#   1. Manual: ln -sf ../../scripts/pre-commit.sh .git/hooks/pre-commit
#   2. (Legacy) CMake configure — the script is retained as a local
#      convenience; enforcement is handled by the clang-format CI
#      workflow on every push / pull request.

set -e

cd "$(git rev-parse --show-toplevel)"

# Verify git-clang-format is available
if ! command -v git-clang-format &> /dev/null; then
    echo "Warning: git-clang-format not found — format check skipped."
    echo "  Formatting is still enforced by CI on every push / pull request."
    exit 0
fi

# Verify .clang-format configuration exists
if [[ ! -f .clang-format ]]; then
    echo "ERROR: .clang-format configuration file not found."
    exit 1
fi

# Capture exit code so real tool failures are not masked.
rc=0
output=$(git clang-format --staged --extensions 'c,h,cpp,cc,cxx' --diff 2>&1) || rc=$?

if [[ $rc -ne 0 ]]; then
    echo "ERROR: git-clang-format failed (exit code ${rc})."
    echo ""
    echo "${output}"
    exit 1
fi

# Determine whether formatting is needed
if [[ "$output" == *"no modified files to format"* ]]; then exit 0; fi
if [[ "$output" == *"clang-format did not modify any files"* ]]; then exit 0; fi
if [[ -z "$output" ]]; then exit 0; fi

echo "ERROR: Unformatted code detected. Please format first:"
echo ""
echo "  git clang-format --staged --extensions 'c,h,cpp,cc,cxx'         # format staged changes"
echo "  git clang-format --staged --extensions 'c,h,cpp,cc,cxx' --diff  # preview format diff"
echo ""
echo "Skip check (not recommended): git commit --no-verify"
exit 1
