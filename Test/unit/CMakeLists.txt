# Test/unit —— 分期单元测试（S0 起逐步重写）
# 旧版 CMakeLists_OLD.txt / main_OLD.cpp / tests/ 下旧 API 测试保留参考，暂不构建。
# 阶梯约定：每阶段在此追加新测试类（头文件 → qt6_wrap_cpp 手动 moc + 源文件列表 + main.cpp include + qExec 行）。
find_package(Qt6 6.2 REQUIRED COMPONENTS Test)

set(QCHART_UNIT_HEADERS
    # S0：在此追加测试头（qt6_wrap_cpp 手动 moc）
    tests/test_axis_pipeline.h
    # 批次 A：widget 容器冒烟（offscreen CPU）
    tests/test_widget_smoke.h
    # 批次 B1：3D 轴渲染冒烟（offscreen CPU）
    tests/test_axes3d_smoke.h
    # 批次 B2：widget3D 轻量容器冒烟（offscreen CPU）
    tests/test_widget3d_smoke.h
    # 批次 B3：drawAtEdge 专项
    tests/test_axis_edge.h
)

set(QCHART_UNIT_SOURCES
    main.cpp
    # S0：在此追加测试源
    tests/test_axis_pipeline.cpp
    # 批次 A：widget 容器冒烟（offscreen CPU）
    tests/test_widget_smoke.cpp
    # 批次 B1：3D 轴渲染冒烟（offscreen CPU）
    tests/test_axes3d_smoke.cpp
    # 批次 B2：widget3D 轻量容器冒烟（offscreen CPU）
    tests/test_widget3d_smoke.cpp
    # 批次 B3：drawAtEdge 专项
    tests/test_axis_edge.cpp
)

qt6_wrap_cpp(QCHART_UNIT_MOCS ${QCHART_UNIT_HEADERS})

add_executable(QChartUnitTests ${QCHART_UNIT_SOURCES} ${QCHART_UNIT_MOCS})
set_target_properties(QChartUnitTests PROPERTIES AUTOMOC OFF)
target_include_directories(QChartUnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/Test/unit/tests
)
target_link_libraries(QChartUnitTests PRIVATE QChartWidget::QChartWidget Qt6::Test)

add_test(NAME QChartUnitTests COMMAND QChartUnitTests)
if(NOT WIN32)
    set_tests_properties(QChartUnitTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
