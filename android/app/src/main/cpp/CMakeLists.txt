cmake_minimum_required(VERSION 3.18.1)
project("native-socket")

# 添加库
add_library(native-socket SHARED native-socket.cpp)

# 链接日志库
find_library(log-lib log)

# 指定目标属性
target_link_libraries(native-socket ${log-lib})