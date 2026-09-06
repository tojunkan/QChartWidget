# Test —— S0 阶段只构建分期测试；demos/bench 默认关（未迁移）
if(BUILD_UNIT_TESTS)
    add_subdirectory(unit)
endif()
if(BUILD_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()
