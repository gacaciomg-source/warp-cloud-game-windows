#include "warpmenujogo.h"

#include <cmath>

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QStringList>
#include <QSvgRenderer>

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
static const SDL_Color COR_ESCURO = { 7, 19, 31, 255 };
static const SDL_Color COR_VERMELHO = { 255, 122, 122, 255 };

static const SDL_Color FUNDO_TELA = { 3, 10, 18, 150 };
static const SDL_Color FUNDO_CARTAO = { 11, 25, 38, 248 };
static const SDL_Color BORDA_CARTAO = { 34, 58, 78, 255 };
static const SDL_Color FUNDO_ESCOLHIDO = { 0, 196, 142, 34 };
static const SDL_Color FUNDO_ICONE = { 24, 44, 60, 255 };
static const SDL_Color FUNDO_TECLA = { 30, 50, 68, 255 };
static const SDL_Color FUNDO_OPCAO = { 20, 38, 53, 255 };

// Ícones da fonte Segoe MDL2 Assets, que vem no Windows 10 e 11.
static const char32_t ICONE_JOGAR = 0xE768;
static const char32_t ICONE_QUALIDADE = 0xE9E9;
static const char32_t ICONE_ESTATISTICAS = 0xE9D9;
static const char32_t ICONE_PAUSAR = 0xE769;
static const char32_t ICONE_ENCERRAR = 0xE7E8;
static const char32_t ICONE_FECHAR = 0xE711;

static SDL_Surface* novaImagem(int largura, int altura)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, largura, altura, 32, SDL_PIXELFORMAT_ARGB8888);
    if (s != nullptr) {
        SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, 0, 0, 0, 0));
    }
    return s;
}

// Pinta um retângulo de cantos redondos por cima do que já está na imagem,
// misturando pela transparência, com a borda dos cantos suavizada.
static void pintar(SDL_Surface* s, int x, int y, int w, int h, int raio, SDL_Color cor)
{
    if (s == nullptr || w <= 0 || h <= 0) {
        return;
    }
    raio = qMax(0, qMin(raio, qMin(w, h) / 2));
    SDL_LockSurface(s);
    for (int j = qMax(0, y); j < qMin(s->h, y + h); j++) {
        Uint32* linha = reinterpret_cast<Uint32*>(static_cast<Uint8*>(s->pixels) + j * s->pitch);
        for (int i = qMax(0, x); i < qMin(s->w, x + w); i++) {
            double cobre = 1.0;
            double cx = i + 0.5, cy = j + 0.5;
            double dx = 0, dy = 0;
            if (cx < x + raio) {
                dx = x + raio - cx;
            }
            else if (cx > x + w - raio) {
                dx = cx - (x + w - raio);
            }
            if (cy < y + raio) {
                dy = y + raio - cy;
            }
            else if (cy > y + h - raio) {
                dy = cy - (y + h - raio);
            }
            if (dx > 0 && dy > 0) {
                cobre = qBound(0.0, raio - std::sqrt(dx * dx + dy * dy) + 0.5, 1.0);
                if (cobre <= 0) {
                    continue;
                }
            }
            const double a = cor.a / 255.0 * cobre;
            const Uint32 p = linha[i];
            const double da = ((p >> 24) & 0xFF) / 255.0;
            const double oa = a + da * (1 - a);
            auto canal = [&](int desloca, Uint8 c) -> Uint32 {
                double dc = ((p >> desloca) & 0xFF) / 255.0;
                double oc = oa > 0 ? (c / 255.0 * a + dc * da * (1 - a)) / oa : 0;
                return Uint32(qBound(0, qRound(oc * 255), 255)) << desloca;
            };
            linha[i] = (Uint32(qRound(oa * 255)) << 24) | canal(16, cor.r) | canal(8, cor.g) | canal(0, cor.b);
        }
    }
    SDL_UnlockSurface(s);
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

// Texto centrado verticalmente numa faixa (y até y + altura).
static void escreverNoMeio(SDL_Surface* destino, TTF_Font* f, const QString& texto, SDL_Color cor,
                           int x, int y, int altura)
{
    if (f != nullptr) {
        escrever(destino, f, texto, cor, x, y + (altura - TTF_FontHeight(f)) / 2);
    }
}

static QString icone(char32_t codigo)
{
    // Os ícones usados ficam todos no primeiro plano do Unicode (um QChar só).
    return QString(QChar(static_cast<ushort>(codigo)));
}

// Tecla desenhada ("Esc", "Enter"...). Devolve a largura.
static int tecla(SDL_Surface* s, TTF_Font* f, const QString& texto, int x, int y, int altura)
{
    int largura = qMax(altura, larguraDoTexto(f, texto) + altura * 2 / 3);
    pintar(s, x, y, largura, altura, altura / 4, FUNDO_TECLA);
    escreverNoMeio(s, f, texto, COR_TEXTO, x + (largura - larguraDoTexto(f, texto)) / 2, y, altura);
    return largura;
}

// Chave liga/desliga, como a do celular.
static void chave(SDL_Surface* s, bool ligada, int x, int y, int largura, int altura)
{
    pintar(s, x, y, largura, altura, altura / 2, ligada ? COR_VERDE : SDL_Color { 52, 72, 90, 255 });
    int bola = altura - altura / 4;
    int folga = (altura - bola) / 2;
    pintar(s, ligada ? x + largura - folga - bola : x + folga, y + folga, bola, bola, bola / 2,
           ligada ? COR_BRANCO : SDL_Color { 190, 205, 215, 255 });
}

// Símbolo da WARP (o mesmo do site e do ícone do programa).
static void logo(SDL_Surface* s, int x, int y, int tamanho)
{
    QSvgRenderer svg(QString(":/res/warp.svg"));
    QImage imagem(tamanho, tamanho, QImage::Format_RGBA8888);
    imagem.fill(0);
    QPainter pintor(&imagem);
    svg.render(&pintor);
    pintor.end();
    SDL_Surface* l = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uchar*>(imagem.constBits()), imagem.width(),
                                                        imagem.height(), 32, imagem.bytesPerLine(),
                                                        SDL_PIXELFORMAT_RGBA32);
    if (l != nullptr) {
        SDL_SetSurfaceBlendMode(l, SDL_BLENDMODE_BLEND);
        SDL_Rect onde = { x, y, tamanho, tamanho };
        SDL_BlitSurface(l, nullptr, s, &onde);
        SDL_FreeSurface(l);
    }
}

