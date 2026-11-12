import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import WarpApi 1.0

// Entrar com a conta da WARP. A sessão fica guardada no computador.
Item {
    id: tela
    property bool warp: true
    objectName: "WARP Cloud Game"

    property bool entrando: false

    function entrar() {
        if (email.text.trim() === "" || senha.text === "") {
            erro.text = qsTr("Preencha o e-mail e a senha.")
            return
        }
        erro.text = ""
        entrando = true
        WarpApi.entrar(email.text, senha.text)
    }

    Connections {
        target: WarpApi
        function onEntrou() {
            tela.entrando = false
            Qt.callLater(function () { stackView.replace(null, Qt.resolvedUrl("WarpInicio.qml")) })
        }
        function onFalhouAoEntrar(mensagem) {
            tela.entrando = false
            erro.text = mensagem
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0A1A29" }
            GradientStop { position: 1.0; color: "#07131F" }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(420, tela.width - 48)
        spacing: 14

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "WARP"
            color: "#00C48E"
            font.pixelSize: 44
            font.bold: true
            font.letterSpacing: 6
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Cloud Game"
            color: "#EAF3F8"
            font.pixelSize: 22
            font.bold: true
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 18
            text: qsTr("Entre com a sua conta da WARP para jogar.")
            color: "#6F8BA3"
            font.pixelSize: 14
        }

        TextField {
            id: email
            Layout.fillWidth: true
            placeholderText: qsTr("E-mail")
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            focus: true
            Keys.onReturnPressed: senha.forceActiveFocus()
        }
        TextField {
            id: senha
            Layout.fillWidth: true
            placeholderText: qsTr("Senha")
            echoMode: TextInput.Password
            Keys.onReturnPressed: tela.entrar()
        }

        Text {
            id: erro
            Layout.fillWidth: true
            visible: text !== ""
            color: "#F2545B"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }

        WarpBotao {
            Layout.fillWidth: true
            Layout.topMargin: 6
            principal: true
            enabled: !tela.entrando
            text: tela.entrando ? qsTr("Entrando…") : qsTr("Entrar")
            onClicked: tela.entrar()
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            text: qsTr("Ainda não tem conta? Crie em warpserver.com.br")
            color: "#6F8BA3"
            font.pixelSize: 13
        }
    }
}
