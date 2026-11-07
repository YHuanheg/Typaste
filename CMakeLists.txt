# CMakeLists.txt --- CMake project settings
#    x64 (default):   cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release
#    Win32 (legacy):  cmake -S . -B build-x86 -A Win32 -DCMAKE_BUILD_TYPE=Release
#    MinGW:           cmake -S . -B build-mingw -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
##############################################################################

cmake_minimum_required(VERSION 3.15)

# Select the MSVC runtime library the modern way (CMP0091)
cmake_policy(SET CMP0091 NEW)

# The x64 build is the default one. Visual Studio generators need the platform
# to be chosen before project() is called, so this block cannot test MSVC here
# (the compiler is not detected yet).
if(NOT CMAKE_GENERATOR_PLATFORM AND CMAKE_GENERATOR MATCHES "^Visual Studio")
    set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "Target platform: x64, Win32 or ARM64")
endif()

project(Typaste CXX RC)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" uppercase_CMAKE_BUILD_TYPE)
if(NOT uppercase_CMAKE_BUILD_TYPE MATCHES "^(DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
    message(FATAL_ERROR "Invalid value for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_C_FLAGS "-static -s")
    set(CMAKE_CXX_FLAGS "-static -s")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_C_FLAGS "-static -static-libgcc -static-libstdc++ -s")
    set(CMAKE_CXX_FLAGS "-static -static-libgcc -static-libstdc++ -s")
elseif(MSVC)
    # Build without runtime DLLs (static CRT), for both x86 and x64
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

##############################################################################

# Typaste.exe
add_executable(Typaste WIN32
    Typaste.cpp AutoType.cpp Lang.cpp
    Settings.cpp Config.cpp Clipboard.cpp Dpi.cpp Util.cpp
    Typaste_res.rc)
set_target_properties(Typaste PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(Typaste PRIVATE UNICODE _UNICODE) # Unicode support
target_link_libraries(Typaste comctl32 shell32 shlwapi winmm imm32 advapi32)

if(MSVC)
    # /utf-8 keeps the sources and the resource files encoding-safe on any locale
    target_compile_options(Typaste PRIVATE /utf-8 /W4 /permissive- /Zc:__cplusplus)
    # Fixed-up, ASLR, DEP, and the high-entropy 64-bit address space
    target_link_options(Typaste PRIVATE /MANIFEST:NO /DYNAMICBASE /NXCOMPAT)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_options(Typaste PRIVATE /HIGHENTROPYVA)
    endif()
endif()

##############################################################################

# The localized resource checker (also used by CI). It must come after
# project(), otherwise CMake has no compiler information yet.
add_executable(verify_lang tools/verify_lang.cpp)
target_compile_definitions(verify_lang PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(verify_lang PRIVATE /utf-8)
endif()

##############################################################################
