### Compilation

Compile getifaddrs_bridge_server.c using NDK:

`aarch64-linux-android-clang getifaddrs_bridge_server.c -o getifaddrs_bridge_server`

Compile getifaddrs_bridge_client_lib.c on Tiny Computer:

`gcc getifaddrs_bridge_client_lib.c -o getifaddrs_bridge_client_lib.so -shared`

### Usage

On the Android side:

`getifaddrs_bridge_server /path/to/container/tmp/.getifaddrs-bridge`

In the proot container:

`LD_PRELOAD=/path/to/getifaddrs_bridge_client_lib.so <your_program>`
