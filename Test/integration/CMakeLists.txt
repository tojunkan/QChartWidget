# Test/integration —— 分期集成测试（S0 起逐步扩充）
# 旧版 CMakeLists_OLD.txt / main_OLD.cpp / test_initialization_flow 保留参考。
# 阶梯约定：每阶段在此追加集成测试类（多类搭配、覆盖不同组合）。
find_package(Qt6 6.2 REQUIRED COMPONENTS Test)

set(QCHART_INTEGRATION_HEADERS
    # S0：在此追加集成测试头
    tests/test_axis_matrix.h
    # 批次 A：widget GL 宿主冒烟
    tests/test_widget_gl.h
)

set(QCHART_INTEGRATION_SOURCES
    main.cpp
    # S0：在此追加集成测试源
    tests/test_axis_matrix.cpp
    # 批次 A：widget GL 宿主冒烟
    tests/test_widget_gl.cpp
)

qt6_wrap_cpp(QCHART_INTEGRATION_MOCS ${QCHART_INTEGRATION_HEADERS})

add_executable(QChartIntegrationTests ${QCHART_INTEGRATION_SOURCES} ${QCHART_INTEGRATION_MOCS})
set_target_properties(QChartIntegrationTests PROPERTIES AUTOMOC OFF)
target_include_directories(QChartIntegrationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/Test/integration/tests
)
target_link_libraries(QChartIntegrationTests PRIVATE QChartWidget::QChartWidget Qt6::Test)

add_test(NAME QChartIntegrationTests COMMAND QChartIntegrationTests)
if(WIN32)
    set_tests_properties(QChartIntegrationTests PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_PREFIX_PATH}/bin;$ENV{PATH}"
    )
else()
    set_tests_properties(QChartIntegrationTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
