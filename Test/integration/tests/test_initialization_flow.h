// Test/integration/tests/test_initialization_flow.h
#ifndef TEST_INITIALIZATION_FLOW_H
#define TEST_INITIALIZATION_FLOW_H

#include <QObject>

class TestInitializationFlow : public QObject {
    Q_OBJECT
public:
    explicit TestInitializationFlow(QObject* parent = nullptr);

private slots:
    void test_emptyWidget_hasNullViewRect();
    void test_addLayer_createsProjection();
    void test_addAxis_before_projection_fails();
    void test_fullInit_resize_preservesArea();
    // 未来可加的更多测试
    // void test_fitMode_switch();
    // void test_theme_apply();
};

#endif