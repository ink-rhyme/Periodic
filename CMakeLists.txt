cmake_minimum_required(VERSION 3.16)
project(Periodic LANGUAGES C CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "❌ 禁止源码内构建！")
endif()

# 版本号（VERSION 文件）与当前提交哈希，供程序 --version 输出
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PERIODIC_VERSION_RAW)
    string(STRIP "${PERIODIC_VERSION_RAW}" PERIODIC_VERSION)
else()
    set(PERIODIC_VERSION "unknown")
endif()
execute_process(COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE PERIODIC_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RC)
if(NOT GIT_RC EQUAL 0 OR NOT PERIODIC_GIT_HASH)
    set(PERIODIC_GIT_HASH "nogit")
endif()
message(STATUS "✅ 版本: ${PERIODIC_VERSION} (${PERIODIC_GIT_HASH})")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_DEPENDS_USE_COMPILER ON)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE ON)

# 系统依赖
find_package(Freetype REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)

# 从~/.lib加载已安装的IGUI库
set(USER_LIB_DIR $ENV{HOME}/.lib)
set(UI_LIB ${USER_LIB_DIR}/libigui.a)
set(GLFW_LIB ${USER_LIB_DIR}/libglfw3.a)

if(NOT EXISTS ${UI_LIB})
    message(FATAL_ERROR "❌ 未找到libigui.a，请先编译IGUI并执行install安装到~/.lib")
endif()
if(NOT EXISTS ${GLFW_LIB})
    message(FATAL_ERROR "❌ 未找到libglfw3.a，请先编译IGUI并执行install安装到~/.lib")
endif()

message(STATUS "✅ UI库: ${UI_LIB}")
message(STATUS "✅ GLFW库: ${GLFW_LIB}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
file(GLOB_RECURSE PERIODIC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(Periodic ${PERIODIC_SOURCES})

target_include_directories(Periodic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${USER_LIB_DIR}/include/igui
)

target_compile_definitions(Periodic PRIVATE
    PERIODIC_VERSION="${PERIODIC_VERSION}"
    PERIODIC_GIT_HASH="${PERIODIC_GIT_HASH}"
)

target_link_libraries(Periodic PRIVATE
    ${UI_LIB}
    ${GLFW_LIB}
    GLEW::GLEW
    Freetype::Freetype
    OpenGL::GL
    ${X11_LIBRARIES}
    pthread
)

# 清理target（避免CMake保留字clean，改名myclean）
add_custom_target(myclean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_SOURCE_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMENT "✅ 已清理Periodic编译输出"
)

# 自动拷贝assets资源到bin目录（程序运行时直接读取）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    add_custom_command(TARGET Periodic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    )
    message(STATUS "✅ 已配置assets资源自动拷贝到bin目录")
endif()

message(STATUS "===== Periodic配置完成 =====")
message(STATUS "  输出程序: bin/Periodic")

