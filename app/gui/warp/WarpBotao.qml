import QtQuick 2.0
import QtQuick.Controls 2.2

// Botão da WARP: principal (verde) ou secundário (vidro escuro), com moldura
// no foco para teclado e controle. Enter e espaço clicam.
Button {
    id: botao
    property bool principal: false
    property bool perigo: false

    font.pixelSize: 16
    font.bold: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    leftPadding: 24
    rightPadding: 24
    topPadding: 13
    bottomPadding: 13
    scale: activeFocus ? 1.04 : 1.0
    Behavior on scale { NumberAnimation { duration: 120 } }

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    contentItem: Text {
        text: botao.text
        font: botao.font
        color: botao.principal ? "#04211A" : (botao.perigo ? "#FFB3B6" : "#EAF3F8")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: botao.enabled ? 1 : 0.5
    }

    background: Rectangle {
        radius: 12
        color: botao.principal ? (botao.down || botao.hovered ? "#1EE0A6" : "#00C48E")
             : botao.perigo ? (botao.down ? "#40F2545B" : "#14F2545B")
             : (botao.down ? "#33FFFFFF" : botao.hovered ? "#29FFFFFF" : "#1FFFFFFF")
        border.width: botao.activeFocus ? 3 : 1
        border.color: botao.activeFocus ? (botao.principal ? "#FFFFFF" : "#00C48E")
                    : botao.principal ? "transparent" : (botao.perigo ? "#99F2545B" : "#33FFFFFF")
        opacity: botao.enabled ? 1 : 0.5
    }
}
