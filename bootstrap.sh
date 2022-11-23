#!/usr/bin/env bash

set -e

_msg_() { echo -e "\033[0;33m$*\033[0m"; }
_error_() { echo -e "\033[0;31m$*\033[0m" ; exit 1 ; }

[ -f /.dockerenv ] && _error_ 'This script is to be run from outside a docker container. See README.md.'
command -v code &> /dev/null || _error_ 'VSCode executable `code` was not found in PATH. Install it and rerun.' 
command -v docker &> /dev/null || _error_ 'docker is not installed. Install it and rerun.'

if [[ $(code --list-extensions | grep 'ms-vscode-remote.vscode-remote-extensionpack') ]]; then
    :
else
    _msg_ "Installing VSCode Remote Development extension pack ... "
    code --install-extension 'ms-vscode-remote.vscode-remote-extensionpack'
fi

export USER_NAME=$USER
export USER_UID=$(id -u)
export USER_GID=$(id -g)
export REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

code "$REPO_ROOT"