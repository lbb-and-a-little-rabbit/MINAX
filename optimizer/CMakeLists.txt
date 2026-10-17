cmake_minimum_required(VERSION 3.18)
project(minax_optimizer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_path(NLOPT_INCLUDE_DIR nlopt.hpp REQUIRED)
find_library(NLOPT_LIBRARY NAMES nlopt_cxx nlopt REQUIRED)
add_library(minax_optimizer src/Optimizer.cpp)
target_include_directories(minax_optimizer PUBLIC include PRIVATE ${NLOPT_INCLUDE_DIR})
target_link_libraries(minax_optimizer PUBLIC ${NLOPT_LIBRARY})
add_executable(optimizer_demo examples/demo.cpp)
target_link_libraries(optimizer_demo PRIVATE minax_optimizer)
enable_testing()
add_executable(optimizer_tests tests/OptimizerTest.cpp)
target_link_libraries(optimizer_tests PRIVATE minax_optimizer)
add_test(NAME optimizer_tests COMMAND optimizer_tests)

# 独立测试不依赖原编译器；开启后可直接解析 MINAX 数学表达式。
option(MINAX_WITH_EXPRESSIONS "Build adapter to existing MINAX compiler/evaluator" OFF)
set(MINAX_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "MINAX project root")
if(MINAX_WITH_EXPRESSIONS)
  add_library(minax_expression_optimizer
    src/ExpressionProblem.cpp
    ${MINAX_SOURCE_ROOT}/compiler/src/Lexical.cpp
    ${MINAX_SOURCE_ROOT}/compiler/src/Syntax.cpp
    ${MINAX_SOURCE_ROOT}/compiler/src/Semantic.cpp
    ${MINAX_SOURCE_ROOT}/evaluator/src/Evaluator.cpp)
  target_include_directories(minax_expression_optimizer PUBLIC include
    ${MINAX_SOURCE_ROOT}/compiler/include ${MINAX_SOURCE_ROOT}/evaluator/include)
  target_link_libraries(minax_expression_optimizer PUBLIC minax_optimizer)
  add_executable(minax_optimize examples/expression_cli.cpp)
  target_link_libraries(minax_optimize PRIVATE minax_expression_optimizer)
  add_executable(expression_tests tests/ExpressionTest.cpp)
  target_link_libraries(expression_tests PRIVATE minax_expression_optimizer)
  add_test(NAME expression_tests COMMAND expression_tests)
endif()
