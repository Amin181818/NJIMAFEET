#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    SafeFeet by Njima
    display_ui_task.c

    IHM console avec couleurs ANSI.
    Aucune bibliotheque externe requise.

    Priorite SCHED_FIFO : 40 (la plus basse du systeme)
    L'affichage n'est pas critique : il peut attendre que
    les threads de securite aient fini leur travail.
*/

/* ===================================== */
/* Codes couleurs ANSI                   */
/* ===================================== */

#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"

#define FG_RED      "\033[31m"
#define FG_GREEN    "\033[32m"
#define FG_YELLOW   "\033[33m"
#define FG_BLUE     "\033[34m"
#define FG_MAGENTA  "\033[35m"
#define FG_CYAN     "\033[36m"
#define FG_WHITE    "\033[37m"

#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"

#define CLEAR_SCREEN "\033[H\033[2J"
#define HIDE_CURSOR  "\033[?25l"
#define SHOW_CURSOR  "\033[?25h"


/* ===================================== */
/* Fonctions utilitaires                 */
/* ===================================== */

static const char *fall_state_color(FallState state)
{
    switch (state)
    {
        case STATE_NORMAL:        return BOLD FG_GREEN;
        case STATE_WARNING:       return BOLD FG_YELLOW;
        case STATE_FALL_RISK:     return BOLD FG_RED;
        case STATE_FALL_IMMINENT: return BOLD BG_RED FG_WHITE;
        default:                  return RESET;
    }
}

static const char *fall_state_text(FallState state)
{
    switch (state)
    {
        case STATE_NORMAL:        return "  NORMAL  ";
        case STATE_WARNING:       return " WARNING  ";
        case STATE_FALL_RISK:     return " FALL RISK";
        case STATE_FALL_IMMINENT: return "FALL IMMINENT !";
        default:                  return " INCONNU  ";
    }
}

static const char *led_color(int led)
{
    switch (led)
    {
        case LED_GREEN:  return FG_GREEN  BOLD "●" RESET;
        case LED_ORANGE: return FG_YELLOW BOLD "●" RESET;
        case LED_RED:    return FG_RED    BOLD "●" RESET;
        default:         return DIM       "○" RESET;
    }
}

static const char *led_text(int led)
{
    switch (led)
    {
        case LED_GREEN:  return "VERT";
        case LED_ORANGE: return "ORANGE";
        case LED_RED:    return "ROUGE";
        default:         return "OFF";
    }
}

/* Barre de progression : [████████░░░░░░░░] */
static void print_bar(float value, float max_val, int width,
                       const char *color)
{
    float ratio = value / max_val;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < 0.0f) ratio = 0.0f;

    int filled = (int)(ratio * width);

    printf("%s[", color);
    for (int i = 0; i < width; i++)
    {
        if (i < filled)
            printf("█");
        else
            printf(DIM "░" RESET "%s", color);
    }
    printf("]" RESET);
}

/* Indicateur OUI/NON avec couleur */
static void print_bool(int val, const char *label)
{
    if (val)
        printf("  %-18s : " FG_RED BOLD "OUI" RESET "\n", label);
    else
        printf("  %-18s : " FG_GREEN "NON" RESET "\n", label);
}

/* Indicateur ON/OFF */
static void print_onoff(int val, const char *label)
{
    if (val)
        printf("  %-18s : " FG_YELLOW BOLD "ON" RESET "\n", label);
    else
        printf("  %-18s : " DIM "OFF" RESET "\n", label);
}


/* ===================================== */
/* Pied ASCII avec inclinaison           */
/* ===================================== */

static void draw_ascii_foot(float tilt)
{
    /*
       On represente le pied vu de face.
       Le tilt penche le pied a gauche ou a droite.
    */

    /* Calculer le decalage visuel (max 6 colonnes) */
    int offset = (int)(tilt / 5.0f);
    if (offset > 6) offset = 6;
    if (offset < -6) offset = -6;

    /* Couleur selon l'inclinaison */
    const char *color;
    float abs_tilt = tilt < 0 ? -tilt : tilt;
    if (abs_tilt < 8.0f)
        color = FG_GREEN;
    else if (abs_tilt < 15.0f)
        color = FG_YELLOW;
    else
        color = FG_RED;

    /* Espacement de base pour centrer */
    int base = 18;

    /* Dessin du pied (cheville -> semelle) */
    /*    |  |       <- cheville              */
    /*   /    \      <- haut pied             */
    /*  |      |     <- milieu                */
    /*  |______|     <- semelle               */
    /*  ========     <- sol                   */

    int s;  /* espaces avant chaque ligne */

    s = base + offset;
    if (s < 2) s = 2;
    printf("  %s%*s  |  |" RESET "\n", color, s, "");

    s = base + (offset * 4) / 5;
    if (s < 2) s = 2;
    printf("  %s%*s /    \\" RESET "\n", color, s, "");

    s = base + (offset * 3) / 5;
    if (s < 2) s = 2;
    printf("  %s%*s|      |" RESET "\n", color, s, "");

    s = base + (offset * 2) / 5;
    if (s < 2) s = 2;
    printf("  %s%*s|      |" RESET "\n", color, s, "");

    s = base + (offset * 1) / 5;
    if (s < 2) s = 2;
    printf("  %s%*s|______|" RESET "\n", color, s, "");

    /* Sol toujours fixe */
    printf("  " DIM "%*s==========" RESET "\n", base - 1, "");

    /* Afficher l'angle */
    printf("  %s%*s  %.1f deg" RESET "\n", color, base, "", tilt);
}


/* ===================================== */
/* Thread IHM                            */
/* ===================================== */

