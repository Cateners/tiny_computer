
#
# wolfssl crypt test
#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")

set(COMPONENT_SRCS "main.c")

# when using time helper:
# set(COMPONENT_SRCS "main.c" "time_helper.c")

set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
