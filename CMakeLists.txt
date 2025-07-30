cmake_minimum_required(VERSION 3.14)
project(lock_free_event_bus)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SRCS src/*.cpp)

add_library(eventbus_lib ${SRCS}
        src/consumer.cpp)

target_include_directories(eventbus_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/includes
)

add_executable(lock_free_event_bus main.cpp)

target_link_libraries(lock_free_event_bus PRIVATE eventbus_lib)
