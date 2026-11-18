#include "warpapi.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>

#include <algorithm>
#include <memory>

const QString WarpApi::BASE = QStringLiteral("https://warpserver.com.br");
const QString WarpApi::VERSAO = QStringLiteral("1.0.5");

static QString api(const QString& caminho)
{
    return WarpApi::BASE + QStringLiteral("/api/v1") + caminho;
}

/* ------------------------------------------------------------ cookies */

WarpCookies::WarpCookies(QObject* pai) : QNetworkCookieJar(pai)
{
    QSettings s;
    QList<QNetworkCookie> lista;
    for (const QVariant& v : s.value("warp/cookies").toList()) {
        lista.append(QNetworkCookie::parseCookies(v.toByteArray()));
    }
    setAllCookies(lista);
}

bool WarpCookies::setCookiesFromUrl(const QList<QNetworkCookie>& lista, const QUrl& url)
{
    bool r = QNetworkCookieJar::setCookiesFromUrl(lista, url);
    guardar();
    return r;
}

bool WarpCookies::temSessao() const
{
    const QDateTime agora = QDateTime::currentDateTimeUtc();
    for (const QNetworkCookie& c : allCookies()) {
        if (c.isSessionCookie() || c.expirationDate() > agora) return true;
    }
    return false;
}

void WarpCookies::limpar()
{
    setAllCookies({});
    guardar();
}

void WarpCookies::guardar()
{
    QVariantList lista;
    const QDateTime agora = QDateTime::currentDateTimeUtc();
    for (const QNetworkCookie& c : allCookies()) {
        if (!c.isSessionCookie() && c.expirationDate() <= agora) continue;
        lista.append(c.toRawForm(QNetworkCookie::Full));
    }
    QSettings s;
    s.setValue("warp/cookies", lista);
}

/* ---------------------------------------------------------------- API */

WarpApi* WarpApi::get()
{
    static WarpApi* instancia = new WarpApi();
    return instancia;
}

WarpApi::WarpApi(QObject* pai) : QObject(pai)
{
    m_Cookies = new WarpCookies(this);
    m_Rede.setCookieJar(m_Cookies);
    m_Cookies->setParent(this);
}

QString WarpApi::urlAbsoluta(const QString& url) const
{
    if (url.isEmpty()) return QString();
    return url.startsWith('/') ? BASE + url : url;
}

void WarpApi::chamar(const QString& metodo, const QString& caminho, const QJsonObject& corpo, Retorno retorno,
                     bool podeRenovar)
{
    QNetworkRequest req(QUrl(api(caminho)));
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(30000);
    QByteArray dados;
    if (!corpo.isEmpty() || metodo == "POST") {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        dados = corpo.isEmpty() ? QByteArray("{}") : QJsonDocument(corpo).toJson(QJsonDocument::Compact);
    }
    QNetworkReply* r = m_Rede.sendCustomRequest(req, metodo.toUtf8(), dados);
    connect(r, &QNetworkReply::finished, this, [=]() {
        r->deleteLater();
        Resposta resp;
        resp.status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonObject json = QJsonDocument::fromJson(r->readAll()).object();
        resp.corpo = json;

        if (resp.status == 401 && podeRenovar) {
            renovar([=](bool renovou) {
                if (renovou) {
                    chamar(metodo, caminho, corpo, retorno, false);
                } else {
                    Resposta falha;
                    falha.status = 401;
                    falha.mensagem = tr("Sua sessão expirou. Entre de novo.");
                    retorno(falha);
                }
            });
            return;
        }

        resp.ok = resp.status >= 200 && resp.status < 300;
        if (!resp.ok) {
            resp.codigo = json.value("code").toString();
            resp.mensagem = json.value("message").toString();
            if (resp.mensagem.isEmpty()) {
                resp.mensagem = resp.status == 0 ? tr("Sem conexão com a WARP. Confira a internet.")
                                                 : tr("Erro %1 na WARP.").arg(resp.status);
            }
        }
        retorno(resp);
    });
}

void WarpApi::renovar(std::function<void(bool)> retorno)
{
    chamar("POST", "/auth/refresh", {}, [=](const Resposta& r) { retorno(r.ok); }, false);
}

QVariant WarpApi::valor(const QString& chave, const QVariant& padrao) const
{
    return QSettings().value("warp/" + chave, padrao);
}

void WarpApi::definir(const QString& chave, const QVariant& valor)
{
    QSettings().setValue("warp/" + chave, valor);
}

/* ------------------------------------------------------------- conta */

void WarpApi::entrar(const QString& email, const QString& senha)
{
    QJsonObject corpo{{"email", email.trimmed()}, {"password", senha}};
    chamar("POST", "/auth/login", corpo, [=](const Resposta& r) {
        if (!r.ok) {
            emit falhouAoEntrar(r.status == 401 ? tr("E-mail ou senha incorretos.") : r.mensagem);
            return;
        }
        emit contaMudou();
        carregarConta();
        emit entrou();
    }, false);
}