void *display_ui_task(void *arg)
{
    (void)arg;

    /* ================================================ */
    /* Configuration SCHED_FIFO - Priorite 40           */
    /* ================================================ */
    /* L'IHM a la priorite la plus basse : l'affichage   */
    /* peut etre retarde sans impact sur la securite.    */
    /* Tous les threads critiques passent avant.         */
    /* ================================================ */

    struct sched_param param;
    param.sched_priority = 40;  /* Priorite IHM : 40 (min) */

    int ret = pthread_setschedparam(
        pthread_self(),    /* Thread courant */
        SCHED_FIFO,        /* Politique temps-reel FIFO */
        &param             /* Parametres (priorite) */
    );

    if (ret != 0)
    {
        fprintf(stderr,
                "[display_ui] WARN: pthread_setschedparam echoue: %s\n",
                strerror(ret));
        fprintf(stderr,
                "[display_ui] Lancez avec sudo pour les priorites RT.\n");
    }
    else
    {
        printf("[display_ui] SCHED_FIFO active, priorite = %d\n",
               param.sched_priority);
    }

    /* Cacher le curseur */
    printf(HIDE_CURSOR);

    while (system_running)
    {
        SensorData    local_sensor;
        MapData       local_map;
        FallState     local_fall;
        ActuatorState local_actuator;

        /* Lecture protegee des donnees partagees */
        pthread_mutex_lock(&data_mutex);
        local_sensor   = sensor_data;
        local_map      = map_data;
        local_fall     = fall_state;
        local_actuator = actuator_state;
        pthread_mutex_unlock(&data_mutex);

        /* Effacer l'ecran sans clignotement */
        printf(CLEAR_SCREEN);

        /* ========== TITRE ========== */
        printf("\n");
        printf(BOLD FG_CYAN
               "  ╔══════════════════════════════════════════════════╗\n"
               "  ║            SAFEFEET BY NJIMA                     ║\n"
               "  ╚══════════════════════════════════════════════════╝\n"
               RESET);

        /* ========== ETAT SYSTEME ========== */
        printf("\n  " BOLD FG_BLUE "▸ ETAT SYSTEME" RESET "       ");
        printf("%s %s " RESET, fall_state_color(local_fall),
               fall_state_text(local_fall));
        printf("    LED : %s %s\n",
               led_color(local_actuator.led_state),
               led_text(local_actuator.led_state));

        printf(FG_CYAN
               "  ──────────────────────────────────────────────────\n"
               RESET);

        /* ========== PIED ASCII ========== */
        printf("\n  " BOLD FG_BLUE "▸ VISUALISATION PIED" RESET "\n\n");
        draw_ascii_foot(local_sensor.tilt);

        printf(FG_CYAN
               "\n  ──────────────────────────────────────────────────\n"
               RESET);

        /* ========== CAPTEURS ========== */
        printf("\n  " BOLD FG_BLUE "▸ CAPTEURS" RESET "\n");
        printf("  %-18s : " BOLD "%.2f" RESET "\n",
               "Acceleration", local_sensor.accel);
        printf("  %-18s : " BOLD "%.1f deg" RESET "\n",
               "Inclinaison", local_sensor.tilt);
        printf("  %-18s : " BOLD "%.2f" RESET "\n",
               "Gyroscope", local_sensor.gyro);
        printf("  %-18s : " BOLD "%.2f" RESET "\n",
               "Profondeur", local_sensor.depth);

        /* Jauges de pression */
        printf("\n  Pression gauche   : ");
        print_bar(local_sensor.pressure_left, 100.0f, 20, FG_CYAN);
        printf(" %.0f%%\n", local_sensor.pressure_left);

        printf("  Pression droite   : ");
        print_bar(local_sensor.pressure_right, 100.0f, 20, FG_MAGENTA);
        printf(" %.0f%%\n", local_sensor.pressure_right);

        printf(FG_CYAN
               "\n  ──────────────────────────────────────────────────\n"
               RESET);

        /* ========== TERRAIN ========== */
        printf("\n  " BOLD FG_BLUE "▸ TERRAIN" RESET "\n");
        print_bool(local_map.hole_detected,     "Trou detecte");
        print_bool(local_map.obstacle_detected,  "Obstacle");
        print_bool(local_map.slippery_surface,   "Surface glissante");
        printf("  %-18s : %.1f deg\n", "Pente", local_map.terrain_slope);
        printf("  %-18s : %.2f m\n", "Dist. obstacle",
               local_map.obstacle_distance);

        printf(FG_CYAN
               "\n  ──────────────────────────────────────────────────\n"
               RESET);

        /* ========== ACTIONNEURS ========== */
        printf("\n  " BOLD FG_BLUE "▸ ACTIONNEURS" RESET "\n");
        print_onoff(local_actuator.stabilization_on, "Stabilisation");
        print_onoff(local_actuator.ankle_lock_on,    "Verrouil. cheville");

        printf("  %-18s : ", "Vibration");
        for (int i = 0; i < 3; i++)
        {
            if (i < local_actuator.vibration_level)
            {
                if (i == 0) printf(FG_GREEN "▮" RESET);
                else if (i == 1) printf(FG_YELLOW "▮" RESET);
                else printf(FG_RED "▮" RESET);
            }
            else
                printf(DIM "▯" RESET);
        }
        printf("  (%d/3)\n", local_actuator.vibration_level);

        print_onoff(local_actuator.buzzer_on, "Buzzer");

        /* ========== PIED DE PAGE ========== */
        printf(FG_CYAN
               "\n  ══════════════════════════════════════════════════\n"
               RESET);
        printf(DIM "  Ctrl+C pour quitter" RESET "\n");

        /* Rafraichissement */
        fflush(stdout);
        usleep(300000); /* 300 ms */
    }

    /* Restaurer le curseur */
    printf(SHOW_CURSOR);
    printf(CLEAR_SCREEN);

    return NULL;
}
