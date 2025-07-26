cmake_minimum_required(VERSION 3.31)
project(lock_free_event_bus)

set(CMAKE_CXX_STANDARD 17)

add_executable(lock_free_event_bus main.cpp
        includes/event.hpp
        includes/lock_free_spsc_queue.hpp
        includes/event_bus.hpp
        includes/consumer.hpp
        includes/topic.hpp
        includes/consumer_group.hpp
)
