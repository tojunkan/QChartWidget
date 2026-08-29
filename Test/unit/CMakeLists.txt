# Test/unit/CMakeLists.txt
set(UNIT_TEST_HEADERS
    tests/test_qvalueaxis.h
    tests/test_qbarcategoryaxis.h
    tests/test_qlogaxis.h
    tests/test_qdatetimeaxis.h
    tests/test_qchartcamera.h
    tests/test_qchartcamera3d.h
    tests/test_qchartaxes3d.h
    tests/test_qchartmath.h
    tests/test_qchartprojection.h
    tests/test_qchartsurface3d.h
    tests/test_qchartrenderer.h
    tests/test_qchartrenderer3d.h
    tests/test_hittest.h
    tests/test_qcharthittester.h
    tests/test_qcharttheme.h
    tests/test_qchartlegend.h
    tests/test_legend_interact.h
    tests/test_export.h
    tests/test_qchartgl.h
    tests/test_qopenglrenderer.h
)

set(UNIT_TEST_SOURCES
    main.cpp
    tests/test_qvalueaxis.cpp
    tests/test_qbarcategoryaxis.cpp
    tests/test_qlogaxis.cpp
    tests/test_qdatetimeaxis.cpp
    tests/test_qchartcamera.cpp
    tests/test_qchartcamera3d.cpp
    tests/test_qchartaxes3d.cpp
    tests/test_qchartmath.cpp
    tests/test_qchartprojection.cpp
    tests/test_qchartsurface3d.cpp
    tests/test_qchartrenderer.cpp
    tests/test_qchartrenderer3d.cpp
    tests/test_hittest.cpp
    tests/test_qcharthittester.cpp
    tests/test_qcharttheme.cpp
    tests/test_qchartlegend.cpp
    tests/test_legend_interact.cpp
    tests/test_export.cpp
    tests/test_qchartgl.cpp
    tests/test_qopenglrenderer.cpp
)

qt6_wrap_cpp(UNIT_TEST_MOCS ${UNIT_TEST_HEADERS})

add_executable(QChartUnitTests ${UNIT_TEST_SOURCES} ${UNIT_TEST_MOCS})
set_target_properties(QChartUnitTests PROPERTIES AUTOMOC OFF)
target_include_directories(QChartUnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QChartUnitTests PRIVATE QChartWidget::QChartWidget Qt6::Test)

add_test(NAME QChartUnitTests COMMAND QChartUnitTests)
if(NOT WIN32)
    set_tests_properties(QChartUnitTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()