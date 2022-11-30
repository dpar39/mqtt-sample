#!/usr/bin/env bash
set -e

REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

_msg_() { echo -e "\033[0;33m$*\033[0m"; }
_end_() { echo -e "\033[0;31mCommand '$*' Failed!\033[0m";  popd > /dev/null; exit 1; }
_exe_() { _msg_ $*; $* || _end_ $*; }

pushd $REPO_ROOT >/dev/null

# Build the executable
_exe_ ./build.sh release

# Run integration tests
_exe_ python test/integration_tests.py --verbose

popd >/dev/null
_msg_ "---------------------- DONE ----------------------"

