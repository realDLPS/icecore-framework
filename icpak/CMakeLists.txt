cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(READ "${CMAKE_SOURCE_DIR}/icpak.ini" ICPAK_INI)
set(APP_TARGET "ICPAK")

project(${APP_TARGET})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
find_package(OpenSSL REQUIRED)

add_executable(${APP_TARGET}
    main.cpp
    src/icpak_io.hpp
    src/icpak_openssl.hpp
    src/icpak_reader.hpp
    src/icpak_types.hpp
    src/icpak.hpp
    src/icpaker.hpp
)

target_compile_definitions(${APP_TARGET} PRIVATE
    $<$<CONFIG:Debug>:BUILD_DEBUG>
    $<$<CONFIG:Release>:BUILD_RELEASE>
    $<$<CONFIG:Test>:BUILD_TEST>
)

target_compile_options(${APP_TARGET} PRIVATE
    -Wall
)

target_compile_options(${APP_TARGET} PRIVATE
    $<$<CONFIG:Release>:-O3 -fno-ident -flto -ffunction-sections -fdata-sections>
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Test>:-O0 -g>
)

target_link_options(${APP_TARGET} PRIVATE
    $<$<CONFIG:Debug>:-Wl,--gc-sections>
)

target_link_libraries(${APP_TARGET} PRIVATE OpenSSL::Crypto)