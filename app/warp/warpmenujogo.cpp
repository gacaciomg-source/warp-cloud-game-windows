#include "warpmenujogo.h"

#include <QFile>
#include <QSettings>
#include <QStringList>

#include "path.h"
#include "streaming/session.h"
#include "streaming/input/input.h"

// Código do evento que esconde a dica do atalho (fora da faixa do motor).
#define SDL_CODE_WARP_DICA 150

const PerfilWarp PERFIS_WARP[] = {
    { "auto", "Automática", 0, 0 },
    { "economica", "Econômica", 720, 6000 },
    { "equilibrada", "Equilibrada", 1080, 15000 },
    { "alta", "Alta", 1080, 25000 },
    { "maxima", "Máxima", 1080, 40000 },
};
const int QUANTOS_PERFIS_WARP = int(sizeof(PERFIS_WARP) / sizeof(PERFIS_WARP[0]));

QString WarpMenuDoJogo::s_Pedido;

enum ItemDoMenu {
    ItemVoltar,
    ItemQualidade,
    ItemEstatisticas,
    ItemPausar,
    ItemEncerrar,
    QuantosItens
};

/* ------------------------------------------------------------ desenho */

static const SDL_Color COR_VERDE = { 0, 196, 142, 255 };
static const SDL_Color COR_BRANCO = { 234, 243, 248, 255 };
static const SDL_Color COR_TEXTO = { 173, 196, 214, 255 };
static const SDL_Color COR_APAGADO = { 111, 139, 163, 255 };

static SDL_Surface* novaImagem(int largura, int altura)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, largura, altura, 32, SDL_PIXELFORMAT_ARGB8888);
    if (s != nullptr) {
        SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, 0, 0, 0, 0));
    }
    return s;
}

static void retangulo(SDL_Surface* s, int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Rect q = { x, y, w, h };
    SDL_FillRect(s, &q, SDL_MapRGBA(s->format, r, g, b, a));
}

static int larguraDoTexto(TTF_Font* f, const QString& texto)
{
    int w = 0, h = 0;
    if (f != nullptr) {
        TTF_SizeUTF8(f, texto.toUtf8().constData(), &w, &h);
    }
    return w;
}

static void escrever(SDL_Surface* destino, TTF_Font* f, const QString& texto, SDL_Color cor, int x, int y)
{
    if (f == nullptr || texto.isEmpty()) {
        return;
    }
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, texto.toUtf8().constData(), cor);
    if (s == nullptr) {
        return;
    }
    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_BLEND);
    SDL_Rect onde = { x, y, s->w, s->h };
    SDL_BlitSurface(s, nullptr, destino, &onde);
    SDL_FreeSurface(s);
}

static Uint32 SDLCALL fimDaDica(Uint32, void*)
{
    SDL_Event e = {};
    e.type = SDL_USEREVENT;
    e.user.code = SDL_CODE_WARP_DICA;
    SDL_PushEvent(&e);
    return 0;
}

/* ------------------------------------------------------------ menu */

WarpMenuDoJogo::WarpMenuDoJogo(Session* sessao)
    : m_Sessao(sessao),
      m_Aberto(false),
      m_DicaVisivel(false),
      m_Item(0),
      m_Qualidade(0),
      m_QualidadeAtual(0),
      m_TimerDica(0),
      m_TamanhoFontes(0),
      m_Texto(nullptr),
      m_Forte(nullptr),
      m_Pequena(nullptr)
{
}

WarpMenuDoJogo::~WarpMenuDoJogo()
{
    if (m_TimerDica != 0) {
        SDL_RemoveTimer(m_TimerDica);
    }
    fecharFontes();
}

QString WarpMenuDoJogo::consumirPedido()
{
    QString pedido = s_Pedido;
    s_Pedido.clear();
    return pedido;
}

double WarpMenuDoJogo::escala()
{
    int w = 0, h = 0;
    if (m_Sessao->m_Window != nullptr) {
#if SDL_VERSION_ATLEAST(2, 26, 0)
        SDL_GetWindowSizeInPixels(m_Sessao->m_Window, &w, &h);
#else
        SDL_GetWindowSize(m_Sessao->m_Window, &w, &h);
#endif
    }
    if (h <= 0) {
        h = 1080;
    }
    return qBound(0.6, h / 1080.0, 2.0);
}

