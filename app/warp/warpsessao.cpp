#include "warpsessao.h"
#include "warpapi.h"

#include "backend/computermanager.h"
#include "backend/nvcomputer.h"
#include "backend/nvpairingmanager.h"
#include "settings/streamingpreferences.h"
#include "streaming/session.h"

#include <QJsonObject>
#include <QSettings>

#include <algorithm>

static const int TENTATIVAS_DE_PAREAR = 3;
static const int PRAZO_PAREAR_MS = 20000;

WarpSessao::WarpSessao(QObject* pai) : QObject(pai)
{
    m_Fila.setInterval(3000);
    connect(&m_Fila, &QTimer::timeout, this, &WarpSessao::consultar);
    m_Sinal.setInterval(20000);
    connect(&m_Sinal, &QTimer::timeout, this, &WarpSessao::sinalDeVida);
    m_Prazo.setSingleShot(true);
    m_Entrega.setInterval(1200);
}

WarpSessao::~WarpSessao()
{
    // Sem "Encerrar" (fechou o app): a WARP encerra sozinha depois de 2 min sem sinal.
    disconnect(m_ConexaoPareamento);
    disconnect(m_ConexaoEstado);
    disconnect(m_ConexaoAdicao);
}

void WarpSessao::falhar(const QString& mensagem)
{
    m_Fila.stop();
    m_Prazo.stop();
    m_Entrega.stop();
    emit falhou(mensagem);
}

/* --------------------------------------------------------- fila */

void WarpSessao::iniciar(QObject* computerManager, const QString& slug)
{
    m_Gerente = qobject_cast<ComputerManager*>(computerManager);
    if (!m_Gerente) {
        falhar(tr("Erro interno: gerenciador de conexões ausente."));
        return;
    }
    emit etapa(0, tr("Pedindo sua máquina…"), QString());
    WarpApi::get()->chamar("POST", "/cloud-game/sessions", QJsonObject{{"jogo", slug}},
                           [=, eu = QPointer<WarpSessao>(this)](const WarpApi::Resposta& r) {
        if (!eu) return;
        if (!r.ok) {
            falhar(r.mensagem);
            return;
        }
        m_SessaoId = r.corpo.value("id").toString();
        m_Sinal.start();
        consultar();
    });
}

void WarpSessao::consultar()
{
    if (m_SessaoId.isEmpty() || m_Encerrada) return;
    WarpApi::get()->chamar("GET", "/cloud-game/sessions/" + m_SessaoId, {}, [=, eu = QPointer<WarpSessao>(this)](const WarpApi::Resposta& r) {
        if (!eu) return;
        if (!r.ok) {
            falhar(r.mensagem);
            return;
        }
        const QString status = r.corpo.value("status").toString();
        if (status == "queued") {
            int pos = r.corpo.value("posicao").toInt();
            QString detalhe = r.corpo.value("liberaEm").isNull()
                ? tr("As máquinas estão ocupadas. Deixe o app aberto: quando uma liberar, o jogo abre sozinho.")
                : tr("Uma máquina está terminando de salvar a partida de quem acabou de sair. Libera em instantes.");
            emit etapa(0, pos <= 1 ? tr("Você é o próximo da fila") : tr("Você é o %1º da fila").arg(pos), detalhe);
            if (!m_Fila.isActive()) m_Fila.start();
            return;
        }
        m_Fila.stop();
        if (status == "ended") {
            falhar(r.corpo.value("motivoDoFim").toString(tr("A sessão foi encerrada.")));
            return;
        }
        QJsonObject maquina = r.corpo.value("maquina").toObject();
        QJsonObject jogo = r.corpo.value("jogo").toObject();
        maquinaPronta(maquina.value("host").toString(), maquina.value("porta").toInt(47989),
                      jogo.value("appNoSunshine").toString(), maquina.value("bitrateMaximoKbps").toInt(0));
    });
}

/* ------------------------------------------------------- máquina */

void WarpSessao::maquinaPronta(const QString& host, int porta, const QString& app, int tetoKbps)
{
    m_App = app;
    m_TetoKbps = tetoKbps;
    m_Endereco = host + ":" + QString::number(porta);
    emit etapa(1, tr("Conectando à máquina…"), QString());

    if (computador() != nullptr) {
        achouComputador();
        return;
    }
    m_ConexaoAdicao = connect(m_Gerente, &ComputerManager::computerAddCompleted, this,
                              [=](QVariant sucesso, QVariant) {
        disconnect(m_ConexaoAdicao);
        if (!sucesso.toBool() || computador() == nullptr) {
            falhar(tr("A máquina não respondeu. Tente de novo em instantes."));
            return;
        }
        achouComputador();
    });
    m_Gerente->addNewHostManually(m_Endereco);
}