void WarpApi::criarConta(const QString& nome, const QString& email, const QString& senha)
{
    QJsonObject corpo{{"name", nome.trimmed()}, {"email", email.trimmed()}, {"password", senha}};
    chamar("POST", "/auth/register", corpo, [=](const Resposta& r) {
        if (!r.ok) {
            // O erro de cada campo ("A senha precisa ter...") diz mais que "Dados inválidos."
            QStringList campos;
            const QJsonObject f = r.corpo.value("fields").toObject();
            for (const QString& k : f.keys()) {
                campos << f.value(k).toString();
            }
            emit falhouAoEntrar(campos.isEmpty() ? r.mensagem : campos.join(" "));
            return;
        }
        emit contaMudou();
        carregarConta();
        emit entrou();
    }, false);
}

void WarpApi::carregarConta()
{
    if (!logado()) return;
    chamar("GET", "/auth/me", {}, [=](const Resposta& r) {
        if (!r.ok) {
            if (r.status == 401) {
                m_Cookies->limpar();
                emit contaMudou();
            }
            return;
        }
        QJsonObject u = r.corpo.value("user").toObject();
        m_Nome = u.value("name").toString();
        m_Email = u.value("email").toString();
        emit contaMudou();
    });
}

void WarpApi::sairDaConta()
{
    chamar("POST", "/auth/logout", {}, [=](const Resposta&) {
        m_Cookies->limpar();
        m_Nome.clear();
        m_Email.clear();
        emit contaMudou();
    }, false);
}

/* ----------------------------------------------------------- catálogo */

void WarpApi::carregarJogos()
{
    // Primeiro a conta (renova o acesso vencido): é o login que faz o
    // administrador receber também os recursos só dele.
    chamar("GET", "/auth/me", {}, [=](const Resposta&) {
        chamar("GET", "/cloud-game/games", {}, [=](const Resposta& r) {
            if (!r.ok) {
                emit falhaNosJogos(r.mensagem);
                return;
            }
            QVariantList lista;
            for (const QJsonValue& v : r.corpo.value("games").toArray()) {
                QVariantMap j = v.toObject().toVariantMap();
                j["coverUrl"] = urlAbsoluta(j.value("coverUrl").toString());
                lista.append(j);
            }
            emit jogosCarregados(lista);
        });
    });
}

/* ------------------------------------------------- teste de conexão */

void WarpApi::medirRede(std::function<void(int, double)> retorno)
{
    // Ping: mediana de 5 idas e voltas (a primeira só abre a conexão segura).
    auto tempos = std::make_shared<QList<qint64>>();
    auto relogio = std::make_shared<QElapsedTimer>();
    auto proximo = std::make_shared<std::function<void(int)>>();
    *proximo = [=](int i) {
        relogio->start();
        QNetworkRequest req(QUrl(api("/cloud-game/rede?bytes=0")));
        req.setTransferTimeout(10000);
        QNetworkReply* r = m_Rede.get(req);
        connect(r, &QNetworkReply::finished, this, [=]() {
            r->deleteLater();
            if (i > 0) tempos->append(relogio->elapsed());
            if (i < 5 && r->error() == QNetworkReply::NoError) {
                (*proximo)(i + 1);
                return;
            }
            std::sort(tempos->begin(), tempos->end());
            int ping = tempos->isEmpty() ? 0 : int(tempos->at(tempos->size() / 2));

            // Velocidade: 8 MB, contando a partir dos primeiros 256 KB.
            QNetworkRequest req2(QUrl(api("/cloud-game/rede?bytes=8388608")));
            req2.setTransferTimeout(15000);
            QNetworkReply* d = m_Rede.get(req2);
            auto recebidos = std::make_shared<qint64>(0);
            auto contados = std::make_shared<qint64>(0);
            auto inicio = std::make_shared<QElapsedTimer>();
            connect(d, &QNetworkReply::readyRead, this, [=]() {
                qint64 n = d->readAll().size();
                *recebidos += n;
                if (!inicio->isValid()) {
                    if (*recebidos >= 256 * 1024) inicio->start();
                    return;
                }
                *contados += n;
                if (inicio->elapsed() > 4000) d->abort();
            });
            connect(d, &QNetworkReply::finished, this, [=]() {
                d->deleteLater();
                double mbps = 0;
                if (inicio->isValid() && inicio->elapsed() > 0 && *contados > 0) {
                    mbps = *contados * 8.0 / (inicio->elapsed() / 1000.0) / 1e6;
                }
                retorno(ping, mbps);
            });
        });
    };
    (*proximo)(0);
}

