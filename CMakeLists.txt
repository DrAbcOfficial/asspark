cmake_minimum_required(VERSION 3.20)
project(asspark LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler selection (Linux only)
if(UNIX AND NOT APPLE)
    option(USE_CLANG "Use Clang compiler instead of GCC" OFF)
    if(USE_CLANG)
        find_program(CLANG_C_COMPILER clang)
        find_program(CLANG_CXX_COMPILER clang++)
        if(CLANG_C_COMPILER AND CLANG_CXX_COMPILER)
            set(CMAKE_C_COMPILER ${CLANG_C_COMPILER})
            set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER})
            message(STATUS "Using Clang compiler: ${CLANG_CXX_COMPILER}")
        else()
            message(FATAL_ERROR "Clang compiler not found. Install clang or disable USE_CLANG option.")
        endif()
    endif()
endif()

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_compile_definitions(PLATFORM_WINDOWS WIN32 _WINDOWS _USRDLL)
    if(MSVC)
        add_compile_options(/W3)
    endif()
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
    add_compile_definitions(PLATFORM_LINUX LINUX)
    
    # Linux: force 32-bit (i386/arm)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        # aarch64编译器不支持-m32或-marm，需要安装armhf交叉编译器
        # 或者使用aarch64原生编译（如果目标平台是aarch64）
        message(WARNING "aarch64 detected. For 32-bit ARM build, install arm-linux-gnueabihf-gcc and set CMAKE_C_COMPILER/CMAKE_CXX_COMPILER")
    endif()
    
    # Linux: use Ninja backend
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "")
endif()

# Configuration-specific definitions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
    if(MSVC)
        add_compile_options(/Od /RTC1)
    else()
        add_compile_options(-O0 -g)
    endif()
else()
    add_compile_definitions(NDEBUG)
    if(MSVC)
        add_compile_options(/O2 /GL)
        add_link_options(/LTCG)
    else()
        add_compile_options(-O2)
    endif()
endif()

# Common definitions
add_compile_definitions(FALLGUYS_EXPORTS _CRT_SECURE_NO_WARNINGS NOMINMAX)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/common
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/dlls
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/pm_shared
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/metamod
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/thirdparty/angelscript-sdk/angelscript/include
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/asext/include
)

# Source files
set(SOURCES
    dllapi.cpp
    h_export.cpp
    meta_api.cpp
    sdk_util.cpp
)

# Windows-specific source
if(WIN32)
    list(APPEND SOURCES dllmain.cpp)
endif()

# Headers
set(HEADERS
    dlldef.h
    signatures.h
)

# Create shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

# Windows-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/addons/metamod/dlls
    )
    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()
endif()

# Linux-specific settings
if(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/addons/metamod/dlls
        PREFIX ""  # Remove 'lib' prefix for Linux shared libraries
        SUFFIX ".so"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

# Link libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 oleaut32 uuid odbc32 odbccp32)
endif()

# Post-build: copy to output directory (Windows only, Linux uses output directory directly)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${PROJECT_NAME}>
        ${CMAKE_CURRENT_SOURCE_DIR}/build/addons/metamod/dlls/
    )
endif()

# Print configuration info
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
