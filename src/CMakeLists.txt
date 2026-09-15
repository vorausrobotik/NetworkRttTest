cmake_minimum_required(VERSION 3.25)
project(NetworkRttTest LANGUAGES CXX)

find_package(CLI11 REQUIRED)

add_library(${PROJECT_NAME}_lib STATIC RSock.cpp Options.cpp Benchmark.cpp TestFrame.cpp)
add_library(${PROJECT_NAME}::lib ALIAS ${PROJECT_NAME}_lib)

target_include_directories(${PROJECT_NAME}_lib PUBLIC include)
target_link_libraries(${PROJECT_NAME}_lib PUBLIC voraus-rt-utils::core CLI11::CLI11)


add_executable(NetworkRttTest main.cpp)
target_link_libraries(NetworkRttTest PRIVATE ${PROJECT_NAME}::lib)
