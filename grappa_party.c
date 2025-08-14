
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* === Display target ======================================================== */
#ifndef LCD_W
#define LCD_W 720
#endif
#ifndef LCD_H
#define LCD_H 720
#endif

/* === Tipografia/animazione ================================================= */
#define SCALE                 2        /* glifo 5x7 -> 10x14 px */
#define CHAR_W_PIX            (5 * SCALE)
#define CHAR_H_PIX            (7 * SCALE)
#define CHAR_ADV_PIX          (6 * SCALE)  /* 5 + 1 spacing */
#define DEFAULT_LINE_ADV      (8 * SCALE)  /* 7 + 1 interlinea minima */
#define MARGIN_X              24
#define MARGIN_Y              60
#define FPS_TARGET_MS         33       /* ~30 FPS */

/* Onda “bandiera” */
#define WAVE_AMPL_PIX         (2 * SCALE)     /* ampiezza verticale */
#define WAVE_WAVELEN_PIX      120.0f          /* lunghezza d’onda (px) */
#define WAVE_SPEED_RAD_S      0.8f            /* velocità angolare (rad/s) */
#define WAVE_LINE_PHASE_RAD   0.5f            /* sfasamento tra righe */

/* === Messaggio ============================================================= */

static const char* MESSAGE =
"            G.R.A.P.P.A O.S.            \n"
"11/08/2025 - YOU ARE INVITED\n"
"Come tonight @ G.R.A.P.P.A ITALIAN CLUSTER\n"
"We have a DJ, a dance floor and a lot of grappa!\n"
"Join us FROM 8PM TILL ... well .. till we stand\n"
"PARTY LIKE NEVER BEFORE AND MAKE NEW FRIENDS!";

/* === Stato app ============================================================= */
typedef struct {
    SDL_Window*   window;
    SDL_Renderer* renderer;
    uint32_t      frame;
    uint64_t      t0_ms;
} AppState;

/* === HSV -> RGB ============================================================ */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t* R, uint8_t* G, uint8_t* B) {
    if (s == 0) { *R = *G = *B = v; return; }
    uint16_t region = (h / 256) % 6;
    int rem = (int)(h % 256);
    uint8_t p = (uint8_t)((v * (255 - s)) / 255);
    uint8_t q = (uint8_t)((v * (255 - (s * rem) / 255)) / 255);
    uint8_t t = (uint8_t)((v * (255 - (s * (255 - rem)) / 255)) / 255);
    switch (region) {
        case 0: *R = v; *G = t; *B = p; break;
        case 1: *R = q; *G = v; *B = p; break;
        case 2: *R = p; *G = v; *B = t; break;
        case 3: *R = p; *G = q; *B = v; break;
        case 4: *R = t; *G = p; *B = v; break;
        default:*R = v; *G = p; *B = q; break;
    }
}

