import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import WarpApi 1.0

// Início: o jogo em destaque no topo, com a arte de fundo, e as fileiras de
// jogos ("Continue jogando", "Jogue agora", "Em breve" e, para o
// administrador, "Administração"). O destaque segue o jogo com foco.
Item {
    id: inicio
    property bool warp: true
    objectName: "Início"

    property var jogos: []
    property var fileiras: []
    property var destaque: null
    property string aviso: ""
    property bool carregando: true

    function jogado(slug) {
        return WarpApi.valor("jogado/" + slug, 0) > 0
    }

    function montar(lista) {
        var continuar = [], agora = [], breve = [], admin = []
        for (var i = 0; i < lista.length; i++) {
            var j = lista[i]
            if (j.soAdmin) { admin.push(j); continue }
            if (!j.jogavel) { breve.push(j); continue }
            agora.push(j)
            if (jogado(j.slug)) continuar.push(j)
        }
        var f = []
        if (continuar.length) f.push({ titulo: qsTr("Continue jogando"), itens: continuar })
        if (agora.length) f.push({ titulo: qsTr("Jogue agora"), itens: agora })
        if (breve.length) f.push({ titulo: qsTr("Em breve"), itens: breve })
        if (admin.length) f.push({ titulo: qsTr("Administração (só você vê)"), itens: admin })
        fileiras = f
        // O destaque continua no mesmo jogo depois de recarregar a lista.
        var mesmo = null
        for (var k = 0; destaque && k < lista.length; k++) if (lista[k].slug === destaque.slug) mesmo = lista[k]
        destaque = mesmo ? mesmo : continuar.length ? continuar[0] : agora.length ? agora[0] : (lista.length ? lista[0] : null)
        aviso = lista.length ? "" : qsTr("Nenhum jogo liberado ainda. Volte em breve.")
    }

    function abrir(jogo) {
        stackView.push(Qt.resolvedUrl("WarpJogo.qml"), { "jogo": jogo })
    }

    function jogar(jogo) {
        if (!jogo || !jogo.jogavel) return
        stackView.push(Qt.resolvedUrl("WarpSessaoView.qml"), { "jogo": jogo })
    }

    Connections {
        target: WarpApi
        function onJogosCarregados(lista) {
            inicio.carregando = false
            inicio.jogos = lista
            inicio.montar(lista)
        }
        function onFalhaNosJogos(mensagem) {
            inicio.carregando = false
            if (!inicio.jogos.length) inicio.aviso = mensagem
        }
        function onContaMudou() {
            if (!WarpApi.logado) stackView.replace(Qt.resolvedUrl("WarpEntrar.qml"))
        }
    }

    StackView.onActivated: {
        if (!WarpApi.logado) {
            stackView.replace(Qt.resolvedUrl("WarpEntrar.qml"))
            return
        }
        WarpApi.carregarJogos()
        if (destaqueJogar.visible) destaqueJogar.forceActiveFocus()
    }

    Rectangle { anchors.fill: parent; color: "#07131F" }

    // Arte do destaque, atrás de tudo.
    Item {
        id: arte
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.max(360, Math.min(560, inicio.height * 0.66))
        y: -rolagem.contentY * 0.45

        Rectangle { anchors.fill: parent; color: inicio.destaque && inicio.destaque.accentColor ? inicio.destaque.accentColor : "#15304A" }
        Image {
            anchors.fill: parent
            source: inicio.destaque && inicio.destaque.coverUrl ? inicio.destaque.coverUrl : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: status === Image.Ready ? 0.9 : 0
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#F207131F" }
                GradientStop { position: 0.5; color: "#B307131F" }
                GradientStop { position: 1.0; color: "#0007131F" }
            }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#CC07131F" }
                GradientStop { position: 0.25; color: "#0007131F" }
                GradientStop { position: 0.7; color: "#6607131F" }
                GradientStop { position: 1.0; color: "#FF07131F" }
            }
        }
    }

    Flickable {
        id: rolagem
        anchors.fill: parent
        contentHeight: conteudo.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: conteudo
            width: rolagem.width
            spacing: 0

            // Barra de cima.
            RowLayout {
                width: parent.width
                height: 76
                Item { width: 40 }
                Column {
                    Text { text: "WARP"; color: "#EAF3F8"; font.pixelSize: 22; font.bold: true; font.letterSpacing: 3 }
                    Text { text: "Cloud Game"; color: "#ADC4D6"; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                WarpBotao {
                    text: qsTr("Configurações")
                    onClicked: stackView.push(Qt.resolvedUrl("WarpConfig.qml"))
                }
                WarpBotao {
                    text: WarpApi.nome !== "" ? WarpApi.nome : qsTr("Conta")
                    onClicked: contaMenu.open()
                    Menu {
                        id: contaMenu
                        y: parent.height
                        MenuItem { text: WarpApi.email !== "" ? WarpApi.email : qsTr("Conectado à WARP"); enabled: false }
                        MenuItem { text: qsTr("Sair da conta"); onTriggered: WarpApi.sairDaConta() }
                    }
                }
                Item { width: 40 }
            }

            // Destaque.
            Item {
                width: parent.width
                height: arte.height - 76 - 10
                visible: inicio.destaque !== null

                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 48
                    anchors.bottomMargin: 26
                    width: Math.min(640, parent.width - 96)
                    spacing: 10

                    Text {
                        text: !inicio.destaque ? "" : inicio.destaque.soAdmin ? qsTr("SÓ ADMINISTRADOR")
                            : !inicio.destaque.jogavel ? qsTr("EM BREVE NA WARP")
                            : inicio.jogado(inicio.destaque.slug) ? qsTr("CONTINUE DE ONDE PAROU") : qsTr("EM DESTAQUE")
                        color: "#00C48E"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 2
                    }
                    Text {
                        width: parent.width
                        text: inicio.destaque ? inicio.destaque.title : ""
                        color: "#EAF3F8"; font.pixelSize: 46; font.bold: true
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }
                    Text {
                        text: inicio.destaque ? [inicio.destaque.genre, inicio.destaque.store].filter(function (x) { return x }).join("  ·  ") : ""
                        color: "#ADC4D6"; font.pixelSize: 15; font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: inicio.destaque && inicio.destaque.description ? inicio.destaque.description
                              : qsTr("Jogue direto da nuvem da WARP, sem instalar nada. Seu progresso fica salvo na sua conta da loja.")
                        color: "#ADC4D6"; font.pixelSize: 15
                        wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
                    }
                    Row {
                        spacing: 12
                        topPadding: 10
                        WarpBotao {
                            id: destaqueJogar
                            principal: true
                            visible: inicio.destaque && inicio.destaque.jogavel
                            text: inicio.destaque && inicio.jogado(inicio.destaque.slug) ? qsTr("▶  Continuar jogando") : qsTr("▶  Jogar")
                            onClicked: inicio.jogar(inicio.destaque)
                            KeyNavigation.right: detalhes
                            Keys.onDownPressed: if (listas.count) listas.itemAt(0).focar()
                        }
                        WarpBotao {
                            id: detalhes
                            text: qsTr("Detalhes")
                            onClicked: inicio.abrir(inicio.destaque)
                            Keys.onDownPressed: if (listas.count) listas.itemAt(0).focar()
                        }
                    }
                }
            }

            // Fileiras.
            Repeater {
                id: listas
                model: inicio.fileiras

                Column {
                    id: fileira
                    width: conteudo.width
                    spacing: 10
                    topPadding: 18

                    function focar() { lista.forceActiveFocus(); rolagem.contentY = Math.max(0, Math.min(fileira.y - 120, rolagem.contentHeight - rolagem.height)) }

                    Text {
                        leftPadding: 48
                        text: modelData.titulo
                        color: "#EAF3F8"; font.pixelSize: 19; font.bold: true
                    }
                    ListView {
                        id: lista
                        width: parent.width
                        height: 250
                        orientation: ListView.Horizontal
                        spacing: 18
                        leftMargin: 48
                        rightMargin: 48
                        clip: false
                        model: modelData.itens
                        keyNavigationEnabled: true
                        highlightFollowsCurrentItem: true
                        delegate: WarpCartao {
                            y: 12
                            jogo: modelData
                            focus: ListView.isCurrentItem
                            onEscolhido: inicio.abrir(modelData)
                            onGanhouFoco: { inicio.destaque = modelData; lista.currentIndex = index }
                        }
                        Keys.onUpPressed: {
                            if (index > 0) listas.itemAt(index - 1).focar()
                            else { rolagem.contentY = 0; destaqueJogar.visible ? destaqueJogar.forceActiveFocus() : detalhes.forceActiveFocus() }
                        }
                        Keys.onDownPressed: if (index < listas.count - 1) listas.itemAt(index + 1).focar()
                    }
                }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: inicio.carregando && inicio.jogos.length === 0
    }

    Text {
        anchors.centerIn: parent
        visible: inicio.aviso !== "" && !inicio.carregando
        text: inicio.aviso
        color: "#ADC4D6"
        font.pixelSize: 16
    }
}
