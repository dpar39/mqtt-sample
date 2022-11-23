#!/bin/bash
set -e

REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
_end_() { echo -e "\033[0;31mCommand '$*' Failed!\033[0m"; popd > /dev/null;  exit 1; }
_echo_() { echo -e "\033[0;33m$*\033[0m"; $* || _end_ $*; }

BUILD_CONFIG=${1:-release}
RUN_TESTS=${2:-false}

BUILD_DIR="$REPO_ROOT/build_${BUILD_CONFIG}"

_echo_ cmake -G Ninja -B$BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_CONFIG $REPO_ROOT
_echo_ cmake --build $BUILD_DIR --target all --

[[ $RUN_TESTS = true ]] && _echo_ ctest --test-dir $BUILD_DIR --output-junit $BUILD_DIR/results.xml

exit 0