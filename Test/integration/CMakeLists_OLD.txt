# Test/integration/CMakeLists.txt
set(INTEGRATION_TEST_HEADERS
    tests/test_initialization_flow.h
)

set(INTEGRATION_TEST_SOURCES
    main.cpp
    tests/test_initialization_flow.cpp
)

qt6_wrap_cpp(INTEGRATION_TEST_MOCS ${INTEGRATION_TEST_HEADERS})

add_executable(QChartIntegrationTests ${INTEGRATION_TEST_SOURCES} ${INTEGRATION_TEST_MOCS})
set_target_properties(QChartIntegrationTests PROPERTIES AUTOMOC OFF)
target_include_directories(QChartIntegrationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QChartIntegrationTests PRIVATE QChartWidget::QChartWidget Qt6::Test)

add_test(NAME QChartIntegrationTests COMMAND QChartIntegrationTests)
if(NOT WIN32)
    set_tests_properties(QChartIntegrationTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()