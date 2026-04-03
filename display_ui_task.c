#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "shared_data.h"

/*
    SafeFeet by Njima
    display_ui_task.c

    IHM graphique SDL2 avec :
    - Visualisation du pied qui pivote selon le tilt
    - Jauges de pression gauche / droite
    - Indicateurs d'etat (NORMAL, WARNING, FALL_RISK, FALL_IMMINENT)
    - LED couleur
    - Donnees capteurs en temps reel
*/

/* ===================================== */
/* Constantes graphiques                 */
/* ===================================== */

#define WIN_W       1000
#define WIN_H       650
#define FPS         30
#define FRAME_DELAY (1000 / FPS)

/* Couleurs RGBA */
#define COL_BG_R        30
#define COL_BG_G        30
#define COL_BG_B        40
#define COL_BG_A        255

#define COL_PANEL_R     45
#define COL_PANEL_G     45
#define COL_PANEL_B     58
#define COL_PANEL_A     255

#define COL_TEXT_R      220
#define COL_TEXT_G      220
#define COL_TEXT_B      220
#define COL_TEXT_A      255

#define COL_TITLE_R     100
#define COL_TITLE_G     180
#define COL_TITLE_B     255
#define COL_TITLE_A     255

#define COL_GREEN_R     50
#define COL_GREEN_G     205
#define COL_GREEN_B     50

#define COL_ORANGE_R    255
#define COL_ORANGE_G    165
#define COL_ORANGE_B    0

#define COL_RED_R       220
#define COL_RED_G       50
#define COL_RED_B       50

#define COL_GREY_R      80
#define COL_GREY_G      80
#define COL_GREY_B      90


/* ===================================== */
/* Chemins police (Linux)                */
/* ===================================== */

static const char *font_paths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    NULL
};


/* ===================================== */
/* Variables globales SDL                */
/* ===================================== */

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font     *font_big = NULL;
static TTF_Font     *font_med = NULL;
static TTF_Font     *font_sml = NULL;


/* ===================================== */
/* Fonctions utilitaires dessin          */
/* ===================================== */

