cmake_minimum_required(VERSION 3.15)
project(OpenLamina
        VERSION 0.1.3
        DESCRIPTION "OpenLamina"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS ON)

if (MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif ()

set(OPENLAMINA_CORE_SOURCES
        tools/debug.cpp
        tools/debug.hpp
        tools/error.cpp
        tools/error.hpp
        tools/utf8_io.cpp
        tools/utf8_io.hpp
        tools/lang/builtins.cpp
        tools/lang/builtins.hpp
        tools/lang/type_methods.cpp
        tools/lang/type_methods.hpp
        tools/lang/std_modules.cpp
        tools/lang/std_modules.hpp
        tools/lang/number.cpp
        tools/lang/number.hpp
        tools/lang/rational.cpp
        tools/lang/rational.hpp
        src/irgen/generator.cpp
        src/irgen/generator.hpp
        src/irgen/optimizer.cpp
        src/irgen/optimizer.hpp
        src/irgen/bytecode_file.cpp
        src/irgen/bytecode_file.hpp
        src/lexer/lexer.cpp
        src/lexer/lexer.hpp
        src/parser/parser.cpp
        src/parser/parser.hpp
        src/front-end/front_end.cpp
        src/front-end/front_end.hpp
        src/front-end/repl_session.cpp
        src/front-end/repl_session.hpp
        src/parser/ast.hpp
        src/irgen/opcode.cpp
        src/irgen/cell_pool.cpp
        src/irgen/opcode.hpp
        src/irgen/struct_types.cpp
        src/irgen/struct_types.hpp
        src/irgen/typing.cpp
        src/irgen/typing.hpp
        src/irgen/iterator_ops.cpp
        src/irgen/iterator_ops.hpp
        src/irgen/exceptions.cpp
        src/irgen/exceptions.hpp
        src/irgen/value_copy.cpp
        src/irgen/value_copy.hpp
        src/irgen/friend_function.cpp
        src/irgen/friend_function.hpp
        src/irgen/runtime_ast.cpp
        src/irgen/runtime_ast.hpp
        src/irgen/macro_ops.cpp
        src/irgen/macro_ops.hpp
        src/repl/repl.cpp
        src/repl/repl.hpp
        tools/cli.cpp
        tools/cli.hpp
        tools/checker.cpp
        tools/checker.hpp
)

set(OPENLAMINA_TEST_SOURCES
        test/test.cpp
        tools/debug.cpp
        tools/error.cpp
        tools/utf8_io.cpp
        tools/lang/builtins.cpp
        tools/lang/number.cpp
        tools/lang/rational.cpp
        tools/lang/type_methods.cpp
        tools/lang/std_modules.cpp
        src/irgen/generator.cpp
        src/irgen/optimizer.cpp
        src/irgen/bytecode_file.cpp
        src/lexer/lexer.cpp
        src/parser/parser.cpp
        src/front-end/front_end.cpp
        src/front-end/repl_session.cpp
        src/irgen/opcode.cpp
        src/irgen/cell_pool.cpp
        src/irgen/iterator_ops.cpp
        src/irgen/struct_types.cpp
        src/irgen/typing.cpp
        src/irgen/exceptions.cpp
        src/irgen/friend_function.cpp
        src/irgen/value_copy.cpp
        src/irgen/runtime_ast.cpp
        src/irgen/macro_ops.cpp
        tools/checker.hpp
        tools/checker.cpp
)

add_executable(OpenLamina main.cpp ${OPENLAMINA_CORE_SOURCES})

find_package(ZLIB QUIET)
if (ZLIB_FOUND)
    message(STATUS "OpenLamina: .lmc zlib compression enabled")
    target_link_libraries(OpenLamina PRIVATE ZLIB::ZLIB)
    target_compile_definitions(OpenLamina PRIVATE OPENLAMINA_LMC_ZLIB=1)
elseif (MINGW)
    find_library(OPENLAMINA_ZLIB z PATHS "D:/msys64/mingw64/lib" "C:/msys64/mingw64/lib")
    find_path(OPENLAMINA_ZLIB_INCLUDE zlib.h PATHS "D:/msys64/mingw64/include" "C:/msys64/mingw64/include")
    if (OPENLAMINA_ZLIB AND OPENLAMINA_ZLIB_INCLUDE)
        message(STATUS "OpenLamina: .lmc zlib compression enabled (manual MinGW path)")
        target_include_directories(OpenLamina PRIVATE ${OPENLAMINA_ZLIB_INCLUDE})
        target_link_libraries(OpenLamina PRIVATE ${OPENLAMINA_ZLIB})
        target_compile_definitions(OpenLamina PRIVATE OPENLAMINA_LMC_ZLIB=1)
        set(OPENLAMINA_LMC_ZLIB_MANUAL TRUE)
    else ()
        message(STATUS "OpenLamina: ZLIB not found, .lmc uses varint only (no zlib)")
    endif ()
else ()
    message(STATUS "OpenLamina: ZLIB not found, .lmc uses varint only (no zlib)")
endif ()

option(BUILD_TESTS "Build OpenLamina test runner" ON)
if (BUILD_TESTS)
    add_executable(OpenLaminaTests test/main.cpp ${OPENLAMINA_TEST_SOURCES})
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(OpenLaminaTests PRIVATE -mcmodel=large)
    endif()
endif ()

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
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g0 -fno-math-errno -march=native -DNDEBUG")
    endif ()
    message(STATUS "OpenLamina: RELEASE mode (aggressive optimization)")
endif ()

target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(OpenLamina PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (BUILD_TESTS)
    target_include_directories(OpenLaminaTests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(OpenLaminaTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(OpenLaminaTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(OpenLaminaTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
    if (WIN32)
        target_link_libraries(OpenLaminaTests PRIVATE psapi)
    endif ()
    if (ZLIB_FOUND)
        target_link_libraries(OpenLaminaTests PRIVATE ZLIB::ZLIB)
        target_compile_definitions(OpenLaminaTests PRIVATE OPENLAMINA_LMC_ZLIB=1)
    elseif (OPENLAMINA_LMC_ZLIB_MANUAL)
        target_include_directories(OpenLaminaTests PRIVATE ${OPENLAMINA_ZLIB_INCLUDE})
        target_link_libraries(OpenLaminaTests PRIVATE ${OPENLAMINA_ZLIB})
        target_compile_definitions(OpenLaminaTests PRIVATE OPENLAMINA_LMC_ZLIB=1)
    endif ()
endif ()

target_compile_definitions(OpenLamina PRIVATE
        OPENLAMINA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        OPENLAMINA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        OPENLAMINA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

option(FETCH_LAMMP "Fetch LAMMP from GitHub repository" ON)


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

    if (TARGET LammpCore AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(LammpCore PRIVATE m)
    endif ()

    if (WIN32 AND TARGET LammpCore)
        add_custom_command(TARGET OpenLamina POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:LammpCore>"
                "$<TARGET_FILE_DIR:OpenLamina>"
                COMMENT "Copying LammpCore.dll to output directory"
        )
        if (BUILD_TESTS)
            add_custom_command(TARGET OpenLaminaTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:LammpCore>"
                    "$<TARGET_FILE_DIR:OpenLaminaTests>"
                    COMMENT "Copying LammpCore.dll to test output directory"
            )
        endif ()
    endif ()

    target_include_directories(OpenLamina PRIVATE
            ${LAMMP_SOURCE_DIR}/include
    )
    if (BUILD_TESTS)
        target_include_directories(OpenLaminaTests PRIVATE
                ${LAMMP_SOURCE_DIR}/include
        )
    endif ()

    target_link_libraries(OpenLamina PRIVATE LammpCore)
    if (BUILD_TESTS)
        target_link_libraries(OpenLaminaTests PRIVATE LammpCore)
    endif ()
else ()
    message(STATUS "Try to use local LAMMP")
    target_link_libraries(OpenLamina PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/libLammpCore.dll")
    if (BUILD_TESTS)
        target_link_libraries(OpenLaminaTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/libLammpCore.dll")
    endif ()
endif ()
