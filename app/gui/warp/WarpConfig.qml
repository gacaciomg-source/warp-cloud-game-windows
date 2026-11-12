import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import StreamingPreferences 1.0
import WarpApi 1.0

// Configurações da WARP: qualidade da imagem (automática pelo teste de
// conexão, ou fixa), o teste e informações do app.
Item {
    id: tela
    property bool warp: true
    objectName: qsTr("Configurações")

    property string qualidade: WarpApi.valor("qualidade", "auto")
    property int ping: WarpApi.valor("rede/ping", 0)
    property real mbps: WarpApi.valor("rede/mbps", 0)
    property bool testando: false

    readonly property var perfis: [
        { id: "auto", nome: qsTr("Automática"), resumo: qsTr("A WARP testa a sua conexão e escolhe por você."), altura: 0, kbps: 0 },
        { id: "economica", nome: qsTr("Econômica"), resumo: qsTr("720p · 6 Mbps — internet móvel ou fraca."), altura: 720, kbps: 6000 },
        { id: "equilibrada", nome: qsTr("Equilibrada"), resumo: qsTr("1080p · 15 Mbps — boa para a maioria."), altura: 1080, kbps: 15000 },
        { id: "alta", nome: qsTr("Alta"), resumo: qsTr("1080p · 25 Mbps — imagem mais nítida."), altura: 1080, kbps: 25000 },
        { id: "maxima", nome: qsTr("Máxima"), resumo: qsTr("1080p · 40 Mbps — internet rápida, de preferência no cabo."), altura: 1080, kbps: 40000 }
    ]

    function escolher(p) {
        tela.qualidade = p.id
        WarpApi.definir("qualidade", p.id)
        if (p.id !== "auto") {
            StreamingPreferences.width = p.altura * 16 / 9
            StreamingPreferences.height = p.altura
            StreamingPreferences.fps = 60
            StreamingPreferences.bitrateKbps = p.kbps
            StreamingPreferences.save()
        }
    }

    function recomendado() {
        if (tela.mbps <= 0) return qsTr("teste a conexão")
        var kbps = Math.max(4000, Math.min(40000, Math.round(tela.mbps * 600)))
        if (tela.ping >= 80) kbps = Math.min(kbps, 12000)
        return (kbps >= 12000 ? "1080p" : "720p") + " · " + Math.round(kbps / 1000) + " Mbps"
    }

    Connections {
        target: WarpApi
        function onConexaoTestada(p, m) {
            tela.testando = false
            tela.ping = p
            tela.mbps = m
        }
    }

    StackView.onActivated: voltar.forceActiveFocus()

    Rectangle { anchors.fill: parent; color: "#07131F" }

    Flickable {
        anchors.fill: parent
        contentHeight: coluna.height + 60
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: coluna
            x: (tela.width - width) / 2
            y: 24
            width: Math.min(760, tela.width - 64)
            spacing: 10

            RowLayout {
                spacing: 16
                WarpBotao { id: voltar; text: qsTr("←  Voltar"); onClicked: stackView.pop() }
                Text { text: qsTr("Configurações"); color: "#EAF3F8"; font.pixelSize: 28; font.bold: true }
            }

            Text {
                Layout.topMargin: 26
                text: qsTr("QUALIDADE DA IMAGEM")
                color: "#00C48E"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 2
            }

            Repeater {
                model: tela.perfis
                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 72
                    radius: 14
                    color: tela.qualidade === modelData.id ? "#1F00C48E" : "#B30F2336"
                    border.width: activeFocus ? 3 : (tela.qualidade === modelData.id ? 2 : 1)
                    border.color: activeFocus ? "#FFFFFF" : (tela.qualidade === modelData.id ? "#00C48E" : "#331E3A53")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: tela.escolher(modelData)
                    Keys.onEnterPressed: tela.escolher(modelData)
                    Keys.onSpacePressed: tela.escolher(modelData)

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        spacing: 4
                        Text {
                            text: modelData.nome + (modelData.id === "auto" ? "   ·   " + qsTr("recomendada: %1").arg(tela.recomendado()) : "")
                            color: "#EAF3F8"; font.pixelSize: 16; font.bold: true
                        }
                        Text { text: modelData.resumo; color: "#ADC4D6"; font.pixelSize: 13 }
                    }
                    Text {
                        visible: tela.qualidade === modelData.id
                        anchors.right: parent.right
                        anchors.rightMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        text: "✓"
                        color: "#00C48E"; font.pixelSize: 22; font.bold: true
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { parent.forceActiveFocus(); tela.escolher(modelData) } }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                height: 96
                radius: 16
                color: "#B30F2336"
                border.width: 1
                border.color: "#331E3A53"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    Column {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: qsTr("Sua conexão com a WARP"); color: "#EAF3F8"; font.pixelSize: 15; font.bold: true }
                        Text {
                            text: tela.testando ? qsTr("Testando…")
                                  : tela.mbps > 0 ? qsTr("Ping %1 ms · %2 Mbps").arg(tela.ping).arg(tela.mbps.toFixed(1))
                                  : qsTr("Ainda não testada.")
                            color: "#ADC4D6"; font.pixelSize: 14
                        }
                    }
                    WarpBotao {
                        text: qsTr("Testar conexão")
                        enabled: !tela.testando
                        onClicked: { tela.testando = true; WarpApi.testarConexao() }
                    }
                }
            }

            Text {
                Layout.topMargin: 26
                text: qsTr("SOBRE")
                color: "#00C48E"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 2
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("WARP Cloud Game para Windows. O motor de transmissão vem do Moonlight (moonlight-stream.org), software livre sob a licença GPL-3.0. O código-fonte completo, com as mudanças da WARP, é público: github.com/gacaciomg-source/warp-cloud-game-windows")
                color: "#6F8BA3"; font.pixelSize: 13; wrapMode: Text.WordWrap
            }
        }
    }
}
