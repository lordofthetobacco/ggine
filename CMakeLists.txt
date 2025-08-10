cmake_minimum_required(VERSION 3.20)
project(ggine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.8
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
add_executable(ggine WIN32
    src/main.cpp
    src/Engine.cpp
    src/Engine.h
    src/Scene.h
    src/ObjLoader.cpp
    src/ObjLoader.h
)
target_include_directories(ggine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ggine PRIVATE d3d12 dxgi dxguid d3dcompiler imgui)
if (MSVC)
    target_compile_options(ggine PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()
set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.hlsl)
set(SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
add_custom_command(
    OUTPUT ${SHADER_BIN_DIR}/triangle_vs.cso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    COMMAND dxc -T vs_6_0 -E VSMain -Fo ${SHADER_BIN_DIR}/triangle_vs.cso ${SHADER_SRC}
    DEPENDS ${SHADER_SRC}
)
add_custom_command(
    OUTPUT ${SHADER_BIN_DIR}/triangle_ps.cso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    COMMAND dxc -T ps_6_0 -E PSMain -Fo ${SHADER_BIN_DIR}/triangle_ps.cso ${SHADER_SRC}
    DEPENDS ${SHADER_SRC}
)
add_custom_target(shaders ALL DEPENDS ${SHADER_BIN_DIR}/triangle_vs.cso ${SHADER_BIN_DIR}/triangle_ps.cso)
add_dependencies(ggine shaders)
add_custom_command(TARGET ggine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ggine>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_BIN_DIR} $<TARGET_FILE_DIR:ggine>/shaders
)