static bool dentro(const SDL_Rect& r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
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
      m_CapturaAntes(false),
      m_EngolirSoltar(false),
      m_AreaCartao {},
      m_AreaAplicar {},
      m_AreaFechar {},
      m_TamanhoFontes(0),
      m_Titulo(nullptr),
      m_Texto(nullptr),
      m_Forte(nullptr),
      m_Pequena(nullptr),
      m_Icones(nullptr)
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

void WarpMenuDoJogo::tamanhoDaTela(int& largura, int& altura)
{
    largura = altura = 0;
    if (m_Sessao->m_Window != nullptr) {
#if SDL_VERSION_ATLEAST(2, 26, 0)
        SDL_GetWindowSizeInPixels(m_Sessao->m_Window, &largura, &altura);
#else
        SDL_GetWindowSize(m_Sessao->m_Window, &largura, &altura);
#endif
    }
}

double WarpMenuDoJogo::escala()
{
    int w, h;
    tamanhoDaTela(w, h);
    if (h <= 0) {
        h = 1080;
    }
    return qBound(0.6, h / 1080.0, 2.0);
}

// Fontes do Windows (Segoe UI com acentos; ícones da Segoe MDL2). Sem elas, a do motor.
TTF_Font* WarpMenuDoJogo::fonte(const QString& arquivo, int tamanho)
{
    QString caminho = qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/" + arquivo;
    TTF_Font* f = QFile::exists(caminho) ? TTF_OpenFont(caminho.toUtf8().constData(), tamanho) : nullptr;
    if (f == nullptr && arquivo != "segmdl2.ttf") {
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
    m_Titulo = fonte("segoeuib.ttf", qRound(base * 1.0));
    m_Texto = fonte("segoeui.ttf", qRound(base * 0.84));
    m_Forte = fonte("segoeuib.ttf", qRound(base * 0.68));
    m_Pequena = fonte("segoeui.ttf", qRound(base * 0.64));
    m_Icones = fonte("segmdl2.ttf", qRound(base * 0.8));
    return m_Titulo != nullptr && m_Texto != nullptr && m_Forte != nullptr && m_Pequena != nullptr;
}

void WarpMenuDoJogo::fecharFontes()
{
    for (TTF_Font** f : { &m_Titulo, &m_Texto, &m_Forte, &m_Pequena, &m_Icones }) {
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

    // Solta o mouse para clicar no menu; volta a prender ao fechar.
    m_CapturaAntes = m_Sessao->m_InputHandler->isCaptureActive();
    if (m_CapturaAntes) {
        m_Sessao->m_InputHandler->setCaptureActive(false);
    }

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
    if (m_CapturaAntes) {
        m_Sessao->m_InputHandler->setCaptureActive(true);
        m_CapturaAntes = false;
    }
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

void WarpMenuDoJogo::aplicarQualidade()
{
    if (m_Qualidade == m_QualidadeAtual) {
        return;
    }
    QSettings s;
    s.setValue("warp/qualidade", PERFIS_WARP[m_Qualidade].id);
    sair("reconectar");
}

void WarpMenuDoJogo::executar()
{
    switch (m_Item) {
    case ItemVoltar:
        fechar();
        break;
    case ItemQualidade:
        if (m_Qualidade != m_QualidadeAtual) {
            aplicarQualidade();
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

// Mouse com o menu aberto: passar por cima escolhe, clicar confirma, clicar
// fora do cartão volta ao jogo.
bool WarpMenuDoJogo::tratarMouse(const SDL_Event& e)
{
    if (!m_Aberto) {
        if (e.type == SDL_MOUSEBUTTONUP && m_EngolirSoltar) {
            m_EngolirSoltar = false;
            return true;
        }
        return false;
    }
    if (e.type != SDL_MOUSEMOTION && e.type != SDL_MOUSEBUTTONDOWN) {
        return true;
    }

    // O SDL dá a posição em pontos da janela; o desenho é em pixels.
    int pontosW = 0, pontosH = 0, pixelsW = 0, pixelsH = 0;
    SDL_GetWindowSize(m_Sessao->m_Window, &pontosW, &pontosH);
    tamanhoDaTela(pixelsW, pixelsH);
    double fator = pontosW > 0 ? double(pixelsW) / pontosW : 1.0;
    int x = e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x;
    int y = e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y;
    x = qRound(x * fator);
    y = qRound(y * fator);

    if (e.type == SDL_MOUSEMOTION) {
        for (int i = 0; i < m_AreaItens.size(); i++) {
            if (dentro(m_AreaItens[i], x, y) && m_Item != i) {
                m_Item = i;
                desenharMenu();
                break;
            }
        }
        return true;
    }

    if (e.button.button != SDL_BUTTON_LEFT) {
        return true;
    }
    if (dentro(m_AreaFechar, x, y) || !dentro(m_AreaCartao, x, y)) {
        m_EngolirSoltar = true;
        fechar();
        return true;
    }
    if (m_Qualidade != m_QualidadeAtual && dentro(m_AreaAplicar, x, y)) {
        aplicarQualidade();
        return true;
    }
    for (int i = 0; i < m_AreaQualidades.size(); i++) {
        if (dentro(m_AreaQualidades[i], x, y)) {
            m_Item = ItemQualidade;
            m_Qualidade = i;
            desenharMenu();
            return true;
        }
    }
    for (int i = 0; i < m_AreaItens.size(); i++) {
        if (dentro(m_AreaItens[i], x, y)) {
            m_Item = i;
            if (i == ItemQualidade) {
                // A linha da qualidade só escolhe; quem aplica é o botão.
                desenharMenu();
            }
            else {
                m_EngolirSoltar = true;
                executar();
            }
            return true;
        }
    }
    return true;
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

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        return tratarMouse(e);

    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERAXISMOTION:
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

    int telaW, telaH;
    tamanhoDaTela(telaW, telaH);
    if (telaW <= 0 || telaH <= 0) {
        return;
    }

    // Medidas do cartão.
    const int largura = qMin(px(640), telaW - px(32));
    const int folga = px(28);
    const int linha = px(58);
    const int recuo = px(56);              // onde começa o texto dos itens (depois do ícone)
    const int alturaOpcoes = px(36);
    const int faixaOpcoes = alturaOpcoes + px(12);
    const int alturaCabecalho = px(46);
    const int alturaRodape = px(28);
    const int separador = px(14);

    int alturaItens = 0;
    for (int i = 0; i < QuantosItens; i++) {
        alturaItens += linha + (i == ItemQualidade ? faixaOpcoes : 0);
    }
    const int altura = folga + alturaCabecalho + px(20) + alturaItens + separador + px(18) + alturaRodape + folga;

    const int x0 = (telaW - largura) / 2;
    const int y0 = qMax(0, (telaH - altura) / 2);

    SDL_Surface* s = novaImagem(telaW, telaH);
    if (s == nullptr) {
        return;
    }

    // O jogo escurecido atrás e o cartão no meio, com uma borda fina.
    SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, FUNDO_TELA.r, FUNDO_TELA.g, FUNDO_TELA.b, FUNDO_TELA.a));
    pintar(s, x0 - 1, y0 - 1, largura + 2, altura + 2, px(22) + 1, BORDA_CARTAO);
    pintar(s, x0, y0, largura, altura, px(22), FUNDO_CARTAO);
    m_AreaCartao = { x0, y0, largura, altura };

    // Cabeçalho: símbolo, título, nome do jogo e o X para fechar.
    int y = y0 + folga;
    const int tamanhoLogo = alturaCabecalho;
    logo(s, x0 + folga, y, tamanhoLogo);
    const int xTitulo = x0 + folga + tamanhoLogo + px(14);
    escrever(s, m_Titulo, "Menu da partida", COR_BRANCO, xTitulo, y - px(4));
    escrever(s, m_Pequena, QString("%1 continua rodando").arg(m_Sessao->m_App.name), COR_APAGADO, xTitulo,
             y + TTF_FontHeight(m_Titulo) - px(6));

    const int tamanhoX = px(36);
    m_AreaFechar = { x0 + largura - folga - tamanhoX, y + (alturaCabecalho - tamanhoX) / 2, tamanhoX, tamanhoX };
    pintar(s, m_AreaFechar.x, m_AreaFechar.y, tamanhoX, tamanhoX, tamanhoX / 2, FUNDO_ICONE);
    if (m_Icones != nullptr) {
        QString fecharIcone = icone(ICONE_FECHAR);
        int w = larguraDoTexto(m_Icones, fecharIcone);
        escreverNoMeio(s, m_Icones, fecharIcone, COR_TEXTO, m_AreaFechar.x + (tamanhoX - w) / 2, m_AreaFechar.y, tamanhoX);
    }
    y += alturaCabecalho + px(20);

    // Itens.
    const STREAM_CONFIGURATION& agora = m_Sessao->m_StreamConfig;
    const bool estatisticas = m_Sessao->m_OverlayManager.isOverlayEnabled(Overlay::OverlayDebug);
    const bool qualidadeNova = m_Qualidade != m_QualidadeAtual;
    const char32_t icones[QuantosItens] = { ICONE_JOGAR, ICONE_QUALIDADE, ICONE_ESTATISTICAS, ICONE_PAUSAR, ICONE_ENCERRAR };
    const QStringList rotulos = {
        "Voltar ao jogo",
        "Qualidade da imagem",
        "Estatísticas na tela",
        "Pausar e ir para o app",
        "Encerrar a sessão",
    };

    m_AreaItens.clear();
    m_AreaQualidades.clear();
    m_AreaAplicar = {};
    const int xItem = x0 + px(14);
    const int larguraItem = largura - px(28);
    const int alturaTexto = TTF_FontHeight(m_Texto);

    for (int i = 0; i < QuantosItens; i++) {
        if (i == ItemEncerrar) {
            pintar(s, x0 + folga, y + separador / 2, largura - folga * 2, qMax(1, px(1)), 0, BORDA_CARTAO);
            y += separador;
        }

        const bool escolhido = i == m_Item;
        const int alturaDoItem = linha + (i == ItemQualidade ? faixaOpcoes : 0);
        if (escolhido) {
            pintar(s, xItem, y, larguraItem, alturaDoItem, px(14), FUNDO_ESCOLHIDO);
        }
        m_AreaItens.append({ xItem, y, larguraItem, alturaDoItem });

        // Ícone num círculo.
        const SDL_Color corDoItem = i == ItemEncerrar ? COR_VERMELHO : (escolhido ? COR_VERDE : COR_TEXTO);
        const int bolinha = px(38);
        const int xBolinha = xItem + px(12);
        const int yBolinha = y + (linha - bolinha) / 2;
        pintar(s, xBolinha, yBolinha, bolinha, bolinha, bolinha / 2, FUNDO_ICONE);
        if (m_Icones != nullptr) {
            QString ic = icone(icones[i]);
            escreverNoMeio(s, m_Icones, ic, corDoItem, xBolinha + (bolinha - larguraDoTexto(m_Icones, ic)) / 2,
                           yBolinha, bolinha);
        }

        // Rótulo.
        const int xTexto = xItem + recuo + px(8);
        escrever(s, m_Texto, rotulos[i], i == ItemEncerrar ? COR_VERMELHO : (escolhido ? COR_BRANCO : COR_TEXTO),
                 xTexto, y + (linha - alturaTexto) / 2);

        // Lado direito de cada item.
        const int direita = xItem + larguraItem - px(16);
        switch (i) {
        case ItemVoltar: {
            int w = larguraDoTexto(m_Pequena, "Esc") + px(26);
            tecla(s, m_Pequena, "Esc", direita - w, y + (linha - px(28)) / 2, px(28));
            break;
        }
        case ItemQualidade: {
            QString detalhe;
            const PerfilWarp& p = PERFIS_WARP[m_Qualidade];
            if (p.altura > 0) {
                detalhe = QString("%1p · %2 Mbps").arg(p.altura).arg(p.kbps / 1000);
            }
            else {
                detalhe = QString("agora %1p · %2 Mbps").arg(agora.height).arg(qRound(agora.bitrate / 1000.0));
            }
            if (qualidadeNova) {
                // Botão Aplicar no lugar do detalhe.
                const QString texto = "Aplicar";
                int w = larguraDoTexto(m_Forte, texto) + px(32);
                int h = px(32);
                m_AreaAplicar = { direita - w, y + (linha - h) / 2, w, h };
                pintar(s, m_AreaAplicar.x, m_AreaAplicar.y, w, h, h / 2, COR_VERDE);
                escreverNoMeio(s, m_Forte, texto, COR_ESCURO, m_AreaAplicar.x + px(16), m_AreaAplicar.y, h);
            }
            else {
                escreverNoMeio(s, m_Pequena, detalhe, COR_APAGADO, direita - larguraDoTexto(m_Pequena, detalhe), y, linha);
            }

            // As cinco qualidades, lado a lado.
            const int xOpcoes = xTexto;
            const int larguraOpcoes = direita - xOpcoes;
            const int espaco = px(8);
            const int w = (larguraOpcoes - espaco * (QUANTOS_PERFIS_WARP - 1)) / QUANTOS_PERFIS_WARP;
            const int yOpcoes = y + linha - px(4);
            for (int q = 0; q < QUANTOS_PERFIS_WARP; q++) {
                SDL_Rect r = { xOpcoes + q * (w + espaco), yOpcoes, w, alturaOpcoes };
                m_AreaQualidades.append(r);
                const bool marcada = q == m_Qualidade;
                const bool emUso = q == m_QualidadeAtual;
                pintar(s, r.x, r.y, r.w, r.h, r.h / 2, marcada ? COR_VERDE : FUNDO_OPCAO);
                QString nome = QString::fromUtf8(PERFIS_WARP[q].nome);
                if (q == 0) {
                    nome = "Auto";
                }
                SDL_Color cor = marcada ? COR_ESCURO : (emUso ? COR_VERDE : COR_TEXTO);
                escreverNoMeio(s, m_Forte, nome, cor, r.x + (r.w - larguraDoTexto(m_Forte, nome)) / 2, r.y, r.h);
            }
            break;
        }
        case ItemEstatisticas: {
            int w = px(46), h = px(26);
            chave(s, estatisticas, direita - w, y + (linha - h) / 2, w, h);
            break;
        }
        case ItemPausar: {
            QString t = "a máquina fica reservada";
            escreverNoMeio(s, m_Pequena, t, COR_APAGADO, direita - larguraDoTexto(m_Pequena, t), y, linha);
            break;
        }
        case ItemEncerrar: {
            QString t = "salva o jogo e libera a máquina";
            escreverNoMeio(s, m_Pequena, t, COR_APAGADO, direita - larguraDoTexto(m_Pequena, t), y, linha);
            break;
        }
        }
        y += alturaDoItem;
    }

    // Rodapé: as teclas.
    y += px(18);
    int x = x0 + folga;
    const int h = alturaRodape;
    auto dica = [&](const QStringList& teclas, const QString& texto) {
        for (const QString& t : teclas) {
            x += tecla(s, m_Pequena, t, x, y, h) + px(4);
        }
        x += px(4);
        escreverNoMeio(s, m_Pequena, texto, COR_APAGADO, x, y, h);
        x += larguraDoTexto(m_Pequena, texto) + px(18);
    };
    dica({ "↑", "↓" }, "escolher");
    dica({ "←", "→" }, "qualidade");
    dica({ "Enter" }, "confirmar");
    dica({ "Esc" }, "voltar");

    mostrar(s);
}

void WarpMenuDoJogo::desenharDica()
{
    if (!abrirFontes()) {
        return;
    }
    const double e = escala();
    auto px = [e](double v) { return qRound(v * e); };

    int telaW, telaH;
    tamanhoDaTela(telaW, telaH);
    if (telaW <= 0 || telaH <= 0) {
        return;
    }

    // Uma pílula no alto da tela: "Menu da partida  [Ctrl] + [Shift] + [M]".
    const int altura = px(52);
    const int tecla_h = px(30);
    const QString titulo = "Menu da partida";
    const QString controle = "no controle: Start + Select + LB + RB";
    const QStringList teclas = { "Ctrl", "Shift", "M" };

    int larguraTeclas = 0;
    for (const QString& t : teclas) {
        larguraTeclas += qMax(tecla_h, larguraDoTexto(m_Pequena, t) + tecla_h * 2 / 3);
    }
    larguraTeclas += (teclas.size() - 1) * (larguraDoTexto(m_Pequena, "+") + px(12));
    const int largura = px(20) + px(26) + px(12) + larguraDoTexto(m_Forte, titulo) + px(16) + larguraTeclas +
            px(16) + larguraDoTexto(m_Pequena, controle) + px(24);

    SDL_Surface* s = novaImagem(telaW, telaH);
    if (s == nullptr) {
        return;
    }
    const int x0 = (telaW - largura) / 2;
    const int y0 = px(28);
    pintar(s, x0 - 1, y0 - 1, largura + 2, altura + 2, altura / 2 + 1, BORDA_CARTAO);
    pintar(s, x0, y0, largura, altura, altura / 2, SDL_Color { 11, 25, 38, 235 });

    int x = x0 + px(20);
    logo(s, x, y0 + (altura - px(26)) / 2, px(26));
    x += px(26) + px(12);
    escreverNoMeio(s, m_Forte, titulo, COR_BRANCO, x, y0, altura);
    x += larguraDoTexto(m_Forte, titulo) + px(16);
    for (int i = 0; i < teclas.size(); i++) {
        if (i > 0) {
            x += px(6);
            escreverNoMeio(s, m_Pequena, "+", COR_APAGADO, x, y0, altura);
            x += larguraDoTexto(m_Pequena, "+") + px(6);
        }
        x += tecla(s, m_Pequena, teclas[i], x, y0 + (altura - tecla_h) / 2, tecla_h);
    }
    x += px(16);
    escreverNoMeio(s, m_Pequena, controle, COR_APAGADO, x, y0, altura);

    mostrar(s);
}
