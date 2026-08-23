cmake_minimum_required(VERSION 3.15)
project(YanshenNative VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-specific settings
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp src/*.c)

# Include directories
include_directories(include)

# Build as DLL
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Link with version.lib for GetModuleVersion
target_link_libraries(${PROJECT_NAME} PRIVATE
    kernel32
    user32
    advapi32
    ole32
    shlwapi
)

# Optimization
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /O2 /GL)
    target_link_options(${PROJECT_NAME} PRIVATE /LTCG)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -fomit-frame-pointer)
endif()

# Output to the deployment directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)