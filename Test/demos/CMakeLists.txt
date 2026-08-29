# Test/demos/CMakeLists.txt
add_executable(QChartDemo
    test.cpp
    demo_polar.cpp
    demo_bar.cpp
    demo_pendulum.cpp
    demo_sort.cpp
    demo_camera.cpp
    demo_swirl.cpp
    demo_stress.cpp
    demo_theme.cpp
    demo_scatter3d.cpp
    demo_line3d.cpp
    demo_surface3d.cpp
)
set_target_properties(QChartDemo PROPERTIES AUTOMOC OFF)
target_link_libraries(QChartDemo PRIVATE QChartWidget::QChartWidget)

# windeployqt 部署
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${CMAKE_PREFIX_PATH}/bin)
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET QChartDemo POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:QChartDemo>
        COMMENT "Deploying Qt dependencies for QChartDemo"
    )
endif()