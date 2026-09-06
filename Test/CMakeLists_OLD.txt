# Test/CMakeLists.txt
# 所有需要 Qt 测试框架的模块集中在此

if(BUILD_DEMOS)
    add_subdirectory(demos)
endif()

if(BUILD_BENCH)
    add_subdirectory(bench)
endif()

if(BUILD_UNIT_TESTS)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_subdirectory(unit)
endif()

if(BUILD_INTEGRATION_TESTS)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_subdirectory(integration)
endif()