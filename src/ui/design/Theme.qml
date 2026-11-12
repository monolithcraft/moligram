pragma Singleton

import QtQuick

QtObject {
    id: theme

    enum Mode {
        Dark,
        Light,
        Oled
    }

    property int mode: Theme.Dark
    property bool reducedMotion: false

    readonly property bool dark: mode !== Theme.Light

    readonly property SurfaceTokens surface: SurfaceTokens {
        dark: theme.dark
        oled: theme.mode === Theme.Oled
    }
    readonly property LineTokens line: LineTokens {
        dark: theme.dark
    }
    readonly property TextTokens text: TextTokens {
        dark: theme.dark
    }
    readonly property ToneTokens tone: ToneTokens {
        dark: theme.dark
    }
    readonly property SpaceTokens space: SpaceTokens {}
    readonly property RadiusTokens radius: RadiusTokens {}
    readonly property FontTokens font: FontTokens {}
    readonly property MotionTokens motion: MotionTokens {
        reduced: theme.reducedMotion
    }

    component SurfaceTokens: QtObject {
        property bool dark
        property bool oled

        readonly property color base: oled ? "#000000" : dark ? "#0a0a0b" : "#f4f3f1"
        readonly property color raised: dark ? "#111113" : "#fbfaf9"
        readonly property color overlay: dark ? "#18181b" : "#ffffff"
        readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.04)
        readonly property color pressed: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
        readonly property color inverse: dark ? "#f2f2f3" : "#111113"
    }

    component LineTokens: QtObject {
        property bool dark

        readonly property color subtle: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.08)
        readonly property color strong: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.16)
        readonly property color focus: dark ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(0, 0, 0, 0.6)
    }

    component TextTokens: QtObject {
        property bool dark

        readonly property color primary: dark ? "#f2f2f3" : "#111113"
        readonly property color secondary: Qt.alpha(primary, dark ? 0.62 : 0.64)
        readonly property color tertiary: Qt.alpha(primary, dark ? 0.40 : 0.42)
        readonly property color inverse: dark ? "#111113" : "#f2f2f3"
    }

    component ToneTokens: QtObject {
        property bool dark

        readonly property color danger: dark ? "#ff6b6b" : "#d63c3c"
        readonly property color mention: dark ? "#e8e8e8" : "#111113"
    }

    component SpaceTokens: QtObject {
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 24
        readonly property int xxl: 32
    }

    component RadiusTokens: QtObject {
        readonly property int sm: 6
        readonly property int md: 10
        readonly property int lg: 16
    }

    component FontTokens: QtObject {
        readonly property string ui: "Inter Variable"
        readonly property string mono: "JetBrains Mono"
        readonly property var features: ({ "cv11": 1, "ss01": 1, "tnum": 1 })

        readonly property int micro: 11
        readonly property int caption: 12
        readonly property int small: 13
        readonly property int body: 14
        readonly property int title: 16
        readonly property int headline: 20
        readonly property int display: 28
    }

    component MotionTokens: QtObject {
        property bool reduced

        readonly property int quick: reduced ? 0 : 90
        readonly property int standard: reduced ? 0 : 160
        readonly property int emphasized: reduced ? 0 : 240
    }
}
