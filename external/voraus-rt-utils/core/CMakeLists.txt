add_library(${PROJECT_NAME}_core STATIC src/CycleControl.cpp src/TimeHandler.cpp  src/core.cpp src/Histogram.cpp src/RtBenchmark.cpp)
add_library(${PROJECT_NAME}::core ALIAS ${PROJECT_NAME}_core)


target_include_directories(${PROJECT_NAME}_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_core PUBLIC pthread)

install(TARGETS ${PROJECT_NAME}_core EXPORT ${PROJECT_NAME}Targets)
install(DIRECTORY include/ TYPE INCLUDE)
