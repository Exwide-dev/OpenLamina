# 修正 CMake 最低版本要求
cmake_minimum_required(VERSION 3.15)
project(OpenLamina
        VERSION 0.1.2
        DESCRIPTION "OpenLamina"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================
# 1. 配置 Flex & Bison
# ==============================================
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# 词法和语法文件路径
set(LEXER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer/scanner.l")
set(PARSER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parser.y")

# 定义 Flex 和 Bison 的生成目标
# 这里会生成 lexer.cpp 和 parser.cpp 等文件
FLEX_TARGET(OpenLaminaScanner ${LEXER_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/lexer_generated.cpp
        COMPILE_FLAGS --nounistd)
BISON_TARGET(OpenLaminaParser ${PARSER_SRC} ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp)

# 声明 Flex 和 Bison 的依赖关系
ADD_FLEX_BISON_DEPENDENCY(OpenLaminaScanner OpenLaminaParser)

# ==============================================
# 2. 配置 SymEngine
# ==============================================
# 如果 SymEngine 安装在了非标准路径，可以通过 PATHS 指定
# find_package(SymEngine 0.6.0 CONFIG PATHS /path/to/symengine)
# find_package(SymEngine 0.6.0 CONFIG REQUIRED)

# ==============================================
# 3. 创建可执行文件
# ==============================================
# 将你项目中的其他源文件（如 main.cpp）和生成的文件一起添加
add_executable(
    OpenLamina
    main.cpp
    ${FLEX_OpenLaminaScanner_OUTPUTS}
    ${BISON_OpenLaminaParser_OUTPUTS}
        tools/debug.cpp
        tools/debug.hpp
        tools/error.cpp
        tools/error.hpp
        tools/lang/builtins.cpp
        tools/lang/builtins.hpp

        src/irgen/generator.cpp
        src/irgen/generator.hpp
        src/lexer/lex.cpp
        src/front-end/front_end.cpp
        src/front-end/front_end.hpp

        src/irgen/opcode.cpp
        src/irgen/opcode.hpp
        src/repl/repl.cpp
        src/repl/repl.hpp
        src/lexer/lexer_vars.hpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (MSVC)
        set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
    else ()
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    endif ()
    message(STATUS "OpenLamina: DEBUG mode")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "/O2 /arch:AVX2 /GL /DNDEBUG")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    endif ()
    message(STATUS "OpenLamina: RELEASE mode (aggressive optimization)")
endif ()


# ==============================================
# 4. 链接库与包含目录
# ==============================================
# 包含 Flex/Bison 生成的文件所在的目录（通常是 CMAKE_CURRENT_BINARY_DIR）
target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
# 包含源码目录
target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 链接 SymEngine 库
# target_link_libraries(OpenLamina PRIVATE ${SYMENGINE_LIBRARIES})

target_compile_definitions(OpenLamina PRIVATE
        OPENLAMINA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        OPENLAMINA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        OPENLAMINA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
#[[
message(STATUS "OpenLamina: Fetching LAMMP")

include(FetchContent)

FetchContent_Declare(
        LAMMP
        GIT_REPOSITORY https://github.com/Lamina-dev/LAMMP.git
        GIT_TAG main
)

set(USE_ASM OFF CACHE BOOL "Disable assembly" FORCE)

FetchContent_MakeAvailable(LAMMP)

# 链接 LammpCore 动态库
target_link_libraries(OpenLamina PRIVATE LammpCore)

# 如果需要使用 LAMMP 的头文件
target_include_directories(OpenLamina PRIVATE
        ${LAMMP_SOURCE_DIR}/include
)]]
