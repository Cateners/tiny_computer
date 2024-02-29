
#
# wolfssl benchmark test
#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")

set(COMPONENT_SRCS main.c)
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
