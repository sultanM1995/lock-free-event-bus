cmake_minimum_required(VERSION 3.14)
project(lock_free_event_bus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(eventbus_core INTERFACE)
target_include_directories(eventbus_core
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/core
)

add_library(eventbus_lib
        lib/eventbus/src/consumer.cpp
        lib/eventbus/src/consumer_group.cpp
)

target_include_directories(eventbus_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/eventbus
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/core
)

target_link_libraries(eventbus_lib PUBLIC eventbus_core)

add_executable(basic_usage_demo examples/basic_usage_demo.cpp)
target_link_libraries(basic_usage_demo PRIVATE eventbus_lib)

add_executable(partition_scaling_demo examples/partition_scaling_demo.cpp)
target_link_libraries(partition_scaling_demo PRIVATE eventbus_lib)