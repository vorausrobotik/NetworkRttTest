add_executable(utest_runner TestFrame.cpp TestOptions.cpp TestBenchmark.cpp)

target_link_libraries(utest_runner PRIVATE NetworkRttTest::lib Catch2::Catch2WithMain)
catch_discover_tests(utest_runner)
