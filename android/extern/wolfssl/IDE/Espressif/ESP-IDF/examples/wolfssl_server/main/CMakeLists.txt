
#
# wolfssl server test
#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")

set(COMPONENT_SRCS "server-tls.c" "wifi_connect.c")
set(COMPONENT_ADD_INCLUDEDIRS "." "./include")

register_component()
