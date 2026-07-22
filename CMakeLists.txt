cmake_minimum_required(VERSION 3.20)

set(CMAKE_BUILD_TYPE Release)

set(APP_TARGET icecoreframework)

project(${APP_TARGET})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS "-Wall -O3 -s -fno-ident -flto -ffunction-sections -fdata-sections -Wl,--gc-sections")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)

add_subdirectory(raylib-6.0)

add_executable(${APP_TARGET}
    main.cpp
    src/drawing/icfw_drawing.hpp
    src/math/vector.hpp
)

find_program(UPX_EXECUTABLE upx)

if(UPX_EXECUTABLE)
    add_custom_command(
        TARGET ${APP_TARGET}
        POST_BUILD
        COMMAND 
            ${UPX_EXECUTABLE} --best --lzma $<TARGET_FILE:${APP_TARGET}>
        COMMENT "Compressing executable with UPX"
    )
endif()

if(ipo_supported)
    set_property(TARGET ${APP_TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_link_libraries(${APP_TARGET} PRIVATE raylib)