/* === Font bitmap 5x7 (subset ASCII) ======================================= */
static const uint8_t DIGITS_5X7[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x10,0x10},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}
};
static const uint8_t LETTERS_5X7[26][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x11,0x19,0x15,0x13,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},{0x11,0x0A,0x04,0x04,0x0A,0x11,0x11},
    {0x11,0x0A,0x04,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
};
static const uint8_t GLYPH_SPACE[7]={0,0,0,0,0,0,0}, GLYPH_EXCL [7]={0x08,0x08,0x08,0x08,0,0,0x08};
static const uint8_t GLYPH_COMMA[7]={0,0,0,0,0,0x08,0x10}, GLYPH_DASH [7]={0,0,0x1C,0,0,0,0};
static const uint8_t GLYPH_DOT [7]={0,0,0,0,0,0,0x08}, GLYPH_COLON[7]={0,0x08,0,0,0x08,0,0};
static const uint8_t GLYPH_AT  [7]={0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, GLYPH_LPAR [7]={0x02,0x04,0x08,0x08,0x08,0x04,0x02};
static const uint8_t GLYPH_RPAR[7]={0x08,0x04,0x02,0x02,0x02,0x04,0x08}, GLYPH_QMARK[7]={0x0E,0x11,0x02,0x04,0x04,0,0x04};

static const uint8_t* glyph_rows(uint8_t c) {
    if (c>='0' && c<='9') return DIGITS_5X7[c-'0'];
    if (c>='A' && c<='Z') return LETTERS_5X7[c-'A'];
    if (c>='a' && c<='z') return LETTERS_5X7[c-'a'];
    switch (c) {
        case ' ': return GLYPH_SPACE; case '!': return GLYPH_EXCL;
        case ',': return GLYPH_COMMA; case '-': return GLYPH_DASH;
        case '.': return GLYPH_DOT;   case ':': return GLYPH_COLON;
        case '@': return GLYPH_AT;    case '(': return GLYPH_LPAR;
        case ')': return GLYPH_RPAR;  case '?': return GLYPH_QMARK;
        default:  return NULL;
    }
}

/* === Drawing primitives ==================================================== */
static void draw_char(SDL_Renderer* r, int x, int y, int scale, uint8_t c) {
    const uint8_t* rows = glyph_rows(c);
    if (!rows) return;
    SDL_FRect fr; fr.w = (float)scale; fr.h = (float)scale;
    for (int ry=0; ry<7; ++ry) {
        uint8_t row = rows[ry];
        for (int rx=0; rx<5; ++rx) {
            if ((row >> (4 - rx)) & 1) {
                fr.x = (float)(x + rx*scale);
                fr.y = (float)(y + ry*scale);
                SDL_RenderFillRect(r, &fr);
            }
        }
    }
}

/* Disegna una “slice” di testo (len caratteri), con effetto bandiera */
static void draw_textn_flag(SDL_Renderer* r, int x, int y, int scale,
                            const char* text, int len,
                            float t_seconds, int line_index)
{
    int cx = x, cy = y;
    for (int i=0; i<len; ++i) {
        uint8_t ch = (uint8_t)text[i];
        /* colore rainbow per carattere */
        uint16_t hue = (uint16_t)(((i * 48u) + (uint32_t)(t_seconds * 20.0f)) & 0x5FFu);
        uint8_t R,G,B; hsv_to_rgb(hue, 255, 255, &R,&G,&B);
        SDL_SetRenderDrawColor(r, R, G, B, SDL_ALPHA_OPAQUE);

        /* offset sinusoidale (flag) */
        float phase_x   = (float)cx * (2.0f * (float)M_PI / WAVE_WAVELEN_PIX);
        float phase_t   = WAVE_SPEED_RAD_S * t_seconds;
        float phase_row = WAVE_LINE_PHASE_RAD * (float)line_index;
        int   wave_off  = (int)(WAVE_AMPL_PIX * sinf(phase_x + phase_t + phase_row));

        draw_char(r, cx, cy + wave_off, scale, ch);
        cx += CHAR_ADV_PIX;
    }
}

/* === Wrapping e layout ===================================================== */
typedef struct { const char* ptr; int len; } Slice;

#define MAX_LINES 64

/* Splitta MESSAGE in righe (per '\n'), poi applica word-wrap per larghezza */
static int wrap_message(const char* msg, int max_cols, Slice out[MAX_LINES]) {
    int n = 0;
    const char* p = msg;
    while (*p && n < MAX_LINES) {
        /* estrai riga base fino a '\n' o fine */
        const char* line = p;
        while (*p && *p != '\n') ++p;
        int base_len = (int)(p - line);
        if (*p == '\n') ++p;

        /* wrap della riga base */
        int i = 0;
        while (i < base_len && n < MAX_LINES) {
            int take = (base_len - i > max_cols) ? max_cols : (base_len - i);
            /* backtrack fino allo spazio se possibile */
            if (take == max_cols) {
                int j = take - 1;
                while (j > 0 && line[i + j] != ' ') --j;
                if (j > 0) take = j; /* spezza su spazio */
            }
            out[n].ptr = line + i;
            out[n].len = take;
            n++;
            i += take;
            /* skip singolo spazio se wrap su spazio */
            while (i < base_len && line[i] == ' ') ++i;
        }
        if (base_len == 0 && n < MAX_LINES) { /* riga vuota -> mantieni */
            out[n].ptr = line; out[n].len = 0; n++;
        }
    }
    return n;
}

/* === SDL Callbacks ========================================================= */
static AppState g_as;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    (void)argc; (void)argv;
    if (!SDL_SetAppMetadata("WHY2025 Invite (Flag)", "1.2", "com.example.invite.flag"))
        return SDL_APP_FAILURE;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    memset(&g_as, 0, sizeof(g_as));
    *appstate = &g_as;

    g_as.window = SDL_CreateWindow("invite/why2025", LCD_W, LCD_H, 0);
    if (!g_as.window) { fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError()); return SDL_APP_FAILURE; }
    g_as.renderer = SDL_CreateRenderer(g_as.window, NULL);
    if (!g_as.renderer){ fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError()); return SDL_APP_FAILURE; }

    g_as.frame = 0;
    g_as.t0_ms = SDL_GetTicks();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* e) {
    (void)appstate;
    if (e->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (e->type == SDL_EVENT_KEY_DOWN) {
        if (e->key.scancode == SDL_SCANCODE_ESCAPE || e->key.scancode == SDL_SCANCODE_Q)
            return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    (void)appstate;

    /* Tempo per onda */
    uint64_t now_ms = SDL_GetTicks();
    float    t_sec  = (float)(now_ms - g_as.t0_ms) / 1000.0f;

    /* Background */
    uint8_t bgR,bgG,bgB;
    hsv_to_rgb((uint16_t)((g_as.frame * 2u) & 0x5FFu), 60, 40, &bgR,&bgG,&bgB);
    SDL_SetRenderDrawColor(g_as.renderer, bgR, bgG, bgB, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(g_as.renderer);

    /* Wrapping per screen width */
    int max_cols = (LCD_W - 2*MARGIN_X) / CHAR_ADV_PIX;
    if (max_cols < 1) max_cols = 1;
    Slice lines[MAX_LINES];
    int n_lines = wrap_message(MESSAGE, max_cols, lines);
    if (n_lines < 1) n_lines = 1;

    /* Calcolo step verticale per riempire lo schermo, rispettando l’onda */
    int min_gap = CHAR_H_PIX + 2*WAVE_AMPL_PIX + 2;  /* nessuna sovrapposizione */
    int avail_h = LCD_H - 2*MARGIN_Y;
    int gap = (n_lines > 1) ? (avail_h / (n_lines - 1)) : avail_h;
    if (gap < min_gap) gap = min_gap;
    /* Se così sforiamo, ridistribuiamo dall’alto con scroll “soft” (semplice clamp) */
    int total_h = (n_lines - 1) * gap + CHAR_H_PIX;
    int y0 = (LCD_H - total_h) / 2;
    if (y0 < MARGIN_Y) y0 = MARGIN_Y;

    /* Render righe */
    for (int li=0; li<n_lines; ++li) {
        const char* s = lines[li].ptr;
        int len       = lines[li].len;
        int line_wpx  = len * CHAR_ADV_PIX;
        int x         = MARGIN_X + (LCD_W - 2*MARGIN_X - line_wpx)/2;
        int y         = y0 + li * gap;
        draw_textn_flag(g_as.renderer, x, y, SCALE, s, len, t_sec, li);
    }

    SDL_RenderPresent(g_as.renderer);
    g_as.frame++;
    SDL_Delay(FPS_TARGET_MS);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    (void)appstate; (void)result;
    if (g_as.renderer) SDL_DestroyRenderer(g_as.renderer);
    if (g_as.window)   SDL_DestroyWindow(g_as.window);
    SDL_Quit();
}
