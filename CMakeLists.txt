cmake_minimum_required(VERSION 3.10)
project(SonoffZigbeeDriver VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/zstack_protocol.cpp
    src/serial_communication.cpp
    src/zigbee_dongle.cpp
    src/device_manager.cpp
)

# Create library
add_library(sonoff_zigbee_lib SHARED ${SOURCES})
target_link_libraries(sonoff_zigbee_lib PRIVATE Threads::Threads)

# Create executable example
add_executable(zigbee_example src/main.cpp)
target_link_libraries(zigbee_example PRIVATE sonoff_zigbee_lib Threads::Threads)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS sonoff_zigbee_lib DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)