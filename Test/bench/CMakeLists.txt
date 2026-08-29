# Test/bench/CMakeLists.txt
add_executable(QChartBench bench_main.cpp)
set_target_properties(QChartBench PROPERTIES AUTOMOC OFF)
target_link_libraries(QChartBench PRIVATE QChartWidget::QChartWidget)