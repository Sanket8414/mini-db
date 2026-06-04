cmake_minimum_required(VERSION 3.10)
project(mini_db)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

# Source files for the main executable
set(SOURCES
    src/common/types.cpp
    src/common/value.cpp
    src/storage/disk_manager.cpp
    src/storage/page.cpp
    src/storage/buffer_pool.cpp
    src/index/b_plus_tree.cpp
    src/catalog/schema.cpp
    src/catalog/catalog.cpp
    src/sql/lexer.cpp
    src/sql/parser.cpp
    src/sql/ast.cpp
    src/execution/executor.cpp
    src/execution/tuple.cpp
    src/execution/seq_scan.cpp
    src/execution/index_scan.cpp
    src/execution/filter.cpp
    src/execution/projection.cpp
    src/execution/binder.cpp
    src/execution/planner.cpp
    src/main.cpp
)

add_executable(mini_db ${SOURCES})
target_include_directories(mini_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Tests
include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(TEST_SOURCES
    src/common/types.cpp
    src/common/value.cpp
    src/storage/disk_manager.cpp
    src/storage/page.cpp
    src/storage/buffer_pool.cpp
    src/index/b_plus_tree.cpp
    src/catalog/schema.cpp
    src/catalog/catalog.cpp
    src/sql/lexer.cpp
    src/sql/parser.cpp
    src/sql/ast.cpp
    src/execution/executor.cpp
    src/execution/tuple.cpp
    src/execution/seq_scan.cpp
    src/execution/index_scan.cpp
    src/execution/filter.cpp
    src/execution/projection.cpp
    src/execution/binder.cpp
    src/execution/planner.cpp
)

add_executable(tests
    test/storage_test.cpp
    test/buffer_pool_test.cpp
    test/b_plus_tree_test.cpp
    test/catalog_test.cpp
    test/sql_test.cpp
    test/executor_test.cpp
    test/integration_test.cpp
    ${TEST_SOURCES}
)

target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tests gtest_main)
add_test(NAME AllTests COMMAND tests)