// Segoe UI, a fonte do Windows (acentos e setas). Sem ela, a do motor.
TTF_Font* WarpMenuDoJogo::fonte(const QString& arquivo, int tamanho)
{
    QString caminho = qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/" + arquivo;
    TTF_Font* f = QFile::exists(caminho) ? TTF_OpenFont(caminho.toUtf8().constData(), tamanho) : nullptr;
    if (f == nullptr) {
        if (m_FonteReserva.isEmpty()) {
            m_FonteReserva = Path::readDataFile("ModeSeven.ttf");
        }
        if (!m_FonteReserva.isEmpty()) {
            f = TTF_OpenFontRW(SDL_RWFromConstMem(m_FonteReserva.constData(), m_FonteReserva.size()), 1, tamanho);
        }
    }
    return f;
}

bool WarpMenuDoJogo::abrirFontes()
{
    int base = qRound(24 * escala());
    if (m_Texto != nullptr && base == m_TamanhoFontes) {
        return true;
    }
    fecharFontes();
    m_TamanhoFontes = base;
    m_Texto = fonte("segoeui.ttf", base);
    m_Forte = fonte("segoeuib.ttf", qRound(base * 0.78));
    m_Pequena = fonte("segoeui.ttf", qRound(base * 0.68));
    return m_Texto != nullptr && m_Forte != nullptr && m_Pequena != nullptr;
}

void WarpMenuDoJogo::fecharFontes()
{
    for (TTF_Font** f : { &m_Texto, &m_Forte, &m_Pequena }) {
        if (*f != nullptr) {
            TTF_CloseFont(*f);
            *f = nullptr;
        }
    }
    m_TamanhoFontes = 0;
}

void WarpMenuDoJogo::mostrar(SDL_Surface* imagem)
{
    if (imagem != nullptr) {
        m_Sessao->m_OverlayManager.definirImagem(Overlay::OverlayStatusUpdate, imagem);
    }
}

void WarpMenuDoJogo::abrir()
{
    if (m_Aberto) {
        return;
    }

    QSettings s;
    QString atual = s.value("warp/qualidade", "auto").toString();
    m_QualidadeAtual = 0;
    for (int i = 0; i < QUANTOS_PERFIS_WARP; i++) {
        if (atual == PERFIS_WARP[i].id) {
            m_QualidadeAtual = i;
        }
    }
    m_Qualidade = m_QualidadeAtual;
    m_Item = ItemVoltar;
    m_Aberto = true;
    m_DicaVisivel = false;

    // O jogo não pode ficar com tecla ou botão "presos" enquanto o menu está aberto.
    m_Sessao->m_InputHandler->raiseAllKeys();
    m_Sessao->m_InputHandler->zerarControles();

    desenharMenu();
}

void WarpMenuDoJogo::fechar()
{
    if (!m_Aberto) {
        return;
    }
    m_Aberto = false;
    m_Sessao->m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, false);
    m_Sessao->m_InputHandler->zerarControles();
}

void WarpMenuDoJogo::mostrarDica()
{
    m_DicaVisivel = true;
    desenharDica();
    if (m_TimerDica != 0) {
        SDL_RemoveTimer(m_TimerDica);
    }
    m_TimerDica = SDL_AddTimer(8000, fimDaDica, nullptr);
}

void WarpMenuDoJogo::esconderDica()
{
    m_TimerDica = 0;
    if (!m_DicaVisivel) {
        return;
    }
    m_DicaVisivel = false;
    if (!m_Aberto) {
        m_Sessao->m_OverlayManager.setOverlayState(Overlay::OverlayStatusUpdate, false);
    }
}

void WarpMenuDoJogo::redesenhar()
{
    if (m_Aberto) {
        desenharMenu();
    }
    else if (m_DicaVisivel) {
        desenharDica();
    }
}

void WarpMenuDoJogo::mover(int passo)
{
    m_Item = (m_Item + passo + QuantosItens) % QuantosItens;
    desenharMenu();
}

void WarpMenuDoJogo::mudarQualidade(int passo)
{
    m_Item = ItemQualidade;
    m_Qualidade = (m_Qualidade + passo + QUANTOS_PERFIS_WARP) % QUANTOS_PERFIS_WARP;
    desenharMenu();
}

