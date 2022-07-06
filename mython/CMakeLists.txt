cmake_minimum_required(VERSION 3.10)

project(Interpreter CXX)
set(CMAKE_CXX_STANDARD 17)

set(INTERPRETER_FILES runtime.h runtime.cpp runtime_test.cpp
                      lexer.h lexer.cpp lexer_test_open.cpp
                      statement.h statement.cpp statement_test.cpp
                      parse.h parse.cpp parse_test.cpp
                      test_runner_p.h
                      main.cpp)

add_executable(interpreter ${INTERPRETER_FILES})
