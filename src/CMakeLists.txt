add_library(zigbee_driver STATIC
    ZNPFrame.cpp
    SerialPort.cpp
    ZigbeeDevice.cpp
    ZigbeeDongle.cpp
)

target_include_directories(zigbee_driver PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(zigbee_driver PUBLIC asio_interface)

if(MSVC)
    target_compile_options(zigbee_driver PRIVATE /W4 /utf-8)
else()
    target_compile_options(zigbee_driver PRIVATE -Wall -Wextra -Wpedantic)
endif()
