#!/usr/bin/env sh

script_path=$(realpath $(dirname "$0"))
cmd="$script_path/treeland $@"
output=$($cmd --try-exec 2>&1)
exit_code=$?

if [ $exit_code -ne 0 ] && echo "$output" | grep -q "failed to create dri2 screen"; then
    # In VirtualBox, if 3D acceleration is not enabled, calling `eglInitialize` will cause
    # the program to crash. Therefore, a preliminary check is performed here: if `treeland`
    # fails to exit normally, it falls back to pixman renderer mode.
    export WLR_RENDERER=pixman
    echo "Treeland crash, try fallback to software renderer."
fi

$cmd