NvComputer* WarpSessao::computador() const
{
    if (!m_Gerente) return nullptr;
    const QString host = m_Endereco.section(':', 0, 0);
    const int porta = m_Endereco.section(':', 1, 1).toInt();
    for (NvComputer* c : m_Gerente->getComputers()) {
        // uniqueAddresses() já trava o computador para ler.
        for (const NvAddress& a : c->uniqueAddresses()) {
            if (a.address() == host && a.port() == porta) return c;
        }
    }
    return nullptr;
}

// Toda sessão nova pareia de novo: no fim de cada sessão a WARP desfaz o
// pareamento no Sunshine (ninguém fica com acesso direto à máquina), mas o
// computador guardado aqui ainda "lembra" que estava pareado — confiar nisso
// fazia o app pular o pareamento e a máquina recusar ("não respondeu").
// "Voltar ao jogo" (mesma sessão) não passa por aqui.
void WarpSessao::achouComputador()
{
    parear(1);
}

/* ------------------------------------------------------ pareamento */

// Pareamento sem código: o computador começa a parear e fica esperando o
// código no Sunshine; em paralelo, a WARP entrega o código (o Sunshine só
// aceita depois que o pedido chegou, então a entrega insiste). Se uma tentativa
// anterior ficou pendurada, a WARP destrava a máquina e tenta de novo.
void WarpSessao::parear(int tentativa)
{
    NvComputer* c = computador();
    if (c == nullptr || m_Encerrada) return;
    m_Tentativa = tentativa;
    emit etapa(2, tentativa == 1 ? tr("Liberando seu acesso…") : tr("Liberando seu acesso (tentativa %1)…").arg(tentativa),
               tr("A WARP faz isso por você, sem código."));

    NvPairingManager::definirNomeDoAparelho("warp-" + m_SessaoId.left(8));
    m_Pin = m_Gerente->generatePinString();

    // Resultado do pareamento (do lado do computador).
    disconnect(m_ConexaoPareamento);
    m_ConexaoPareamento = connect(m_Gerente, &ComputerManager::pairingCompleted, this,
                                  [=](NvComputer* computador, QString erro) {
        if (computador != this->computador()) return;
        disconnect(m_ConexaoPareamento);
        m_Prazo.stop();
        m_Entrega.stop();
        if (erro.isEmpty()) {
            abrirJogo();
            return;
        }
        if (m_Tentativa < TENTATIVAS_DE_PAREAR) {
            emit etapa(2, tr("Destravando a máquina…"), tr("Uma tentativa anterior ficou pela metade. Leva alguns segundos."));
            WarpApi::get()->chamar("POST", "/cloud-game/sessions/" + m_SessaoId + "/destravar", {},
                                   [=, eu = QPointer<WarpSessao>(this)](const WarpApi::Resposta&) { if (eu) parear(m_Tentativa + 1); });
        } else {
            falhar(tr("Não foi possível liberar o acesso à máquina. Tente de novo em instantes."));
        }
    });

    // Entrega do código pela WARP, insistindo enquanto o Sunshine não está esperando.
    m_Entrega.disconnect();
    connect(&m_Entrega, &QTimer::timeout, this, [=]() {
        m_Entrega.stop();
        WarpApi::get()->chamar("POST", "/cloud-game/sessions/" + m_SessaoId + "/pin", QJsonObject{{"pin", m_Pin}},
                               [=, eu = QPointer<WarpSessao>(this)](const WarpApi::Resposta& r) {
        if (!eu) return;
            if (r.ok) return;
            if (r.codigo == "PAREAMENTO_NAO_INICIADO" || r.status == 0) {
                m_Entrega.start();
                return;
            }
            if (r.codigo == "SUNSHINE_SENHA" || r.codigo == "SUNSHINE_SEM_SENHA" || r.codigo == "MAQUINA_EM_USO") {
                falhar(r.mensagem);
            }
        });
    });
    m_Entrega.start();

    // Prazo: pendurou, destrava e tenta de novo.
    m_Prazo.disconnect();
    connect(&m_Prazo, &QTimer::timeout, this, [=]() {
        m_Entrega.stop();
        disconnect(m_ConexaoPareamento);
        if (m_Tentativa < TENTATIVAS_DE_PAREAR) {
            WarpApi::get()->chamar("POST", "/cloud-game/sessions/" + m_SessaoId + "/destravar", {},
                                   [=, eu = QPointer<WarpSessao>(this)](const WarpApi::Resposta&) { if (eu) parear(m_Tentativa + 1); });
        } else {
            falhar(tr("Não foi possível liberar o acesso à máquina. Tente de novo em instantes."));
        }
    });
    m_Prazo.start(PRAZO_PAREAR_MS);

    m_Gerente->pairHost(c, m_Pin);
}

/* ------------------------------------------------------------ jogo */

