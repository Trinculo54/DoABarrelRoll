cmake_minimum_required(VERSION 3.20)
project(doabarrelroll VERSION 1.0.0 LANGUAGES C CXX RC)

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
    message(FATAL_ERROR "Build with llvm-mingw (clang): the mod uses clang's __try/__except.")
else()
    add_compile_options(-Wall -Wno-missing-field-initializers -fms-extensions)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# The FH2 Mod Platform (git submodule): fh2mod.h, and the platform DLLs the zip ships.
add_subdirectory(platform)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)

# The mod: fh2-mods\DoABarrelRoll.dll
add_library(doabarrelroll SHARED src/doabarrelroll.cpp src/version.rc)
target_include_directories(doabarrelroll PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(doabarrelroll PRIVATE fh2mod_api)
set_target_properties(doabarrelroll PROPERTIES OUTPUT_NAME DoABarrelRoll PREFIX "")
# The version resource reads the generated header: rebuild it when the version changes.
set_source_files_properties(src/version.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/generated/fh2_version.h)

# The flight model's self-check: cmake -DDOABARRELROLL_TESTS=ON, then build\flight_test.exe
option(DOABARRELROLL_TESTS "Build the flight model's test" OFF)
if(DOABARRELROLL_TESTS)
    add_executable(flight_test tests/flight_test.cpp)
    target_include_directories(flight_test PRIVATE src)
endif()
