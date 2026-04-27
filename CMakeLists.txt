cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(asspark LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# if(WIN32)
#     add_compile_definitions(WIN32 _WINDOWS _USRDLL)
#     if(MSVC)
#         add_compile_options(/W3)
#     endif()
# elseif(UNIX)
#     add_compile_definitions(LINUX)

#     if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
#         set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
#         set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
#     elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
#         message(WARNING "aarch64 detected. For 32-bit ARM build, install arm-linux-gnueabihf-gcc and set CMAKE_C_COMPILER/CMAKE_CXX_COMPILER")
#     endif()

#     set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "")
# endif()

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

add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/common
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/dlls
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/pm_shared
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/hlsdk/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/metamod
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/thirdparty/angelscript-sdk/angelscript/include
    ${CMAKE_CURRENT_SOURCE_DIR}/metamod/asext/include
)

set(SOURCES
    dllapi.cpp
    meta_api.cpp
    profiler.cpp
    hooks.cpp
    commands.cpp
)

if(WIN32)
    list(APPEND SOURCES dllmain.cpp)
endif()

set(HEADERS
    asspark.hpp
    profiler.hpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

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

if(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/addons/metamod/dlls
        PREFIX ""
        SUFFIX ".so"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE kernel32 user32)
endif()

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${PROJECT_NAME}>
        ${CMAKE_CURRENT_SOURCE_DIR}/build/addons/metamod/dlls/
    )
endif()

message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
