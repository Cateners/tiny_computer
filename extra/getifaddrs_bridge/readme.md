### 编译

使用NDK编译getifaddrs_bridge_server.c:

`aarch64-linux-android-clang getifaddrs_bridge_server.c -o getifaddrs_bridge_server`

在小小电脑上编译getifaddrs_bridge_client_lib.c:

`gcc getifaddrs_bridge_client_lib.c -o getifaddrs_bridge_client_lib.so -shared`

### 使用

在安卓端:

`getifaddrs_bridge_server /path/to/container/tmp/.getifaddrs-bridge`

在proot容器：

`LD_PRELOAD=/path/to/getifaddrs_bridge_client_lib.so <your_program>`