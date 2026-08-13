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
    src/icpak.hpp
    src/icpaker.hpp
    src/icpak_openssl.hpp
)

target_compile_options(${APP_TARGET} PRIVATE
    -Wall
)

target_compile_options(${APP_TARGET} PRIVATE
    $<$<CONFIG:Release>:-O3 -fno-ident -flto -ffunction-sections -fdata-sections>
    $<$<CONFIG:Debug>:-O0 -g>
)

target_link_options(${APP_TARGET} PRIVATE
    $<$<CONFIG:Debug>:-Wl,--gc-sections>
)

find_program(UPX_EXECUTABLE upx)


if(UPX_EXECUTABLE AND WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(
        TARGET ${APP_TARGET}
        POST_BUILD
        COMMAND 
            ${UPX_EXECUTABLE} --best --lzma $<TARGET_FILE:${APP_TARGET}>
        COMMENT "Compressing executable with UPX"
    )
endif()

if(ipo_supported)
    set_property(TARGET ${APP_TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION $<CONFIG:Release>)
endif()

target_link_libraries(${APP_TARGET} PRIVATE OpenSSL::Crypto)