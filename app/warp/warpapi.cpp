#include "warpapi.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include <algorithm>
#include <memory>

const QString WarpApi::BASE = QStringLiteral("https://warpserver.com.br");

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
