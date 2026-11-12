import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import WarpApi 1.0

// Entrar com a conta da WARP, ou criar uma ali mesmo (a mesma conta do site).
// A sessão fica guardada no computador.
Item {
    id: tela
    property bool warp: true
    objectName: "WARP Cloud Game"

    property bool entrando: false
    property bool criando: false

    function enviar() {
        erro.text = ""
        if (tela.criando) {
            if (nome.text.trim().length < 2) {
                erro.text = qsTr("Escreva o seu nome.")
                return
            }
            if (email.text.trim() === "" || senha.text === "") {
                erro.text = qsTr("Preencha o e-mail e a senha.")
                return
            }
            if (senha.text.length < 10) {
                erro.text = qsTr("A senha precisa ter pelo menos 10 caracteres.")
                return
            }
            if (!termos.checked) {
                erro.text = qsTr("Para criar a conta, aceite os termos de uso e a política de privacidade.")
                return
            }
            tela.entrando = true
            WarpApi.criarConta(nome.text, email.text, senha.text)
            return
        }
        if (email.text.trim() === "" || senha.text === "") {
            erro.text = qsTr("Preencha o e-mail e a senha.")
            return
        }
        tela.entrando = true
        WarpApi.entrar(email.text, senha.text)
    }

    function trocarModo() {
        tela.criando = !tela.criando
        erro.text = ""
        if (tela.criando) nome.forceActiveFocus()
        else email.forceActiveFocus()
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
            text: tela.criando ? qsTr("Crie a sua conta da WARP para jogar.") : qsTr("Entre com a sua conta da WARP para jogar.")
            color: "#6F8BA3"
            font.pixelSize: 14
        }

        TextField {
            id: nome
            Layout.fillWidth: true
            visible: tela.criando
            placeholderText: qsTr("Seu nome")
            Keys.onReturnPressed: email.forceActiveFocus()
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
            placeholderText: tela.criando ? qsTr("Senha (pelo menos 10 caracteres)") : qsTr("Senha")
            echoMode: TextInput.Password
            Keys.onReturnPressed: tela.enviar()
        }

        RowLayout {
            Layout.fillWidth: true
            visible: tela.criando
            spacing: 8
            CheckBox {
                id: termos
                Layout.alignment: Qt.AlignTop
            }
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                textFormat: Text.StyledText
                text: qsTr("Li e aceito os <a href=\"https://warpserver.com.br/termos\">termos de uso</a> e a <a href=\"https://warpserver.com.br/privacidade\">política de privacidade</a>.")
                color: "#ADC4D6"
                linkColor: "#00C48E"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                onLinkActivated: function (link) { Qt.openUrlExternally(link) }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
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
            text: tela.entrando ? (tela.criando ? qsTr("Criando a conta…") : qsTr("Entrando…"))
                                : (tela.criando ? qsTr("Criar conta") : qsTr("Entrar"))
            onClicked: tela.enviar()
        }

        WarpBotao {
            Layout.fillWidth: true
            enabled: !tela.entrando
            text: tela.criando ? qsTr("Já tenho conta: entrar") : qsTr("Ainda não tem conta? Criar conta")
            onClicked: tela.trocarModo()
        }
    }
}