// Fluidez: o ritmo de quadros (frame pacing) segura um quadro para entregar
// a imagem em intervalos iguais — some o engasgo pequeno que a internet
// causa, ao custo de ~1 quadro de atraso. Padrão da WARP: ligado.
//
// Qualidade automática: 70% da velocidade medida até a WARP, entre 4 e 40
// Mbps, respeitando o teto da máquina. 1080p a partir de 12 Mbps.
static void aplicarQualidade(StreamingPreferences* p, int tetoKbps)
{
    QSettings s;
    p->framePacing = s.value("warp/fluido", true).toBool();
    p->enableVsync = true;
    // Tela cheia sem borda: o monitor não troca de resolução ao entrar e sair
    // do jogo (sem piscar preto) e o Windows compõe a imagem sem rasgar.
    p->windowMode = StreamingPreferences::WM_FULLSCREEN_DESKTOP;
    // Alt+Tab e a tecla Windows ficam no PC do jogador (como em qualquer jogo):
    // mandados para a máquina, tiravam o jogo da frente e mostravam a Steam.
    p->captureSysKeysMode = StreamingPreferences::CSK_OFF;
    const QString escolhida = s.value("warp/qualidade", "auto").toString();
    for (int i = 1; i < QUANTOS_PERFIS_WARP; i++) {
        if (escolhida == PERFIS_WARP[i].id) {
            const PerfilWarp& f = PERFIS_WARP[i];
            p->width = f.altura * 16 / 9;
            p->height = f.altura;
            p->fps = 60;
            p->bitrateKbps = tetoKbps > 0 ? std::min(f.kbps, tetoKbps) : f.kbps;
            return;
        }
    }
    if (escolhida != "auto") return;
    double mbps = s.value("warp/rede/mbps", 0).toDouble();
    int ping = s.value("warp/rede/ping", 0).toInt();
    int kbps = mbps > 0 ? int(mbps * 700) : 15000;
    kbps = std::max(4000, std::min(40000, kbps));
    if (ping >= 80) kbps = std::min(kbps, 20000);
    if (tetoKbps > 0) kbps = std::min(kbps, tetoKbps);
    kbps = (kbps / 500) * 500;
    p->bitrateKbps = kbps;
    if (kbps >= 12000) {
        p->width = 1920;
        p->height = 1080;
    } else {
        p->width = 1280;
        p->height = 720;
    }
    p->fps = 60;
}

void WarpSessao::abrirJogo()
{
    NvComputer* c = computador();
    if (c == nullptr || m_Encerrada) return;
    emit etapa(3, tr("Abrindo o jogo…"), QString());

    // A lista de aplicativos chega logo depois do pareamento: espera por ela.
    NvApp app;
    bool achou = false;
    {
        QReadLocker trava(&c->lock);
        for (const NvApp& a : c->appList) {
            if (a.name == m_App || (m_App.isEmpty() && !achou)) {
                app = a;
                achou = true;
                if (a.name == m_App) break;
            }
        }
    }
    if (!achou) {
        disconnect(m_ConexaoEstado);
        m_ConexaoEstado = connect(m_Gerente, &ComputerManager::computerStateChanged, this, [=](NvComputer* mudou) {
            if (mudou != computador()) return;
            QReadLocker trava(&mudou->lock);
            bool tem = false;
            for (const NvApp& a : mudou->appList) tem = tem || a.name == m_App || m_App.isEmpty();
            if (!tem) return;
            trava.unlock();
            disconnect(m_ConexaoEstado);
            abrirJogo();
        });
        QTimer::singleShot(20000, this, [=]() {
            if (m_ConexaoEstado) {
                disconnect(m_ConexaoEstado);
                falhar(tr("A máquina não tem o aplicativo \"%1\" configurado.").arg(m_App));
            }
        });
        return;
    }

    StreamingPreferences* prefs = StreamingPreferences::get();
    aplicarQualidade(prefs, m_TetoKbps);
    m_Jogando = true;
    Session* sessao = new Session(c, app, prefs);
    emit prontoParaJogar(app.name, sessao);
}

void WarpSessao::reabrir()
{
    abrirJogo();
}

QString WarpSessao::pedidoDoMenu()
{
    return WarpMenuDoJogo::consumirPedido();
}

/* ----------------------------------------------------- sinal e fim */

void WarpSessao::sinalDeVida()
{
    if (m_SessaoId.isEmpty() || m_Encerrada) return;
    WarpApi::get()->chamar("GET", "/cloud-game/sessions/" + m_SessaoId + (m_Jogando ? "?jogando=1" : ""), {},
                           [](const WarpApi::Resposta&) {});
}

void WarpSessao::encerrar()
{
    if (m_Encerrada) return;
    m_Encerrada = true;
    m_Fila.stop();
    m_Sinal.stop();
    m_Prazo.stop();
    m_Entrega.stop();
    if (m_SessaoId.isEmpty()) {
        emit salvando(0);
        return;
    }
    WarpApi::get()->chamar("POST", "/cloud-game/sessions/" + m_SessaoId + "/end", {}, [=, eu = QPointer<WarpSessao>(this)](const WarpApi::Resposta& r) {
        if (!eu) return;
        int segundos = r.ok && !r.corpo.value("salvandoAte").isNull() ? r.corpo.value("salvarSegundos").toInt() : 0;
        emit salvando(segundos);
    });
}
