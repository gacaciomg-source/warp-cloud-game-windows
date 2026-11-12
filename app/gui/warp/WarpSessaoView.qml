import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import ComputerManager 1.0
import WarpApi 1.0
import WarpSessao 1.0

// Sessão: fila → máquina → acesso → jogo. Ao sair da tela do jogo, a máquina
// fica reservada por alguns minutos ("Voltar ao jogo"); no "Encerrar", a WARP
// fecha o jogo e espera o save subir para a nuvem antes de liberar a máquina.
Item {
    id: tela
    property bool warp: true
    property var jogo
    objectName: jogo ? jogo.title : ""

    // 0 esperando, 1 pausado, 2 salvando, 3 erro
    property int estado: 0
    property int passo: 0
    property string titulo: qsTr("Pedindo sua máquina…")
    property string detalhe: ""
    property bool jogou: false
    property bool encerrou: false
    property int reservaRestante: 180
    property int salvarTotal: 0
    property int salvarRestante: 0

    WarpSessao {
        id: sessao
        onEtapa: function (p, t, d) {
            tela.estado = 0
            tela.passo = p
            tela.titulo = t
            tela.detalhe = d
        }
        onProntoParaJogar: function (appName, session) {
            tela.jogou = true
            WarpApi.definir("jogado/" + tela.jogo.slug, Date.now())
            var componente = Qt.createComponent("qrc:/gui/StreamSegue.qml")
            var segue = componente.createObject(stackView, { "appName": appName, "session": session, "quitAfter": false })
            stackView.push(segue)
        }
        onFalhou: function (mensagem) {
            tela.estado = 3
            tela.titulo = qsTr("Não deu certo desta vez")
            tela.detalhe = mensagem
            tentar.forceActiveFocus()
        }
        onSalvando: function (segundos) {
            tela.estado = 2
            tela.salvarTotal = segundos
            tela.salvarRestante = segundos
            relogioSalvar.start()
        }
    }

    Component.onCompleted: sessao.iniciar(ComputerManager, jogo.slug)

    Component.onDestruction: {
        if (!tela.encerrou) sessao.encerrar()
    }

    // Voltou da tela do jogo: o menu da partida pode ter pedido qualidade
    // nova (reconecta na mesma máquina) ou o fim; senão, partida pausada.
    StackView.onActivated: {
        if (tela.jogou && tela.estado === 0) {
            var pedido = sessao.pedidoDoMenu()
            if (pedido === "reconectar") {
                tela.passo = 3
                tela.titulo = qsTr("Aplicando a nova qualidade…")
                tela.detalhe = qsTr("Reconectando na mesma máquina, sem fila.")
                Qt.callLater(function () { sessao.reabrir() })
                return
            }
            if (pedido === "encerrar") {
                tela.encerrarSessao()
                return
            }
            tela.estado = 1
            tela.titulo = qsTr("Partida pausada")
            tela.detalhe = qsTr("O jogo continua aberto na máquina, reservado para você.")
            tela.reservaRestante = 180
            relogioReserva.start()
            voltarAoJogo.forceActiveFocus()
        }
    }

    function encerrarSessao() {
        tela.encerrou = true
        relogioReserva.stop()
        tela.titulo = qsTr("Salvando seu progresso…")
        tela.detalhe = qsTr("A WARP está fechando o jogo e esperando a nuvem da loja terminar de enviar o save. Pode sair desta tela: a máquina só é liberada depois.")
        tela.estado = 2
        sessao.encerrar()
    }

    Timer {
        id: relogioReserva
        interval: 1000
        repeat: true
        onTriggered: {
            tela.reservaRestante--
            if (tela.reservaRestante <= 0) tela.encerrarSessao()
        }
    }
    Timer {
        id: relogioSalvar
        interval: 1000
        repeat: true
        onTriggered: {
            tela.salvarRestante--
            if (tela.salvarRestante <= 0) {
                stop()
                tela.titulo = qsTr("Pronto!")
                tela.detalhe = qsTr("O jogo foi fechado e a espera para o save subir terminou. Até a próxima!")
                sairDepois.start()
            }
        }
    }
    Timer { id: sairDepois; interval: 3000; onTriggered: stackView.pop() }

    Keys.onEscapePressed: {
        if (tela.estado === 1) tela.encerrarSessao()
        else stackView.pop()
    }

    Rectangle { anchors.fill: parent; color: "#07131F" }
    Image {
        anchors.fill: parent
        source: tela.jogo && tela.jogo.coverUrl ? tela.jogo.coverUrl : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 0.3
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#9907131F" }
            GradientStop { position: 1.0; color: "#FF07131F" }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, tela.width - 64)
        spacing: 12

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: tela.estado === 1 ? qsTr("Máquina reservada por mais %1:%2").arg(Math.floor(tela.reservaRestante / 60)).arg(("0" + tela.reservaRestante % 60).slice(-2))
                  : (tela.jogo ? tela.jogo.title.toUpperCase() : "")
            color: "#00C48E"; font.pixelSize: 13; font.bold: true; font.letterSpacing: 2
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: tela.titulo
            color: "#EAF3F8"; font.pixelSize: 30; font.bold: true; wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: tela.detalhe !== ""
            horizontalAlignment: Text.AlignHCenter
            text: tela.detalhe
            color: "#ADC4D6"; font.pixelSize: 15; wrapMode: Text.WordWrap
        }

        // Etapas: fila, máquina, acesso, jogo.
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 18
            visible: tela.estado === 0
            spacing: 18
            Repeater {
                model: [qsTr("Fila"), qsTr("Máquina"), qsTr("Acesso"), qsTr("Jogo")]
                Row {
                    spacing: 7
                    Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: index <= tela.passo ? "#00C48E" : "#55FFFFFF" }
                    Text { text: modelData; font.pixelSize: 13; font.bold: true; color: index < tela.passo ? "#00C48E" : index === tela.passo ? "#EAF3F8" : "#55FFFFFF" }
                }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 12
            running: tela.estado === 0
            visible: running
        }

        ProgressBar {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 320
            Layout.topMargin: 18
            visible: tela.estado === 1 || tela.estado === 2
            from: 0
            to: 1
            value: tela.estado === 1 ? tela.reservaRestante / 180 : (tela.salvarTotal > 0 ? 1 - tela.salvarRestante / tela.salvarTotal : 1)
        }

        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 24
            spacing: 12

            WarpBotao {
                id: voltarAoJogo
                visible: tela.estado === 1
                principal: true
                text: qsTr("▶  Voltar ao jogo")
                onClicked: {
                    relogioReserva.stop()
                    tela.estado = 0
                    tela.passo = 3
                    tela.titulo = qsTr("Voltando ao jogo…")
                    tela.detalhe = ""
                    sessao.reabrir()
                }
                KeyNavigation.right: encerrar
            }
            WarpBotao {
                id: encerrar
                visible: tela.estado === 1
                perigo: true
                text: qsTr("Encerrar sessão")
                onClicked: tela.encerrarSessao()
            }
            WarpBotao {
                id: tentar
                visible: tela.estado === 3
                principal: true
                text: qsTr("Tentar de novo")
                onClicked: stackView.replace(Qt.resolvedUrl("WarpSessaoView.qml"), { "jogo": tela.jogo })
            }
            WarpBotao {
                visible: tela.estado === 0 || tela.estado === 2 || tela.estado === 3
                text: tela.estado === 0 ? qsTr("Cancelar") : qsTr("Voltar aos jogos")
                onClicked: stackView.pop()
            }
        }
    }
}
