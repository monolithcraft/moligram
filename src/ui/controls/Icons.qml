pragma Singleton

import QtQuick

QtObject {
    readonly property string search: "M14.5 9a5.5 5.5 0 1 1-11 0a5.5 5.5 0 1 1 11 0M13 13l3.5 3.5"
    readonly property string ghost: "M5 17V9a5 5 0 0 1 10 0v8l-1.67-1.25L11.67 17 10 15.75 8.33 17 6.67 15.75zM8 8.5v1M12 8.5v1"
    readonly property string broadcast: "M10 10v.01M7.17 7.17a4 4 0 0 0 0 5.66M12.83 7.17a4 4 0 0 1 0 5.66M4.7 4.7a7.5 7.5 0 0 0 0 10.6M15.3 4.7a7.5 7.5 0 0 1 0 10.6"
    readonly property string moon: "M16.5 12A7 7 0 1 1 8 3.5a5.5 5.5 0 0 0 8.5 8.5z"
    readonly property string sun: "M13 10a3 3 0 1 1-6 0a3 3 0 1 1 6 0M10 2.5v1.5M10 16v1.5M2.5 10H4M16 10h1.5M4.7 4.7l1.06 1.06M14.24 14.24l1.06 1.06M4.7 15.3l1.06-1.06M14.24 5.76l1.06-1.06"
}
