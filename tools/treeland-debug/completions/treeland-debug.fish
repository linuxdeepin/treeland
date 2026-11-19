# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#
# Fish completion for treeland-debug. Installed as
# $DATADIR/fish/vendor_completions.d/treeland-debug.fish.
#
# The word lists mirror debugCommandNames()/completionCandidates() in
# tools/treeland-debug/debughelpers.cpp; testCompletionScriptsMatchCommandTable()
# compares them and fails when one side was changed without the other.

complete -c treeland-debug -n '__fish_use_subcommand' -a tree -d 'Print the complete window tree'
complete -c treeland-debug -n '__fish_use_subcommand' -a cursor -d 'Print the cursor position'
complete -c treeland-debug -n '__fish_use_subcommand' -a windows -d 'List all toplevel windows'
complete -c treeland-debug -n '__fish_use_subcommand' -a window -d 'Print one window in detail'
complete -c treeland-debug -n '__fish_use_subcommand' -a clients -d 'List connected clients'
complete -c treeland-debug -n '__fish_use_subcommand' -a idle -d 'Print the idle state'
complete -c treeland-debug -n '__fish_use_subcommand' -a shortcuts -d 'List registered shortcuts'
complete -c treeland-debug -n '__fish_use_subcommand' -a scene -d 'Dump the QtQuick scene tree'
complete -c treeland-debug -n '__fish_use_subcommand' -a activate -d 'Activate a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a close -d 'Close a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a minimize -d 'Minimize a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a maximize -d 'Maximize a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a fullscreen -d 'Toggle fullscreen on a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a move -d 'Move a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a resize -d 'Resize a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a workspace -d 'Move a window to a workspace'
complete -c treeland-debug -n '__fish_use_subcommand' -a move-cursor -d 'Move the cursor'
complete -c treeland-debug -n '__fish_use_subcommand' -a event -d 'Inject an input event'
complete -c treeland-debug -n '__fish_use_subcommand' -a screenshot -d 'Capture an output or a window'
complete -c treeland-debug -n '__fish_use_subcommand' -a top -d 'Live window table'
complete -c treeland-debug -n '__fish_use_subcommand' -a events -d 'Live input event stream'
complete -c treeland-debug -n '__fish_use_subcommand' -a watch -d 'Watch one window'
complete -c treeland-debug -n '__fish_use_subcommand' -a no-auth -d 'Open the debug socket to every user for 30 minutes'
complete -c treeland-debug -n '__fish_use_subcommand' -a listen -d 'Expose the CLI over HTTP and WebSocket'
complete -c treeland-debug -n '__fish_use_subcommand' -a debug -d 'Read or write the remoteDebug switch'
complete -c treeland-debug -n '__fish_use_subcommand' -a help -d 'Show the command reference'
complete -c treeland-debug -n '__fish_use_subcommand' -a shell -d 'Start the interactive REPL'

# The fixed first argument of the commands that have one.
complete -c treeland-debug -n '__fish_seen_subcommand_from event' -a 'motion button key'
complete -c treeland-debug -n '__fish_seen_subcommand_from screenshot' -a 'output window'
complete -c treeland-debug -n '__fish_seen_subcommand_from debug' -a 'on off status'
complete -c treeland-debug -n '__fish_seen_subcommand_from listen' -l port -r -d 'Port to bind'
complete -c treeland-debug -n '__fish_seen_subcommand_from listen' -l host -r -d 'Address to bind'
