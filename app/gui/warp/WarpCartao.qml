import QtQuick 2.15

// Capa de um jogo nas fileiras do início. Foco (teclado, controle) aumenta e
// ganha moldura verde; clique ou Enter abre o jogo.
FocusScope {
    id: cartao
    property var jogo
    signal escolhido()
    signal ganhouFoco()

    width: 170
    height: 226
    activeFocusOnTab: true
    scale: activeFocus ? 1.07 : (area.containsMouse ? 1.03 : 1.0)
    Behavior on scale { NumberAnimation { duration: 140 } }
    onActiveFocusChanged: if (activeFocus) ganhouFoco()

    Keys.onReturnPressed: escolhido()
    Keys.onEnterPressed: escolhido()
    Keys.onSpacePressed: escolhido()

    Rectangle {
        id: fundo
        anchors.fill: parent
        radius: 14
        color: cartao.jogo && cartao.jogo.accentColor ? cartao.jogo.accentColor : "#15304A"
        clip: true

        Image {
            anchors.fill: parent
            source: cartao.jogo && cartao.jogo.coverUrl && cartao.jogo.coverUrl.indexOf(".mp4") < 0 ? cartao.jogo.coverUrl : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.55
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0007131F" }
                GradientStop { position: 1.0; color: "#F207131F" }
            }
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            text: cartao.jogo ? cartao.jogo.title : ""
            color: "#EAF3F8"
            font.pixelSize: 15
            font.bold: true
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Rectangle {
            visible: cartao.jogo && (cartao.jogo.soAdmin || !cartao.jogo.jogavel)
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 10
            radius: 6
            color: cartao.jogo && cartao.jogo.soAdmin ? "#00C48E" : "#CC07131F"
            width: selo.implicitWidth + 14
            height: selo.implicitHeight + 6
            Text {
                id: selo
                anchors.centerIn: parent
                text: cartao.jogo && cartao.jogo.soAdmin ? "ADMIN" : "EM BREVE"
                color: cartao.jogo && cartao.jogo.soAdmin ? "#04211A" : "#EAF3F8"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: "transparent"
        border.width: cartao.activeFocus ? 3 : 1
        border.color: cartao.activeFocus ? "#00C48E" : "#22FFFFFF"
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { cartao.forceActiveFocus(); cartao.escolhido() }
    }
}
