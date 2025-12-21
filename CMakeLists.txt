cmake_minimum_required(VERSION 3.14)

project(
        brights
        VERSION 0.0.1
        LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON)

add_executable(brights src/main.cpp
        src/render/wgslPreprocessor.hpp
        src/render/textureImage.hpp
        src/render/camera.hpp
        src/world/worldGenerator.hpp
        src/world/chunk.hpp
        src/world/tile.hpp
        src/render/native/graphicsContext.hpp
        src/native/window.hpp
        src/util/logger.hpp
)

set_target_properties(brights PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR OFF
)

set(FASTNOISE2_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
        FastNoise2
        GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
        GIT_TAG v0.10.0-alpha
)
FetchContent_MakeAvailable(FastNoise2)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        glfw3webgpu
        GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu.git
        GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(glfw3webgpu)

FetchContent_Declare(
        webgpu
        GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution.git
        GIT_TAG wgpu-v0.19.4.1
)
FetchContent_MakeAvailable(webgpu)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
)
FetchContent_MakeAvailable(stb)

add_library(MyStbImage INTERFACE)
target_include_directories(MyStbImage INTERFACE ${stb_SOURCE_DIR})


target_link_libraries(brights PRIVATE webgpu glfw glfw3webgpu FastNoise2 MyStbImage)

target_copy_webgpu_binaries(brights)

add_custom_target(CopyAssets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/assets"
        COMMENT "Copying assets folder to binary directory..."
)

# Make your executable depend on the CopyAssets target
# This ensures assets are checked/copied whenever you build 'brights'
add_dependencies(brights CopyAssets)