void WarpMenuDoJogo::executar()
{
    switch (m_Item) {
    case ItemVoltar:
        fechar();
        break;
    case ItemQualidade:
        if (m_Qualidade != m_QualidadeAtual) {
            QSettings s;
            s.setValue("warp/qualidade", PERFIS_WARP[m_Qualidade].id);
            sair("reconectar");
        }
        else {
            mudarQualidade(1);
        }
        break;
    case ItemEstatisticas: {
        Overlay::OverlayManager& o = m_Sessao->m_OverlayManager;
        o.setOverlayState(Overlay::OverlayDebug, !o.isOverlayEnabled(Overlay::OverlayDebug));
        desenharMenu();
        break;
    }
    case ItemPausar:
        sair("");
        break;
    case ItemEncerrar:
        sair("encerrar");
        break;
    }
}

void WarpMenuDoJogo::sair(const char* pedido)
{
    s_Pedido = pedido;
    fechar();

    SDL_Event e = {};
    e.type = SDL_QUIT;
    e.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&e);
}

bool WarpMenuDoJogo::tratarEvento(const SDL_Event& e)
{
    switch (e.type) {
    case SDL_USEREVENT:
        if (e.user.code == SDL_CODE_WARP_DICA) {
            esconderDica();
            return true;
        }
        return false;

    case SDL_KEYDOWN: {
        const SDL_Keysym& k = e.key.keysym;
        bool atalho = k.scancode == SDL_SCANCODE_M &&
                (k.mod & KMOD_CTRL) && (k.mod & KMOD_SHIFT) && !(k.mod & KMOD_ALT);
        if (!m_Aberto) {
            if (!atalho || e.key.repeat) {
                return false;
            }
            m_TeclasEngolidas.insert(k.scancode);
            abrir();
            return true;
        }

        m_TeclasEngolidas.insert(k.scancode);
        if (atalho) {
            if (!e.key.repeat) {
                fechar();
            }
            return true;
        }
        switch (k.scancode) {
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_W:
            mover(-1);
            break;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_S:
            mover(1);
            break;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_A:
            mudarQualidade(-1);
            break;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_D:
            mudarQualidade(1);
            break;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
        case SDL_SCANCODE_SPACE:
            if (!e.key.repeat) {
                executar();
            }
            break;
        case SDL_SCANCODE_ESCAPE:
            if (!e.key.repeat) {
                fechar();
            }
            break;
        default:
            break;
        }
        return true;
    }

    case SDL_KEYUP:
        // A tecla que o menu usou não chega solta ao jogo (nem depois de fechar).
        if (m_TeclasEngolidas.remove(e.key.keysym.scancode)) {
            return true;
        }
        return m_Aberto;

    case SDL_CONTROLLERBUTTONDOWN:
        if (!m_Aberto) {
            return false;
        }
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            mover(-1);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            mover(1);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            mudarQualidade(-1);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            mudarQualidade(1);
            break;
        case SDL_CONTROLLER_BUTTON_A:
            executar();
            break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_START:
            fechar();
            break;
        default:
            break;
        }
        return true;

    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERAXISMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEMOTION:
    case SDL_MOUSEWHEEL:
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
        return m_Aberto;

    default:
        return false;
    }
}

/* ------------------------------------------------------------ telas */

