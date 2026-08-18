cmake_minimum_required(VERSION 3.16)
project(lunify VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(lunify STATIC
    src/ast.cpp
    src/config.cpp
    src/json.cpp
    src/lexer.cpp
    src/minifier.cpp
    src/options.cpp
    src/parser.cpp
    src/passes.cpp
    src/scope.cpp
)
target_include_directories(lunify PUBLIC include)

add_executable(lunify_cli src/main.cpp src/cli.cpp)
target_include_directories(lunify_cli PRIVATE include)
target_link_libraries(lunify_cli PRIVATE lunify)

enable_testing()
foreach(test_name lexer parser minifier passes config)
    add_executable(test_${test_name} tests/test_${test_name}.cpp)
    target_include_directories(test_${test_name} PRIVATE include)
    target_link_libraries(test_${test_name} PRIVATE lunify)
    add_test(NAME test_${test_name} COMMAND test_${test_name})
endforeach()

install(TARGETS lunify lunify_cli RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(DIRECTORY include/lunify DESTINATION include)

option(LUNIFY_BUILD_BENCH "Build the benchmark" OFF)
if(LUNIFY_BUILD_BENCH)
    add_executable(lunify_bench benchmarks/bench.cpp)
    target_include_directories(lunify_bench PRIVATE include)
    target_link_libraries(lunify_bench PRIVATE lunify)
endif()
