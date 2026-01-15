cmake_minimum_required(VERSION 3.14)

project(
        brights
        VERSION 0.0.1
        LANGUAGES CXX C
)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/bin/deploy" CACHE PATH "Default install prefix" FORCE)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON)

add_executable(brights src/main.cpp
        src/render/wgslPreprocessor.hpp
        src/render/textureImage.hpp
        src/core/graphics/camera.hpp
        src/core/world/worldGenerator.hpp
        src/core/world/chunk.hpp
        src/core/world/tile.hpp
        src/render/graphicsContext.hpp
        src/platform/window.hpp
        src/util/logger.hpp
        src/core/world/world.hpp
        src/util/threadpool.hpp
        src/render/gpuBuffer.hpp
        src/render/gpuTexture.hpp
        src/render/gpuHelpers.hpp
        src/render/gpuContext.hpp
        src/core/world/worldRenderAdapter.hpp
        src/platform/input.hpp
        src/app/game.hpp
        src/app/application.hpp
        src/core/world/chunkMesher.hpp
        src/core/world/chunkMesher.cpp
        src/util/bitmask.hpp
        src/core/graphics/gameGraphics.hpp
        src/core/world/planet.hpp
        src/core/world/worldView.hpp
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

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(glm)

add_library(stbWrp INTERFACE)
target_include_directories(stbWrp INTERFACE ${stb_SOURCE_DIR})

target_include_directories(brights PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")


target_link_libraries(brights PRIVATE webgpu glfw glfw3webgpu FastNoise2 stbWrp glm)

target_copy_webgpu_binaries(brights)

add_custom_target(CopyAssets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/assets"
        COMMENT "Copying assets folder to binary directory..."
)


add_dependencies(brights CopyAssets)

install(TARGETS brights RUNTIME DESTINATION .)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION .)

install(FILES "$<TARGET_FILE_DIR:brights>/wgpu_native.dll"
        DESTINATION .
)