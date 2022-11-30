# MQTT Sample publisher and consumer C++ app

This repo builds a sample MQTT client application and dockerize it.
The application connects to an MQTT broker and sends messages to a specified topic on an timer, it also subscribes to another topic and prints out received messages.

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

- To build the app, run `./build.sh`. By default the app is built in `release` mode. If debug is desired, run `./build.sh debug`. Output binary will be available at `build_debug/mqtt-client-app` or `build_release/mqtt-sample` depending on the configuration.

- To run the application, you need to specify the MQTT connection parameters and topic name via environment variables. Say you have a broker running on `localhost:8883`:

```bash
export IO_HOST="localhost",
export IO_PORT="8883",
export IO_PUBLISH_TOPIC="publish_topic",
export IO_CONSUME_TOPIC="consume_topic",
export IO_MESSAGE_COUNT="3",
export IO_MESSAGE_PERIOD_SECONDS="1.5",
export IO_CAFILE="/workspace/certs/ca.crt",
export IO_CERTFILE="/workspace/certs/server.crt",
export IO_KEYFILE="/workspace/certs/server.key",
build_debug/mqtt-client-app

# Sample output
on_connect: Connection Accepted.
on_subscribe: 0:granted qos = 1
Publishing message: 77
Publishing message: 93
Publishing message: 26
```

### Debugging

On VSCode, edit `.vscode/launch.json` and configure environment variables accordingly. Then set up your break points and hit `F5`.

### Code formatting

The script `./format-all.sh` will format C and C++ code.

### Dockerizing the app

The binary generated is linked statically to `mosquitto` library, so any Linux image based on on modern `glibc`, `libssl` and `libcrypto` can run it.

```bash
$ ldd build_release/mqtt-client-app 
        linux-vdso.so.1 (0x00007ffd813a9000)
        libssl.so.3 => /lib/x86_64-linux-gnu/libssl.so.3 (0x00007f0262363000)
        libcrypto.so.3 => /lib/x86_64-linux-gnu/libcrypto.so.3 (0x00007f0261f21000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f0261cf9000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f0262524000)
```
