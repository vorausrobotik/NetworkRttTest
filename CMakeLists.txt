cmake_minimum_required(VERSION 3.25)
project(NetworkRttTest-dev)

# make sure to set this right in the beginning to avoid problems when compilation units are compiled with different
# standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF) # set this to OFF as long as we are adding dependencies

########################################################################################################################
# options

option(COVERAGE "Enable coverage" OFF)
option(ASAN "Enable address sanitizer" OFF)
option(TSAN "Enable thread sanitizer" OFF)
option(UBSAN "Enable undefined behaviour sanitizer" OFF)

########################################################################################################################
# add dependencies that needed to be added via add_subdirectory here

add_subdirectory(external/voraus-rt-utils SYSTEM)

########################################################################################################################
# setup compiler flags

if (ASAN)
    message(STATUS "address sanitizer: enabled")
    add_compile_options("-fsanitize=address")
    add_link_options("-fsanitize=address")
else()
    message(STATUS "address sanitizer: disabled")
endif()

if (UBSAN)
    message(STATUS "undefined behaviour sanitizer: enabled")
    add_compile_options("-fsanitize=undefined")
    add_link_options("-fsanitize=undefined")
else()
    message(STATUS "undefined behaviour sanitizer: disabled")
endif()

if (TSAN)
    message(STATUS "thread sanitizer: enabled")
    add_compile_options("-fsanitize=thread")
    add_link_options("-fsanitize=thread")
else()
    message(STATUS "thread sanitizer: disabled")
endif()

if (COVERAGE)
    message(STATUS "coverage: enabled")
    add_compile_options("--coverage")
    add_link_options("--coverage")
else()
    message(STATUS "coverage: disabled")
endif()


# Define development compile options here
add_compile_options("-Wall" "-Wpedantic" "-Wextra" "-Werror")

########################################################################################################################
# actual source

# enable compile commands at this point to only get our own stuff checked by static analysis tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)
########################################################################################################################
