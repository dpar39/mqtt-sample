#!/usr/bin/env bash
if [ ! -f /.dockerenv ]; then

    REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

    echo -e '\033[0;33mBuilding docker container ...\033[0m'
    docker build -t ci-builder --target builder -f Dockerfile.all \
       --build-arg "USER_NAME=$USER" \
       --build-arg "USER_UID=$(id -u)" \
       --build-arg "USER_GID=$(id -g)" \
       "$REPO_ROOT"
       
    echo -e '\033[0;33mRunning in docker container ...\033[0m'
    docker run \
        -v "$REPO_ROOT:/workspace" \
        -w /workspace ci-builder $*
    exit
fi

$*