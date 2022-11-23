# PAHO MQTT Starter App

This repo builds a sample MQTT client application and dockerize it.

## Getting started for C/C++ development with VSCode

### Prerequisites

- A POSIX system running `docker` (i.e. WSL2, Linux or Mac)
- VSCode with [Remote Development Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.vscode-remote-extensionpack) installed.
- Environment variables `USER_UID` and `USER_GID` matching the result of `id -u` and `id -g` so that files can be written with the right permissions from within the container. Simplest way is to add these two lines at the end of your shell resource file (e.g. `~/.bashrc`) and source it:

```bash
export USER_UID=$(id -u)
export USER_GID=$(id -g)
```

### Getting started

- Clone this repository and open the folder in VSCode. Then click `View -> Command Palette...` select `Dev Containers: Open Folder in Container...`. This will create a Dev Container based on Ubuntu 22.04 ready for C/C++ development with MQTT client library built statically.

### Debugging


