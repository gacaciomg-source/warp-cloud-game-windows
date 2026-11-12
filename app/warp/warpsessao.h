#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class ComputerManager;
class NvComputer;

// WARP Cloud Game: do "Jogar" até o jogo aberto, sem IP nem código.
//
//  1. pede a sessão à WARP; se as máquinas estão ocupadas, mostra a fila;
//  2. com a máquina reservada, cadastra o endereço dela;
//  3. se o computador ainda não está pareado, inventa o código e a WARP o
//     entrega ao Sunshine da máquina (com novas tentativas e "destravar");
//  4. cria a transmissão do aplicativo cadastrado no painel.
//
// Enquanto a pessoa joga, manda sinal de vida. No "Encerrar", a WARP fecha o
// jogo e segura a máquina enquanto o save sobe para a nuvem da loja.
class WarpSessao : public QObject
{
    Q_OBJECT

public:
    explicit WarpSessao(QObject* pai = nullptr);
    ~WarpSessao() override;

    Q_INVOKABLE void iniciar(QObject* computerManager, const QString& slug);
    // Volta ao jogo (nova transmissão, mesma sessão, sem fila).
    Q_INVOKABLE void reabrir();
    // Fim com o tempo de salvar.
    Q_INVOKABLE void encerrar();

signals:
    // passo: 0 fila, 1 máquina, 2 acesso, 3 jogo.
    void etapa(int passo, QString titulo, QString detalhe);
    void prontoParaJogar(QString appName, QObject* session);
    void falhou(QString mensagem);
    void salvando(int segundos);

private:
    void consultar();
    void maquinaPronta(const QString& host, int porta, const QString& app, int tetoKbps);
    void achouComputador();
    void parear(int tentativa);
    void abrirJogo();
    void sinalDeVida();
    NvComputer* computador() const;
    void falhar(const QString& mensagem);

    QPointer<ComputerManager> m_Gerente;
    QString m_SessaoId;
    QString m_Endereco;
    QString m_App;
    int m_TetoKbps = 0;
    bool m_Encerrada = false;
    bool m_Jogando = false;
    QTimer m_Fila;
    QTimer m_Sinal;
    QTimer m_Prazo;
    QTimer m_Entrega;
    QString m_Pin;
    int m_Tentativa = 0;
    QMetaObject::Connection m_ConexaoPareamento;
    QMetaObject::Connection m_ConexaoEstado;
    QMetaObject::Connection m_ConexaoAdicao;
};
