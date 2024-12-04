find_package(Catch2)
include(CTest)
include(Catch)

add_library(test_resources INTERFACE)

set(TEST_RESOURCE_LOCATION_DEFAULT ${CMAKE_CURRENT_SOURCE_DIR}/resources)
configure_file(test_resource_location.h.in test_resource_location.h @ONLY)
target_include_directories(test_resources INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(unit)