static void draw_text(TTF_Font *font, const char *text,
                      int x, int y,
                      Uint8 r, Uint8 g, Uint8 b)
{
    if (!font || !text || text[0] == '\0') return;

    SDL_Color color = {r, g, b, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) { SDL_FreeSurface(surf); return; }

    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_text_centered(TTF_Font *font, const char *text,
                               int cx, int y,
                               Uint8 r, Uint8 g, Uint8 b)
{
    if (!font || !text || text[0] == '\0') return;
    int tw = 0, th = 0;
    TTF_SizeText(font, text, &tw, &th);
    draw_text(font, text, cx - tw / 2, y, r, g, b);
}

static void draw_filled_rect(int x, int y, int w, int h,
                              Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

static void draw_rect_outline(int x, int y, int w, int h,
                               Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderer, &rect);
}

/* Cercle rempli (algorithme midpoint) */
static void draw_filled_circle(int cx, int cy, int radius,
                                Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int dy = -radius; dy <= radius; dy++)
    {
        int dx = (int)sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

/* Ligne epaisse */
static void draw_thick_line(int x1, int y1, int x2, int y2,
                             int thickness,
                             Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int i = -thickness / 2; i <= thickness / 2; i++)
    {
        SDL_RenderDrawLine(renderer, x1 + i, y1, x2 + i, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + i, x2, y2 + i);
    }
}


/* ===================================== */
/* Dessin du pied avec rotation (tilt)   */
/* ===================================== */

static void rotate_point(float cx, float cy,
                          float px, float py,
                          float angle_deg,
                          float *ox, float *oy)
{
    float rad = angle_deg * (float)M_PI / 180.0f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float dx = px - cx;
    float dy = py - cy;
    *ox = cx + dx * cosA - dy * sinA;
    *oy = cy + dx * sinA + dy * cosA;
}

static void draw_foot(int cx, int cy, float tilt_angle,
                       Uint8 r, Uint8 g, Uint8 b)
{
    /* Limiter l'angle d'affichage */
    float angle = tilt_angle;
    if (angle > 45.0f) angle = 45.0f;
    if (angle < -45.0f) angle = -45.0f;

    /*
       Forme du pied (points relatifs au centre):
       Semelle rectangulaire arrondie + orteils
    */

    /* Points de la semelle (forme simplifiee) */
    float pts[][2] = {
        /* Talon (bas) */
        {-25, 70},
        {-30, 55},
        {-32, 30},
        {-30, 0},
        /* Voute */
        {-28, -20},
        {-25, -40},
        /* Avant-pied */
        {-30, -55},
        {-35, -65},
        /* Orteils */
        {-30, -80},
        {-18, -90},
        {-5,  -95},
        {10,  -95},
        {22,  -90},
        {32,  -82},
        /* Cote droit */
        {35,  -65},
        {32,  -55},
        {30,  -40},
        {28,  -20},
        {30,  0},
        {32,  30},
        {30,  55},
        {25,  70},
    };
    int npts = sizeof(pts) / sizeof(pts[0]);

    /* Appliquer la rotation */
    float rotated[22][2];
    for (int i = 0; i < npts; i++)
    {
        rotate_point(0, 0, pts[i][0], pts[i][1], angle,
                     &rotated[i][0], &rotated[i][1]);
    }

    /* Dessiner les lignes du contour */
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int i = 0; i < npts; i++)
    {
        int next = (i + 1) % npts;
        int x1 = cx + (int)rotated[i][0];
        int y1 = cy + (int)rotated[i][1];
        int x2 = cx + (int)rotated[next][0];
        int y2 = cy + (int)rotated[next][1];

        /* Dessiner 3 fois pour epaisseur */
        for (int t = -1; t <= 1; t++)
        {
            SDL_RenderDrawLine(renderer, x1 + t, y1, x2 + t, y2);
            SDL_RenderDrawLine(renderer, x1, y1 + t, x2, y2 + t);
        }
    }

    /* Remplissage simplifie : lignes horizontales entre les bords */
    for (int row = -100; row <= 80; row++)
    {
        int min_x = 9999, max_x = -9999;
        for (int i = 0; i < npts; i++)
        {
            int next = (i + 1) % npts;
            float y1f = rotated[i][1];
            float y2f = rotated[next][1];
            float x1f = rotated[i][0];
            float x2f = rotated[next][0];

            if ((y1f <= row && y2f >= row) || (y2f <= row && y1f >= row))
            {
                if (fabsf(y2f - y1f) < 0.01f) continue;
                float t = (row - y1f) / (y2f - y1f);
                float ix = x1f + t * (x2f - x1f);
                int ixi = (int)ix;
                if (ixi < min_x) min_x = ixi;
                if (ixi > max_x) max_x = ixi;
            }
        }
        if (min_x < max_x)
        {
            /* Couleur interieure plus sombre */
            SDL_SetRenderDrawColor(renderer, r / 2, g / 2, b / 2, 180);
            SDL_RenderDrawLine(renderer,
                               cx + min_x + 1, cy + row,
                               cx + max_x - 1, cy + row);
        }
    }

    /* Re-dessiner le contour par dessus */
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int i = 0; i < npts; i++)
    {
        int next = (i + 1) % npts;
        int x1 = cx + (int)rotated[i][0];
        int y1 = cy + (int)rotated[i][1];
        int x2 = cx + (int)rotated[next][0];
        int y2 = cy + (int)rotated[next][1];
        for (int t = -1; t <= 1; t++)
        {
            SDL_RenderDrawLine(renderer, x1 + t, y1, x2 + t, y2);
            SDL_RenderDrawLine(renderer, x1, y1 + t, x2, y2 + t);
        }
    }

    /* Point central (cheville) */
    draw_filled_circle(cx, cy, 5, 255, 255, 255);
}


/* ===================================== */
/* Jauge verticale                       */
/* ===================================== */

