cmake_minimum_required(VERSION 3.21)

set(APP_TARGET icecoreframework)

project(${APP_TARGET})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)

add_subdirectory(raylib-6.0)

add_executable(${APP_TARGET}
    main.cpp
    src/drawing/icfw_drawing.hpp
    src/math/vector.hpp
)

target_compile_options(icecoreframework PRIVATE
    -Wall
)

target_compile_options(icecoreframework PRIVATE
    $<$<CONFIG:Release>:-O3 -fno-ident -flto -ffunction-sections -fdata-sections>
)

target_link_options(icecoreframework PRIVATE
    $<$<CONFIG:Release>:-s -Wl,--gc-sections>
)

find_program(UPX_EXECUTABLE upx)

if(UPX_EXECUTABLE AND WIN32)
    add_custom_command(
        TARGET ${APP_TARGET}
        POST_BUILD
        COMMAND 
            ${UPX_EXECUTABLE} --best --lzma $<TARGET_FILE:${APP_TARGET}>
        COMMENT "Compressing executable with UPX"
    )
endif()

if(EMSCRIPTEN)
    set_target_properties(${APP_TARGET} PROPERTIES SUFFIX ".html")
endif()

if(ipo_supported)
    set_property(TARGET ${APP_TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_link_libraries(${APP_TARGET} PRIVATE raylib)