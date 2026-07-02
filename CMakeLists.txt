cmake_minimum_required(VERSION 3.14)

project(brights VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_INSTALL_PREFIX
    "${CMAKE_BINARY_DIR}/bin/deploy"
    CACHE PATH
    "Default install prefix"
    FORCE
)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON)

file(
    GLOB_RECURSE BRIGHTS_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(brights ${BRIGHTS_SOURCES})

set_target_properties(
    brights
    PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR OFF
)

set(FASTNOISE2_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "" FORCE)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

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
    GIT_TAG v1.3.0-alpha
)
FetchContent_MakeAvailable(glfw3webgpu)

FetchContent_Declare(
    webgpu
    GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution.git
    GIT_TAG wgpu-v24.0.0.2
)
FetchContent_MakeAvailable(webgpu)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.16.0
)
FetchContent_MakeAvailable(EnTT)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
)
FetchContent_MakeAvailable(imgui)

add_library(
    imgui
    STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
)
target_include_directories(
    imgui
    PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
target_link_libraries(imgui PUBLIC glfw webgpu)

add_library(stbWrp INTERFACE)
target_include_directories(stbWrp INTERFACE ${stb_SOURCE_DIR})

target_include_directories(brights PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# glm must be configured identically in every translation unit
target_compile_definitions(
    brights
    PRIVATE GLM_ENABLE_EXPERIMENTAL GLM_FORCE_DEFAULT_PACKED_GENTYPES
)

target_link_libraries(
    brights
    PRIVATE
        webgpu
        glfw
        glfw3webgpu
        FastNoise2
        stbWrp
        glm
        yaml-cpp
        imgui
        EnTT::EnTT
)

target_copy_webgpu_binaries(brights)

add_custom_target(
    CopyAssets
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/assets"
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/assets"
    COMMENT "Copying assets folder to binary directory..."
)

add_dependencies(brights CopyAssets)

configure_file(
    "${CMAKE_SOURCE_DIR}/config.yaml"
    "${CMAKE_BINARY_DIR}/config.yaml"
    COPYONLY
)

install(TARGETS brights RUNTIME DESTINATION .)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION .)

install(FILES "${CMAKE_SOURCE_DIR}/config.yaml" DESTINATION .)

install(FILES "$<TARGET_FILE_DIR:brights>/wgpu_native.dll" DESTINATION .)
