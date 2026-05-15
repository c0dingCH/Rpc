cmake_minimum_required(VERSION 3.10)
project(Rpc)

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS "-g -Wall -Werror -std=c++17 -DTHREADED")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

include_directories(
    base
    tcp
    tcp/include
    rpc
    rpc/include
    rpc/pbs
    util/buffer
    util/timer
    util/log
    ${Protobuf_INCLUDE_DIRS}
    /usr/include/zookeeper
)

file(GLOB SRC_BASE base/*.cpp)
file(GLOB SRC_TCP tcp/*.cpp)
file(GLOB SRC_RPC rpc/*.cpp rpc/*.cc rpc/pbs/*.cc)
file(GLOB SRC_BUFFER util/buffer/*.cpp)
file(GLOB SRC_TIMER util/timer/*.cpp)
file(GLOB SRC_LOG util/log/*.cpp)
set(SRC_ALL ${SRC_BASE} ${SRC_TCP} ${SRC_RPC} ${SRC_BUFFER} ${SRC_TIMER} ${SRC_LOG})

add_library(chat SHARED ${SRC_ALL})
target_link_libraries(chat ${CMAKE_THREAD_LIBS_INIT} ${Protobuf_LIBRARIES} zookeeper_mt)



# 设置可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/test/)

# 自动收集 test 目录下所有 cpp 文件
file(GLOB TESTS test/*.cpp)

# 遍历每个测试文件，生成对应的可执行文件
foreach(test_file ${TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)  # 去掉路径和扩展名
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} chat)    
endforeach()

