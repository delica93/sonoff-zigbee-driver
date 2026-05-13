add_executable(zigbee_example main.cpp)

target_link_libraries(zigbee_example PRIVATE zigbee_driver)

if(MSVC)
    target_compile_options(zigbee_example PRIVATE /W4 /utf-8)
else()
    target_compile_options(zigbee_example PRIVATE -Wall -Wextra -Wpedantic)
endif()
