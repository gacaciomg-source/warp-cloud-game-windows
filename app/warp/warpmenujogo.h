#pragma once

#include <QByteArray>
#include <QSet>
#include <QString>
#include <QVector>

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
// É desenhado na camada de avisos do motor (a das mensagens de conexão), numa
// imagem do tamanho da janela: o jogo escurecido atrás e um cartão flutuando no
// meio, como no celular. O Qt fica parado durante o jogo, então o desenho é
// feito à mão (SDL_ttf + a fonte e os ícones do próprio Windows). Com o menu
// aberto o mouse fica solto: dá para clicar nos itens.
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
    void aplicarQualidade();
    void sair(const char* pedido);
    bool tratarMouse(const SDL_Event& evento);
    bool abrirFontes();
    void fecharFontes();
    TTF_Font* fonte(const QString& arquivo, int tamanho);
    void tamanhoDaTela(int& largura, int& altura);
    double escala();

    Session* m_Sessao;
    bool m_Aberto;
    bool m_DicaVisivel;
    int m_Item;
    int m_Qualidade;
    int m_QualidadeAtual;
    SDL_TimerID m_TimerDica;
    QSet<int> m_TeclasEngolidas;
    // O mouse estava preso no jogo antes de abrir (volta assim ao fechar).
    bool m_CapturaAntes;
    // O clique que fechou o menu: o "soltar" dele não vai para o jogo.
    bool m_EngolirSoltar;

    // Onde ficou cada parte do menu na tela, em pixels, para o mouse.
    QVector<SDL_Rect> m_AreaItens;
    QVector<SDL_Rect> m_AreaQualidades;
    SDL_Rect m_AreaCartao;
    SDL_Rect m_AreaAplicar;
    SDL_Rect m_AreaFechar;

    int m_TamanhoFontes;
    TTF_Font* m_Titulo;
    TTF_Font* m_Texto;
    TTF_Font* m_Forte;
    TTF_Font* m_Pequena;
    TTF_Font* m_Icones;
    QByteArray m_FonteReserva;

    static QString s_Pedido;
};
