#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QObject>
#include <QVariant>

#include <functional>

// WARP Cloud Game: conversa com a API da WARP (conta, catálogo, sessões).
//
// A API autentica por cookie, o mesmo do site. Os cookies ficam guardados no
// computador (QSettings): a pessoa entra uma vez e continua logada; quando o
// acesso de 15 minutos vence, a classe renova sozinha e repete o pedido.
class WarpCookies : public QNetworkCookieJar
{
public:
    explicit WarpCookies(QObject* pai);
    bool setCookiesFromUrl(const QList<QNetworkCookie>& lista, const QUrl& url) override;
    bool temSessao() const;
    void limpar();

private:
    void guardar();
};

class WarpApi : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool logado READ logado NOTIFY contaMudou)
    Q_PROPERTY(QString nome READ nome NOTIFY contaMudou)
    Q_PROPERTY(QString email READ email NOTIFY contaMudou)
    Q_PROPERTY(QString versao READ versao CONSTANT)

public:
    static WarpApi* get();

    static const QString BASE;

    // Versão da WARP deste programa: a mesma publicada no site (app.json). O
    // número interno do motor (version.txt) não muda e não aparece para ninguém.
    static const QString VERSAO;

    // Resposta de um pedido: ok, status HTTP, corpo em JSON e a mensagem de erro.
    struct Resposta
    {
        bool ok = false;
        int status = 0;
        QJsonObject corpo;
        QString codigo;
        QString mensagem;
    };
    using Retorno = std::function<void(const Resposta&)>;

    void chamar(const QString& metodo, const QString& caminho, const QJsonObject& corpo, Retorno retorno,
                bool podeRenovar = true);

    bool logado() const { return m_Cookies->temSessao(); }
    QString nome() const { return m_Nome; }
    QString email() const { return m_Email; }
    QString versao() const { return VERSAO; }

    // Atualização: confere no site se há versão nova e, se a pessoa aceitar,
    // baixa o instalador, confere o SHA-256 e instala por cima (o app fecha e
    // abre de novo sozinho). O portátil só leva para a página de download.
    Q_INVOKABLE void procurarAtualizacao();
    Q_INVOKABLE void atualizar();

    // Para as telas (QML).
    Q_INVOKABLE void entrar(const QString& email, const QString& senha);
    // Cadastro pelo próprio app (a mesma rota do site; já volta logado).
    Q_INVOKABLE void criarConta(const QString& nome, const QString& email, const QString& senha);
    Q_INVOKABLE void carregarConta();
    Q_INVOKABLE void carregarJogos();
    Q_INVOKABLE void sairDaConta();
    Q_INVOKABLE void testarConexao();
    Q_INVOKABLE QString urlAbsoluta(const QString& url) const;

    // Preferências da WARP guardadas no computador (qualidade, jogos já jogados…).
    Q_INVOKABLE QVariant valor(const QString& chave, const QVariant& padrao = QVariant()) const;
    Q_INVOKABLE void definir(const QString& chave, const QVariant& valor);

    // Teste de rede (ping em ms e velocidade em Mbps), para a qualidade automática.
    void medirRede(std::function<void(int pingMs, double mbps)> retorno);

signals:
    void entrou();
    void falhouAoEntrar(QString mensagem);
    void contaMudou();
    void jogosCarregados(QVariantList jogos);
    void falhaNosJogos(QString mensagem);
    void conexaoTestada(int pingMs, double mbps);
    void atualizacaoDisponivel(QString versao, double megabytes, bool automatica);
    void progressoDaAtualizacao(double fracao);
    void falhaNaAtualizacao(QString mensagem);

private:
    explicit WarpApi(QObject* pai = nullptr);
    void renovar(std::function<void(bool)> retorno);
    void instalar(const QString& msi);

    QNetworkAccessManager m_Rede;
    WarpCookies* m_Cookies;
    QString m_Nome, m_Email;
    QString m_VersaoNova, m_Sha256Novo;
    bool m_Baixando = false;
};