void WarpApi::testarConexao()
{
    medirRede([=](int ping, double mbps) {
        QSettings s;
        s.setValue("warp/rede/ping", ping);
        s.setValue("warp/rede/mbps", mbps);
        s.setValue("warp/rede/quando", QDateTime::currentSecsSinceEpoch());
        emit conexaoTestada(ping, mbps);
    });
}

/* ------------------------------------------------------- atualização */

// "1.0.10" é maior que "1.0.9": compara número por número.
static bool versaoMaior(const QString& a, const QString& b)
{
    const QStringList pa = a.split('.'), pb = b.split('.');
    for (int i = 0; i < qMax(pa.size(), pb.size()); i++) {
        int x = i < pa.size() ? pa[i].toInt() : 0;
        int y = i < pb.size() ? pb[i].toInt() : 0;
        if (x != y) return x > y;
    }
    return false;
}

// O portátil guarda as configurações ao lado do programa (portable.dat): não
// foi instalado pelo .msi, então não dá para atualizar instalando por cima.
static bool modoPortatil()
{
    return QFile(QDir::currentPath() + "/portable.dat").exists();
}

void WarpApi::procurarAtualizacao()
{
    chamar("GET", "/cloud-game/app/windows/info", {}, [=](const Resposta& r) {
        if (!r.ok || !r.corpo.value("disponivel").toBool()) return;
        const QString versao = r.corpo.value("versao").toString();
        if (!versaoMaior(versao, VERSAO)) return;
        m_VersaoNova = versao;
        m_Sha256Novo = r.corpo.value("sha256").toString().toLower();
        emit atualizacaoDisponivel(versao, r.corpo.value("tamanho").toDouble() / 1e6, !modoPortatil());
    }, false);
}

void WarpApi::atualizar()
{
    if (m_VersaoNova.isEmpty() || m_Baixando) return;
    if (modoPortatil()) {
        QDesktopServices::openUrl(QUrl(BASE + "/cloud-game"));
        return;
    }

    const QString destino = QDir::temp().filePath(QString("WARP-Cloud-Game-%1.msi").arg(m_VersaoNova));
    auto arquivo = std::make_shared<QFile>(destino);
    if (!arquivo->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit falhaNaAtualizacao(tr("Não deu para salvar o instalador no computador."));
        return;
    }
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    m_Baixando = true;

    QNetworkRequest pedido(QUrl(api("/cloud-game/app/windows")));
    pedido.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = m_Rede.get(pedido);
    connect(r, &QNetworkReply::readyRead, this, [=]() {
        const QByteArray parte = r->readAll();
        hash->addData(parte);
        arquivo->write(parte);
    });
    connect(r, &QNetworkReply::downloadProgress, this, [=](qint64 recebido, qint64 total) {
        if (total > 0) emit progressoDaAtualizacao(double(recebido) / total);
    });
    connect(r, &QNetworkReply::finished, this, [=]() {
        r->deleteLater();
        m_Baixando = false;
        const QByteArray resto = r->readAll();
        hash->addData(resto);
        arquivo->write(resto);
        arquivo->close();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError || status != 200) {
            QFile::remove(destino);
            emit falhaNaAtualizacao(tr("O download parou no meio. Confira a internet e tente de novo."));
            return;
        }
        if (!m_Sha256Novo.isEmpty() && QString::fromLatin1(hash->result().toHex()) != m_Sha256Novo) {
            QFile::remove(destino);
            emit falhaNaAtualizacao(tr("O instalador baixado veio diferente do publicado. Tente de novo."));
            return;
        }
        instalar(destino);
    });
}

// Roda o instalador e fecha o app (os arquivos dele vão ser trocados). Um
// script espera o instalador terminar e abre o app de novo, já atualizado. O
// Windows pede a permissão de administrador, como em qualquer instalação.
void WarpApi::instalar(const QString& msi)
{
    const QString programa = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString script = QDir::temp().filePath("warp-atualizar.cmd");
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit falhaNaAtualizacao(tr("Não deu para preparar a instalação."));
        return;
    }
    const QString nl = QStringLiteral("\r\n");
    const QString texto = QStringLiteral("@echo off") + nl + QStringLiteral("chcp 65001 >nul") + nl +
            QString("msiexec /i \"%1\" /passive /norestart").arg(QDir::toNativeSeparators(msi)) + nl +
            QString("start \"\" \"%1\"").arg(programa) + nl +
            QString("del \"%1\"").arg(QDir::toNativeSeparators(msi)) + nl;
    f.write(texto.toUtf8());
    f.close();

    QProcess p;
    p.setProgram("cmd.exe");
    p.setArguments({ "/c", QDir::toNativeSeparators(script) });
#ifdef Q_OS_WIN32
    // CREATE_NO_WINDOW: sem a janela preta do cmd.
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a) { a->flags |= 0x08000000; });
#endif
    if (!p.startDetached()) {
        emit falhaNaAtualizacao(tr("Não deu para abrir o instalador."));
        return;
    }
    QCoreApplication::quit();
}
