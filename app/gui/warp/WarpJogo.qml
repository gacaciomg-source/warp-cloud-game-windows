import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import WarpApi 1.0

// A tela de um jogo, antes de jogar: arte, descrição, loja, como se joga e o
// botão Jogar ("Continuar jogando" para quem já jogou neste computador).
Item {
    id: tela
    property bool warp: true
    property var jogo
    objectName: jogo ? jogo.title : ""

    function modos() {
        var c = jogo && jogo.controls ? jogo.controls.split(",") : ["controle"]
        var nomes = { "controle": qsTr("Controle"), "teclado": qsTr("Teclado e mouse"), "toque": qsTr("Toque na tela") }
        return c.map(function (m) { return nomes[m] || m })
    }

    StackView.onActivated: if (jogar.visible) jogar.forceActiveFocus(); else voltar.forceActiveFocus()

    Rectangle { anchors.fill: parent; color: "#07131F" }
    Rectangle { anchors.fill: parent; color: tela.jogo && tela.jogo.accentColor ? tela.jogo.accentColor : "#15304A"; opacity: 0.35 }
    Image {
        anchors.fill: parent
        source: tela.jogo && tela.jogo.coverUrl ? tela.jogo.coverUrl : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 0.5
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#F207131F" }
            GradientStop { position: 0.55; color: "#B307131F" }
            GradientStop { position: 1.0; color: "#3307131F" }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: corpo.height + 60
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: corpo
            x: 48
            y: 24
            width: Math.min(680, tela.width - 96)
            spacing: 12

            WarpBotao {
                id: voltar
                text: qsTr("←  Jogos")
                onClicked: stackView.pop()
            }

            Text {
                Layout.topMargin: 24
                text: tela.jogo && tela.jogo.genre ? tela.jogo.genre.toUpperCase() : "CLOUD GAME"
                color: "#00C48E"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 2
            }
            Text {
                Layout.fillWidth: true
                text: tela.jogo ? tela.jogo.title : ""
                color: "#EAF3F8"; font.pixelSize: 48; font.bold: true; wrapMode: Text.WordWrap
            }
            Row {
                spacing: 8
                Repeater {
                    model: [
                        tela.jogo && tela.jogo.soAdmin ? qsTr("Só administrador") : "",
                        tela.jogo && tela.jogo.jogavel ? qsTr("Disponível agora") : qsTr("Em breve"),
                        qsTr("Progresso salvo na nuvem")
                    ].filter(function (s) { return s !== "" })
                    Rectangle {
                        radius: 8
                        color: "#1FFFFFFF"
                        width: rotulo.implicitWidth + 20
                        height: rotulo.implicitHeight + 10
                        Text { id: rotulo; anchors.centerIn: parent; text: modelData; color: "#ADC4D6"; font.pixelSize: 13; font.bold: true }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: tela.jogo && tela.jogo.description ? tela.jogo.description
                      : qsTr("Jogue direto da nuvem da WARP, sem instalar nada e sem precisar de um computador potente. Entre com a sua conta da loja e continue de onde parou.")
                color: "#ADC4D6"; font.pixelSize: 16; wrapMode: Text.WordWrap
            }

            Row {
                Layout.topMargin: 16
                spacing: 12
                WarpBotao {
                    id: jogar
                    principal: true
                    visible: tela.jogo && tela.jogo.jogavel
                    text: WarpApi.valor("jogado/" + (tela.jogo ? tela.jogo.slug : ""), 0) > 0 ? qsTr("▶  Continuar jogando") : qsTr("▶  Jogar")
                    onClicked: stackView.push(Qt.resolvedUrl("WarpSessaoView.qml"), { "jogo": tela.jogo })
                    KeyNavigation.right: config
                    KeyNavigation.up: voltar
                }
                WarpBotao {
                    id: config
                    text: qsTr("Qualidade e controles")
                    onClicked: stackView.push(Qt.resolvedUrl("WarpConfig.qml"))
                    KeyNavigation.up: voltar
                }
            }

            Text {
                Layout.topMargin: 24
                visible: tela.jogo && tela.jogo.store
                text: qsTr("Loja: %1").arg(tela.jogo && tela.jogo.store ? tela.jogo.store : "")
                color: "#EAF3F8"; font.pixelSize: 15; font.bold: true
            }
            Text {
                Layout.fillWidth: true
                visible: tela.jogo && tela.jogo.store
                text: qsTr("O jogo precisa estar na sua biblioteca. Quando ele pedir, entre com a sua conta da loja — a WARP não guarda essa senha.")
                color: "#6F8BA3"; font.pixelSize: 14; wrapMode: Text.WordWrap
            }
            Text {
                Layout.topMargin: 14
                text: qsTr("Como jogar: %1").arg(tela.modos().join(" · "))
                color: "#EAF3F8"; font.pixelSize: 15; font.bold: true
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 14
                text: qsTr("Seu progresso fica salvo na sua conta. Quando você sai, a WARP fecha o jogo do jeito certo e espera a nuvem da loja terminar de enviar o save antes de liberar a máquina.")
                color: "#6F8BA3"; font.pixelSize: 14; wrapMode: Text.WordWrap
            }
        }
    }
}
