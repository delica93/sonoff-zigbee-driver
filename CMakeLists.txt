cmake_minimum_required(VERSION 3.28)
project(ZigbeeDongleDriver VERSION 0.1.0 LANGUAGES CXX)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ───────────────────────────────────────────────────────────────
include(FetchContent)

# Standalone ASIO (header-only, no Boost)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio_interface INTERFACE)
target_include_directories(asio_interface INTERFACE
    "${asio_SOURCE_DIR}/asio/include"
)
target_compile_definitions(asio_interface INTERFACE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
    _WIN32_WINNT=0x0A00   # Windows 10+ (ignored on non-Windows)
)
if(WIN32)
    target_link_libraries(asio_interface INTERFACE ws2_32 wsock32 mswsock)
endif()

# ── Subdirectories ─────────────────────────────────────────────────────────────
add_subdirectory(src)
add_subdirectory(examples)