static void draw_gauge(int x, int y, int w, int h,
                        float value, float max_val,
                        const char *label,
                        Uint8 r, Uint8 g, Uint8 b)
{
    /* Fond de la jauge */
    draw_filled_rect(x, y, w, h, COL_GREY_R, COL_GREY_G, COL_GREY_B, 255);
    draw_rect_outline(x, y, w, h, 150, 150, 160);

    /* Remplissage */
    float ratio = value / max_val;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < 0.0f) ratio = 0.0f;

    int fill_h = (int)(ratio * h);
    draw_filled_rect(x + 1, y + h - fill_h, w - 2, fill_h, r, g, b, 255);

    /* Valeur numerique */
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f", value);
    draw_text_centered(font_sml, buf, x + w / 2, y + h + 5,
                       COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);

    /* Label */
    draw_text_centered(font_sml, label, x + w / 2, y - 20,
                       COL_TITLE_R, COL_TITLE_G, COL_TITLE_B);
}


/* ===================================== */
/* LED circulaire                        */
/* ===================================== */

static void draw_led(int cx, int cy, int radius, int led_state)
{
    Uint8 r, g, b;

    switch (led_state)
    {
        case LED_GREEN:
            r = COL_GREEN_R; g = COL_GREEN_G; b = COL_GREEN_B;
            break;
        case LED_ORANGE:
            r = COL_ORANGE_R; g = COL_ORANGE_G; b = COL_ORANGE_B;
            break;
        case LED_RED:
            r = COL_RED_R; g = COL_RED_G; b = COL_RED_B;
            break;
        default:
            r = 60; g = 60; b = 60;
            break;
    }

    /* Halo */
    draw_filled_circle(cx, cy, radius + 4, r / 3, g / 3, b / 3);
    /* LED */
    draw_filled_circle(cx, cy, radius, r, g, b);
    /* Reflet */
    draw_filled_circle(cx - radius / 3, cy - radius / 3, radius / 4,
                        (Uint8)((r + 255) / 2),
                        (Uint8)((g + 255) / 2),
                        (Uint8)((b + 255) / 2));
}


/* ===================================== */
/* Panneau d'etat                        */
/* ===================================== */

static void draw_state_banner(int x, int y, int w, int h,
                               FallState state)
{
    Uint8 r, g, b;
    const char *text;

    switch (state)
    {
        case STATE_NORMAL:
            r = COL_GREEN_R; g = COL_GREEN_G; b = COL_GREEN_B;
            text = "NORMAL";
            break;
        case STATE_WARNING:
            r = COL_ORANGE_R; g = COL_ORANGE_G; b = COL_ORANGE_B;
            text = "WARNING";
            break;
        case STATE_FALL_RISK:
            r = COL_RED_R; g = COL_RED_G; b = COL_RED_B;
            text = "FALL RISK";
            break;
        case STATE_FALL_IMMINENT:
            r = 200; g = 0; b = 0;
            text = "FALL IMMINENT !";
            break;
        default:
            r = 100; g = 100; b = 100;
            text = "UNKNOWN";
            break;
    }

    draw_filled_rect(x, y, w, h, r / 4, g / 4, b / 4, 255);
    draw_rect_outline(x, y, w, h, r, g, b);
    draw_text_centered(font_med, text, x + w / 2, y + (h - 18) / 2,
                       r, g, b);
}


/* ===================================== */
/* Panneau info (section)                */
/* ===================================== */

static void draw_panel(int x, int y, int w, int h, const char *title)
{
    draw_filled_rect(x, y, w, h,
                     COL_PANEL_R, COL_PANEL_G, COL_PANEL_B, COL_PANEL_A);
    draw_rect_outline(x, y, w, h, 70, 70, 85);

    if (title)
    {
        draw_text(font_med, title, x + 10, y + 5,
                  COL_TITLE_R, COL_TITLE_G, COL_TITLE_B);
    }
}


/* ===================================== */
/* Rendu principal                       */
/* ===================================== */

