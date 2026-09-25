cmake_minimum_required(VERSION 3.20)
project(eliott-hub VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.1
)

FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw)

find_package(OpenGL REQUIRED)

FetchContent_Declare(
    tinyfiledialogs
    GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git
    GIT_TAG master
)
FetchContent_MakeAvailable(tinyfiledialogs)

add_library(tinyfiledialogs STATIC
    ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c
)

target_include_directories(tinyfiledialogs PUBLIC
    ${tinyfiledialogs_SOURCE_DIR}
)

if(WIN32)
    target_link_libraries(tinyfiledialogs PUBLIC comdlg32 ole32)
endif()

add_executable(eliott-hub
    src/main.cpp
    src/App.cpp
    src/PathUtils.cpp
    src/NetworkUtils.cpp
    src/ProcessLauncher.cpp
    src/Updater.cpp
    src/ProjectRepository.cpp
)

target_include_directories(eliott-hub PRIVATE
    include
)

target_compile_definitions(eliott-hub PRIVATE
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    HUB_VERSION="v${PROJECT_VERSION}"
)

target_link_libraries(eliott-hub PRIVATE
    imgui
    OpenGL::GL
    tinyfiledialogs
)
