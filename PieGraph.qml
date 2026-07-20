import QtQuick
import QtGraphs

GraphsView {
    id: graphView
    antialiasing: true
//    backgroundColor: "transparent"

    Component.onCompleted: {
        console.log("QML Component completed. pieSeries =", pieSeries)
        if (pieSeries) {
            // 直接赋值给 seriesList
            seriesList = [ pieSeries ]
            console.log("Series set. Number of series:", seriesList.length)
        } else {
            console.warn("pieSeries is null!")
        }
    }
}