#pragma once

#include <QByteArray>
#include <QSet>
#include <QString>

#include "SDL_compat.h"
#include <SDL_ttf.h>

class Session;

// Qualidades da WARP (as mesmas da tela Configurações). A primeira é a
// automática, que a WARP calcula pelo teste de conexão.
struct PerfilWarp {
    const char* id;
    const char* nome;
    int altura;
    int kbps;
};
extern const PerfilWarp PERFIS_WARP[];
extern const int QUANTOS_PERFIS_WARP;

// Menu da partida no PC: abre por cima do jogo, sem desconectar, com
// Ctrl + Shift + M (ou Start + Select + LB + RB no controle). Enquanto está
// aberto, o teclado, o mouse e o controle ficam com ele e não chegam ao jogo.
//
// Estatísticas mudam na hora. Qualidade nova precisa de uma conexão nova: o
// menu fecha a transmissão e pede à tela da sessão para reconectar na mesma
// máquina (sem fila), como o app do Android.
//
// É desenhado no aviso de baixo do motor (o mesmo das mensagens de conexão),
// numa imagem própria com fundo, porque o Qt fica parado durante o jogo.
class WarpMenuDoJogo
{
public:
    explicit WarpMenuDoJogo(Session* sessao);
    ~WarpMenuDoJogo();

    // Chamado para cada evento do SDL antes do motor; true = o menu usou.
    bool tratarEvento(const SDL_Event& evento);

    void abrir();
    void fechar();
    bool aberto() const { return m_Aberto; }

    // O aviso de baixo está com a WARP (menu ou dica): as mensagens do motor esperam.
    bool usaAviso() const { return m_Aberto || m_DicaVisivel; }

    // Dica do atalho nos primeiros segundos da partida.
    void mostrarDica();
    void esconderDica();

    // Depois que o motor recria a imagem (troca de janela, placa), o desenho some.
    void redesenhar();

    // O que a tela da sessão faz quando a transmissão termina: "" (pausa),
    // "reconectar" (qualidade nova) ou "encerrar". Lê e limpa.
    static QString consumirPedido();

private:
    void desenharMenu();
    void desenharDica();
    void mostrar(SDL_Surface* imagem);
    void mover(int passo);
    void mudarQualidade(int passo);
    void executar();
    void sair(const char* pedido);
    bool abrirFontes();
    void fecharFontes();
    TTF_Font* fonte(const QString& arquivo, int tamanho);
    double escala();

    Session* m_Sessao;
    bool m_Aberto;
    bool m_DicaVisivel;
    int m_Item;
    int m_Qualidade;
    int m_QualidadeAtual;
    SDL_TimerID m_TimerDica;
    QSet<int> m_TeclasEngolidas;

    int m_TamanhoFontes;
    TTF_Font* m_Texto;
    TTF_Font* m_Forte;
    TTF_Font* m_Pequena;
    QByteArray m_FonteReserva;

    static QString s_Pedido;
};
