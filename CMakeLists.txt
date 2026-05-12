cmake_minimum_required(VERSION 3.15)
project(OpenLamina
        VERSION 0.1.3
        DESCRIPTION "OpenLamina"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# 词法和语法文件路径
set(LEXER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer/scanner.l")
set(PARSER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parser.y")

# 定义 Flex 和 Bison 的生成目标
FLEX_TARGET(OpenLaminaScanner ${LEXER_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/lexer_generated.cpp
        COMPILE_FLAGS --nounistd)
BISON_TARGET(OpenLaminaParser ${PARSER_SRC} ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp)

# 声明 Flex 和 Bison 的依赖关系
ADD_FLEX_BISON_DEPENDENCY(OpenLaminaScanner OpenLaminaParser)

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
    tools/lang/number.cpp
    tools/lang/number.hpp
    src/compiler/module_compiler.cpp
    src/compiler/module_compiler.hpp
    src/compiler/module_manager.cpp
    src/compiler/module_manager.hpp
    tools/cli.cpp
    tools/cli.hpp
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

target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(OpenLamina PRIVATE
        OPENLAMINA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        OPENLAMINA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        OPENLAMINA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

option(FETCH_LAMMP "Fetch LAMMP from GitHub repository" OFF)

if (FETCH_LAMMP)
    message(STATUS "OpenLamina: Fetching LAMMP")
    include(FetchContent)

    FetchContent_Declare(
            LAMMP
            GIT_REPOSITORY https://github.com/Lamina-dev/LAMMP.git
            GIT_TAG main
    )

    set(USE_ASM OFF CACHE BOOL "Disable assembly" FORCE)

    FetchContent_MakeAvailable(LAMMP)

    # 复制 DLL 到输出目录
    if (WIN32 AND TARGET LammpCore)
        add_custom_command(TARGET OpenLamina POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:LammpCore>"
                "$<TARGET_FILE_DIR:OpenLamina>"
                COMMENT "Copying LammpCore.dll to output directory"
        )
    endif ()

    target_include_directories(OpenLamina PRIVATE
            ${LAMMP_SOURCE_DIR}/include
    )

    target_link_libraries(OpenLamina PRIVATE LammpCore)
else()
    message(STATUS "Try to use local LAMMP")
    # 链接 LammpCore 动态库
    target_link_libraries(OpenLamina PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/libLammpCore.dll)
endif()