cmake_minimum_required(VERSION 3.20)
project(flyingcar VERSION 0.2.1 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Targets Windows. On Linux use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    message(FATAL_ERROR "Build with llvm-mingw (clang).")
else()
    add_compile_options(-Wall -Wno-missing-field-initializers -fms-extensions)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# The FH2 Mod Platform (git submodule): fh2mod.h.
add_subdirectory(platform)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)

# The mod: fh2-mods\FlyingCar.dll
add_library(flyingcar SHARED src/flyingcar.cpp src/version.rc)
target_include_directories(flyingcar PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(flyingcar PRIVATE fh2mod_api)
set_target_properties(flyingcar PROPERTIES OUTPUT_NAME FlyingCar PREFIX "")

# The flight model's self-check: cmake -DFLYINGCAR_TESTS=ON, then build\flight_test.exe
option(FLYINGCAR_TESTS "Build the flight model's test" OFF)
if(FLYINGCAR_TESTS)
    add_executable(flight_test tests/flight_test.cpp)
    target_include_directories(flight_test PRIVATE src)
endif()
