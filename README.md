# PAHO MQTT Sample publisher app

This repo builds a sample MQTT client application and dockerize it.
The application connects to an MQTT broker and sends messages to it on an interval.

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

- To build the app, run `./build.sh`. By default the app is built in `release` mode. If debug is desired, run `./build.sh debug`. Output binary will be available at `build_debug/mqtt-sample` or `build_release/mqtt-sample` depending on the configuration.

- To run the application, you need to specify the MQTT connection parameters and topic name. It can be done via CLI or environment variables. Here's an example:

```bash
build_debug/mqtt-sample -h io.adafruit.com -p 1883 -u dpar39 -P <MY_IO_KEY> -t dpar39/feeds/feed-one -m "hello mqtt" --message-interval 2.0 --message-count 3 --verbose

# Sample output
Connecting...
Connected!
Publishing data of length 13
Sleeping for 1999881 us ...
Publishing data of length 13
Sleeping for 1999472 us ...
Publishing data of length 13
Disconnecting...
Disconnected!
```

### Debugging

On VSCode, edit `.vscode/launch.json` and add the MQTT broker password. Then set up your break points and hit `F5`.

### Code formatting

The script `./format-all.sh` will format C and C++ code.

### Dockerizing the app

The binary generated is linked statically to `mqtt.paho.c`, so any Linux image based on on modern `glibc`, `libssl` and `libcrypto` can run it.

```bash
$ ldd build_release/mqtt-sample 
        linux-vdso.so.1 (0x00007ffda24db000)
        libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f9662702000)
        libssl.so.3 => /lib/x86_64-linux-gnu/libssl.so.3 (0x00007f966265e000)
        libcrypto.so.3 => /lib/x86_64-linux-gnu/libcrypto.so.3 (0x00007f966221c000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9661ff4000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f96628c1000)
```