static void render_frame(SensorData *s, MapData *m,
                          FallState fs, ActuatorState *a)
{
    /* Fond */
    SDL_SetRenderDrawColor(renderer,
                           COL_BG_R, COL_BG_G, COL_BG_B, COL_BG_A);
    SDL_RenderClear(renderer);

    /* === TITRE === */
    draw_text_centered(font_big, "SAFEFEET BY NJIMA",
                       WIN_W / 2, 10,
                       COL_TITLE_R, COL_TITLE_G, COL_TITLE_B);

    /* Ligne separation */
    SDL_SetRenderDrawColor(renderer, 70, 70, 85, 255);
    SDL_RenderDrawLine(renderer, 20, 45, WIN_W - 20, 45);


    /* === PANNEAU GAUCHE : Capteurs === */
    draw_panel(15, 55, 280, 280, "CAPTEURS");

    char buf[64];
    int lx = 25, ly = 85;

    snprintf(buf, sizeof(buf), "Scenario    : %d", s->mode_scenario);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Accel       : %.2f", s->accel);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Tilt        : %.1f deg", s->tilt);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Gyro        : %.2f", s->gyro);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Press. L    : %.1f", s->pressure_left);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Press. R    : %.1f", s->pressure_right);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Depth       : %.2f", s->depth);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);


    /* === PANNEAU GAUCHE BAS : Terrain === */
    draw_panel(15, 345, 280, 170, "TERRAIN");

    lx = 25; ly = 375;

    snprintf(buf, sizeof(buf), "Trou        : %s",
             m->hole_detected ? "OUI" : "NON");
    draw_text(font_sml, buf, lx, ly,
              m->hole_detected ? COL_RED_R : COL_GREEN_R,
              m->hole_detected ? COL_RED_G : COL_GREEN_G,
              m->hole_detected ? COL_RED_B : COL_GREEN_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Obstacle    : %s",
             m->obstacle_detected ? "OUI" : "NON");
    draw_text(font_sml, buf, lx, ly,
              m->obstacle_detected ? COL_ORANGE_R : COL_GREEN_R,
              m->obstacle_detected ? COL_ORANGE_G : COL_GREEN_G,
              m->obstacle_detected ? COL_ORANGE_B : COL_GREEN_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Glissant    : %s",
             m->slippery_surface ? "OUI" : "NON");
    draw_text(font_sml, buf, lx, ly,
              m->slippery_surface ? COL_ORANGE_R : COL_GREEN_R,
              m->slippery_surface ? COL_ORANGE_G : COL_GREEN_G,
              m->slippery_surface ? COL_ORANGE_B : COL_GREEN_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Pente       : %.1f deg", m->terrain_slope);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    snprintf(buf, sizeof(buf), "Dist. obs.  : %.2f m", m->obstacle_distance);
    draw_text(font_sml, buf, lx, ly, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);


    /* === CENTRE : Visualisation du pied === */
    draw_panel(310, 55, 370, 400, "VISUALISATION PIED");

    /* Couleur du pied selon l'etat */
    Uint8 fr, fg, fb;
    switch (fs)
    {
        case STATE_NORMAL:
            fr = COL_GREEN_R; fg = COL_GREEN_G; fb = COL_GREEN_B;
            break;
        case STATE_WARNING:
            fr = COL_ORANGE_R; fg = COL_ORANGE_G; fb = COL_ORANGE_B;
            break;
        case STATE_FALL_RISK:
        case STATE_FALL_IMMINENT:
            fr = COL_RED_R; fg = COL_RED_G; fb = COL_RED_B;
            break;
        default:
            fr = 150; fg = 150; fb = 150;
            break;
    }

    /* Ligne de reference horizontale (sol) */
    SDL_SetRenderDrawColor(renderer, 100, 100, 110, 255);
    SDL_RenderDrawLine(renderer, 340, 360, 650, 360);
    draw_text_centered(font_sml, "sol", 650, 363,
                       100, 100, 110);

    /* Dessin du pied au centre du panneau */
    draw_foot(495, 255, s->tilt, fr, fg, fb);

    /* Indicateur d'angle */
    snprintf(buf, sizeof(buf), "Tilt: %.1f", s->tilt);
    draw_text_centered(font_sml, buf, 495, 410,
                       COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);


    /* === JAUGES DE PRESSION === */
    draw_panel(310, 465, 370, 170, "PRESSION PIEDS");

    /* Jauge gauche */
    draw_gauge(380, 510, 40, 100,
               s->pressure_left, 100.0f,
               "Gauche",
               COL_TITLE_R, COL_TITLE_G, COL_TITLE_B);

    /* Jauge droite */
    draw_gauge(560, 510, 40, 100,
               s->pressure_right, 100.0f,
               "Droite",
               COL_TITLE_R, COL_TITLE_G, COL_TITLE_B);


    /* === PANNEAU DROITE : Etat systeme === */
    draw_panel(695, 55, 290, 200, "ETAT SYSTEME");

    /* Banniere d'etat */
    draw_state_banner(710, 85, 260, 45, fs);

    /* LED */
    draw_text(font_sml, "LED :", 710, 145,
              COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    draw_led(800, 155, 14, a->led_state);

    /* Stabilisation */
    snprintf(buf, sizeof(buf), "Stabilisation : %s",
             a->stabilization_on ? "ON" : "OFF");
    draw_text(font_sml, buf, 710, 175,
              a->stabilization_on ? COL_ORANGE_R : COL_TEXT_R,
              a->stabilization_on ? COL_ORANGE_G : COL_TEXT_G,
              a->stabilization_on ? COL_ORANGE_B : COL_TEXT_B);

    /* Cheville */
    snprintf(buf, sizeof(buf), "Cheville lock : %s",
             a->ankle_lock_on ? "ON" : "OFF");
    draw_text(font_sml, buf, 710, 197,
              a->ankle_lock_on ? COL_ORANGE_R : COL_TEXT_R,
              a->ankle_lock_on ? COL_ORANGE_G : COL_TEXT_G,
              a->ankle_lock_on ? COL_ORANGE_B : COL_TEXT_B);

    /* Vibration */
    snprintf(buf, sizeof(buf), "Vibration     : %d/3",
             a->vibration_level);
    draw_text(font_sml, buf, 710, 219,
              COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);


    /* === PANNEAU DROITE BAS : Actionneurs === */
    draw_panel(695, 270, 290, 185, "ACTIONNEURS");

    lx = 710; ly = 300;

    /* Buzzer */
    snprintf(buf, sizeof(buf), "Buzzer : %s",
             a->buzzer_on ? "ACTIF" : "OFF");
    draw_text(font_sml, buf, lx, ly,
              a->buzzer_on ? COL_RED_R : COL_TEXT_R,
              a->buzzer_on ? COL_RED_G : COL_TEXT_G,
              a->buzzer_on ? COL_RED_B : COL_TEXT_B);
    ly += 25;

    /* Barre de vibration visuelle */
    draw_text(font_sml, "Niveau vibration :", lx, ly,
              COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    ly += 22;

    for (int i = 0; i < 3; i++)
    {
        if (i < a->vibration_level)
        {
            Uint8 vr, vg, vb;
            if (i == 0) { vr = COL_GREEN_R; vg = COL_GREEN_G; vb = COL_GREEN_B; }
            else if (i == 1) { vr = COL_ORANGE_R; vg = COL_ORANGE_G; vb = COL_ORANGE_B; }
            else { vr = COL_RED_R; vg = COL_RED_G; vb = COL_RED_B; }
            draw_filled_rect(lx + i * 75, ly, 65, 20, vr, vg, vb, 255);
        }
        else
        {
            draw_filled_rect(lx + i * 75, ly, 65, 20,
                             COL_GREY_R, COL_GREY_G, COL_GREY_B, 255);
        }
        draw_rect_outline(lx + i * 75, ly, 65, 20, 120, 120, 130);
    }

    ly += 35;

    /* Indicateur de danger visuel (grand) */
    if (fs == STATE_FALL_IMMINENT)
    {
        /* Clignotement simule avec SDL_GetTicks */
        Uint32 ticks = SDL_GetTicks();
        if ((ticks / 300) % 2 == 0)
        {
            draw_filled_rect(lx, ly, 260, 30, COL_RED_R, 0, 0, 255);
            draw_text_centered(font_med, "!! DANGER CHUTE !!",
                               lx + 130, ly + 5,
                               255, 255, 255);
        }
    }
    else if (fs == STATE_FALL_RISK)
    {
        draw_filled_rect(lx, ly, 260, 30, 80, 20, 20, 255);
        draw_text_centered(font_med, "RISQUE DETECTE",
                           lx + 130, ly + 5,
                           COL_RED_R, COL_RED_G, COL_RED_B);
    }


    /* === BARRE DU BAS : infos === */
    SDL_SetRenderDrawColor(renderer, 70, 70, 85, 255);
    SDL_RenderDrawLine(renderer, 20, WIN_H - 25, WIN_W - 20, WIN_H - 25);
    draw_text(font_sml, "SafeFeet by Njima  |  Ctrl+C ou fermer la fenetre pour quitter",
              20, WIN_H - 20,
              90, 90, 100);


    /* Afficher */
    SDL_RenderPresent(renderer);
}


/* ===================================== */
/* Initialisation SDL                    */
/* ===================================== */

static int init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init erreur: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() < 0)
    {
        fprintf(stderr, "TTF_Init erreur: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    window = SDL_CreateWindow(
        "SafeFeet by Njima",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_SHOWN
    );

    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow erreur: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1,
                                   SDL_RENDERER_ACCELERATED |
                                   SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer erreur: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Charger une police */
    const char *found_font = NULL;
    for (int i = 0; font_paths[i] != NULL; i++)
    {
        FILE *f = fopen(font_paths[i], "r");
        if (f)
        {
            fclose(f);
            found_font = font_paths[i];
            break;
        }
    }

    if (!found_font)
    {
        fprintf(stderr, "Aucune police trouvee ! Installez fonts-dejavu.\n");
        fprintf(stderr, "  sudo apt install fonts-dejavu\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    font_big = TTF_OpenFont(found_font, 22);
    font_med = TTF_OpenFont(found_font, 16);
    font_sml = TTF_OpenFont(found_font, 13);

    if (!font_big || !font_med || !font_sml)
    {
        fprintf(stderr, "Erreur chargement police: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    return 0;
}

static void cleanup_sdl(void)
{
    if (font_sml) TTF_CloseFont(font_sml);
    if (font_med) TTF_CloseFont(font_med);
    if (font_big) TTF_CloseFont(font_big);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}


/* ===================================== */
/* Thread IHM SDL2                       */
/* ===================================== */

void *display_ui_task(void *arg)
{
    (void)arg;

    if (init_sdl() != 0)
    {
        fprintf(stderr, "Erreur init SDL, fallback terminal.\n");
        system_running = 0;
        return NULL;
    }

    printf("[SDL2] Fenetre graphique ouverte.\n");

    while (system_running)
    {
        Uint32 frame_start = SDL_GetTicks();

        /* Gestion des evenements SDL */
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                system_running = 0;
            }
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE)
            {
                system_running = 0;
            }
        }

        /* Copie locale des donnees partagees */
        SensorData  local_sensor;
        MapData     local_map;
        FallState   local_fall;
        ActuatorState local_actuator;

        pthread_mutex_lock(&data_mutex);
        local_sensor   = sensor_data;
        local_map      = map_data;
        local_fall     = fall_state;
        local_actuator = actuator_state;
        pthread_mutex_unlock(&data_mutex);

        /* Rendu */
        render_frame(&local_sensor, &local_map,
                     local_fall, &local_actuator);

        /* Limiter le framerate */
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY)
        {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    cleanup_sdl();
    printf("[SDL2] Fenetre graphique fermee.\n");

    return NULL;
}