void WarpMenuDoJogo::desenharMenu()
{
    if (!abrirFontes()) {
        return;
    }
    const double e = escala();
    auto px = [e](double v) { return qRound(v * e); };

    const int margem = px(32);
    const int largura = px(780);
    const int folga = px(28);
    const int linha = px(54);

    // Textos de cada linha (rótulo à esquerda, valor à direita).
    const STREAM_CONFIGURATION& agora = m_Sessao->m_StreamConfig;
    const PerfilWarp& p = PERFIS_WARP[m_Qualidade];
    QString qualidade = QString::fromUtf8(p.nome);
    if (p.altura > 0) {
        qualidade += QString(" · %1p, %2 Mbps").arg(p.altura).arg(p.kbps / 1000);
    }
    else if (m_Qualidade == m_QualidadeAtual) {
        qualidade += QString(" · agora %1p, %2 Mbps").arg(agora.height).arg(qRound(agora.bitrate / 1000.0));
    }
    bool estatisticas = m_Sessao->m_OverlayManager.isOverlayEnabled(Overlay::OverlayDebug);

    const QStringList rotulos = {
        "Voltar ao jogo",
        "Qualidade da imagem",
        "Estatísticas na tela",
        "Pausar e ir para o app",
        "Encerrar a sessão",
    };
    const QStringList valores = {
        "Esc",
        "‹  " + qualidade + "  ›",
        estatisticas ? "Ligadas" : "Desligadas",
        "a máquina fica reservada",
        "salva o jogo e libera a máquina",
    };
    bool qualidadeNova = m_Qualidade != m_QualidadeAtual;

    const int alturaTitulo = TTF_FontHeight(m_Forte);
    const int alturaPequena = TTF_FontHeight(m_Pequena);
    int alturaPainel = folga + alturaTitulo + px(4) + alturaPequena + px(16) + linha * QuantosItens +
            px(16) + alturaPequena + (qualidadeNova ? alturaPequena + px(6) : 0) + folga;

    SDL_Surface* s = novaImagem(margem + largura, alturaPainel + margem);
    if (s == nullptr) {
        return;
    }

    // Painel escuro com a faixa verde da WARP em cima.
    retangulo(s, margem, 0, largura, alturaPainel, 10, 26, 41, 238);
    retangulo(s, margem, 0, largura, px(4), 0, 196, 142, 255);

    int y = folga;
    escrever(s, m_Forte, "WARP  ·  MENU DA PARTIDA", COR_VERDE, margem + folga, y);
    y += alturaTitulo + px(4);
    escrever(s, m_Pequena, QString("%1 continua rodando enquanto o menu está aberto.").arg(m_Sessao->m_App.name),
             COR_APAGADO, margem + folga, y);
    y += alturaPequena + px(16);

    const int alturaTexto = TTF_FontHeight(m_Texto);
    for (int i = 0; i < QuantosItens; i++) {
        bool escolhido = i == m_Item;
        if (escolhido) {
            retangulo(s, margem + px(12), y, largura - px(24), linha, 8, 60, 61, 245);
            retangulo(s, margem + px(12), y, px(5), linha, 0, 196, 142, 255);
        }
        int yTexto = y + (linha - alturaTexto) / 2;
        escrever(s, m_Texto, rotulos[i], escolhido ? COR_BRANCO : COR_TEXTO, margem + folga + px(8), yTexto);

        TTF_Font* fonteValor = i == ItemQualidade ? m_Texto : m_Pequena;
        int yValor = y + (linha - TTF_FontHeight(fonteValor)) / 2;
        SDL_Color corValor = escolhido ? COR_VERDE : COR_APAGADO;
        if (i == ItemQualidade && qualidadeNova) {
            corValor = COR_VERDE;
        }
        escrever(s, fonteValor, valores[i], corValor,
                 margem + largura - folga - larguraDoTexto(fonteValor, valores[i]), yValor);
        y += linha;
    }

    y += px(16);
    if (qualidadeNova) {
        escrever(s, m_Pequena, "Enter aplica a nova qualidade: a imagem volta em poucos segundos, sem fila.",
                 COR_VERDE, margem + folga, y);
        y += alturaPequena + px(6);
    }
    escrever(s, m_Pequena, "↑ ↓  escolher     ← →  qualidade     Enter  confirmar     Esc  voltar ao jogo",
             COR_APAGADO, margem + folga, y);

    mostrar(s);
}

void WarpMenuDoJogo::desenharDica()
{
    if (!abrirFontes()) {
        return;
    }
    const double e = escala();
    auto px = [e](double v) { return qRound(v * e); };

    const int margem = px(32);
    const int folga = px(20);
    const QString atalho = "Menu da partida:  Ctrl + Shift + M";
    const QString controle = "No controle: Start + Select + LB + RB";

    const int largura = qMax(larguraDoTexto(m_Texto, atalho), larguraDoTexto(m_Pequena, controle)) + folga * 2 + px(6);
    const int altura = folga + TTF_FontHeight(m_Texto) + px(2) + TTF_FontHeight(m_Pequena) + folga;

    SDL_Surface* s = novaImagem(margem + largura, altura + margem);
    if (s == nullptr) {
        return;
    }
    retangulo(s, margem, 0, largura, altura, 10, 26, 41, 225);
    retangulo(s, margem, 0, px(5), altura, 0, 196, 142, 255);

    int y = folga;
    escrever(s, m_Texto, atalho, COR_BRANCO, margem + folga + px(6), y);
    y += TTF_FontHeight(m_Texto) + px(2);
    escrever(s, m_Pequena, controle, COR_APAGADO, margem + folga + px(6), y);

    mostrar(s);
}
