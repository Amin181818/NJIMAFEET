#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    display_ui_task.c
    IHM console compacte avec couleurs ANSI.
    Priorite : 40 (la plus basse)  -  Periode : 200 ms
*/

/* Couleurs ANSI */
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
#define CLEAR_SCREEN "\033[H\033[2J"
#define HIDE_CURSOR  "\033[?25l"
#define SHOW_CURSOR  "\033[?25h"

/* Donne le temps en microsecondes */
static long monotonic_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

/* Dort jusqu'a une date absolue (evite la derive) */
static void sleep_until_us(long target_us) {
    struct timespec t;
    t.tv_sec  = target_us / 1000000L;
    t.tv_nsec = (target_us % 1000000L) * 1000L;
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
}

static const char *fall_state_color(FallState s) {
    switch (s) {
        case STATE_NORMAL:        return BOLD FG_GREEN;
        case STATE_WARNING:       return BOLD FG_YELLOW;
        case STATE_FALL_RISK:     return BOLD FG_RED;
        case STATE_FALL_IMMINENT: return BOLD BG_RED FG_WHITE;
        default:                  return RESET;
    }
}

static const char *fall_state_text(FallState s) {
    switch (s) {
        case STATE_NORMAL:        return " NORMAL ";
        case STATE_WARNING:       return "WARNING ";
        case STATE_FALL_RISK:     return "FALL RISK";
        case STATE_FALL_IMMINENT: return "FALL IMMINENT";
        default:                  return "?";
    }
}

static const char *led_str(int led) {
    switch (led) {
        case LED_GREEN:  return FG_GREEN  BOLD "● VERT" RESET;
        case LED_ORANGE: return FG_YELLOW BOLD "● ORANGE" RESET;
        case LED_RED:    return FG_RED    BOLD "● ROUGE" RESET;
        default:         return DIM       "○ OFF" RESET;
    }
}

/* Petite barre [████░░░░] */
static void mini_bar(int filled, int width, const char *color) {
    printf("%s[", color);
    for (int i = 0; i < width; i++) {
        if (i < filled) printf("█");
        else printf(DIM "░" RESET "%s", color);
    }
    printf("]" RESET);
}

/* Dessine le pied vu de face avec inclinaison temps-reel */
static void draw_ascii_foot(float tilt) {
    /* Decalage visuel selon l'inclinaison (max +-6) */
    int offset = (int)(tilt / 4.0f);
    if (offset > 6)  offset = 6;
    if (offset < -6) offset = -6;

    /* Couleur selon la gravite */
    float abs_tilt = tilt < 0 ? -tilt : tilt;
    const char *color;
    if      (abs_tilt < 8.0f)  color = FG_GREEN;
    else if (abs_tilt < 15.0f) color = FG_YELLOW;
    else                       color = FG_RED;

    int base = 14;
    int s;

    s = base + offset;           if (s < 2) s = 2;
    printf("  %s%*s  |  |" RESET "\n", color, s, "");
    s = base + (offset * 4) / 5; if (s < 2) s = 2;
    printf("  %s%*s /    \\" RESET "\n", color, s, "");
    s = base + (offset * 2) / 5; if (s < 2) s = 2;
    printf("  %s%*s|      |" RESET "\n", color, s, "");
    s = base;
    printf("  %s%*s|______|" RESET "\n", color, s, "");
    printf("  " DIM "%*s==========" RESET "   %s%+.1f deg" RESET "\n",
           base - 1, "", color, tilt);
}

void *display_ui_task(void *arg)
{
    (void)arg;

    /* On configure la priorite SCHED_FIFO */
    struct sched_param param;
    param.sched_priority = 40;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        fprintf(stderr, "[display_ui] WARN: %s (sudo conseille)\n", strerror(ret));
    } else {
        printf("[display_ui] SCHED_FIFO prio=%d\n", param.sched_priority);
    }

    printf(HIDE_CURSOR);

    long next_deadline = 0;
    unsigned long prev_exec[NB_THREADS] = {0};

    while (system_running)
    {
        /* Debut : on note l'heure */
        long t0 = monotonic_us();
        if (next_deadline == 0) next_deadline = t0 + 200000L;

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_DISPLAY].is_running = 1;
        pthread_mutex_unlock(&data_mutex);

        SensorData    local_sensor;
        MapData       local_map;
        FallState     local_fall;
        ActuatorState local_actuator;
        ThreadStats   local_stats[NB_THREADS];

        /* Lecture protegee des donnees partagees */
        pthread_mutex_lock(&data_mutex);
        local_sensor   = sensor_data;
        local_map      = map_data;
        local_fall     = fall_state;
        local_actuator = actuator_state;
        for (int i = 0; i < NB_THREADS; i++)
            local_stats[i] = thread_stats[i];
        pthread_mutex_unlock(&data_mutex);


        printf(CLEAR_SCREEN);

        /* ====== TITRE ====== */
        printf("\n");
        printf(BOLD FG_CYAN
               "  ╔══════════════════════════════════════════════════════════╗\n"
               "  ║                    SAFEFEET BY NJIMA                     ║\n"
               "  ╚══════════════════════════════════════════════════════════╝\n"
               RESET);

        /* ====== ETAT + LED ====== */
        printf("\n  " BOLD FG_BLUE "▸ ETAT SYSTEME" RESET "     ");
        printf("%s %s " RESET, fall_state_color(local_fall), fall_state_text(local_fall));
        printf("     LED : %s\n", led_str(local_actuator.led_state));

        printf(DIM "  ──────────────────────────────────────────────────────────" RESET "\n");

        /* ====== CAPTEURS + TERRAIN sur 2 colonnes ====== */
        printf("\n  " BOLD FG_BLUE "▸ CAPTEURS" RESET "                             "
                    BOLD FG_BLUE "▸ TERRAIN" RESET "\n");
        printf("   Accel  : %5.2f      Tilt   : %6.1f deg      Trou      : %s\n",
               local_sensor.accel, local_sensor.tilt,
               local_map.hole_detected ? FG_RED "OUI" RESET : FG_GREEN "NON" RESET);
        printf("   Gyro   : %5.2f      Depth  : %6.2f m        Obstacle  : %s\n",
               local_sensor.gyro, local_sensor.depth,
               local_map.obstacle_detected ? FG_RED "OUI" RESET : FG_GREEN "NON" RESET);
        printf("   PressG : %5.1f %%    PressD : %5.1f %%         Glissant  : %s\n",
               local_sensor.pressure_left, local_sensor.pressure_right,
               local_map.slippery_surface ? FG_RED "OUI" RESET : FG_GREEN "NON" RESET);
        printf("   Pente  : %5.1f deg                           Risque    : %d/10\n",
               local_map.terrain_slope, local_map.terrain_risk_level);

        printf(DIM "\n  ──────────────────────────────────────────────────────────" RESET "\n");

        /* ====== ACTIONNEURS ====== */
        printf("\n  " BOLD FG_BLUE "▸ ACTIONNEURS" RESET "\n");
        printf("   Stabilisation  : %s      ",
               local_actuator.stabilization_on ? FG_YELLOW BOLD "ON " RESET : DIM "OFF" RESET);
        printf("Verrouil. cheville : %s\n",
               local_actuator.ankle_lock_on ? FG_YELLOW BOLD "ON " RESET : DIM "OFF" RESET);
        printf("   Vibration      : ");
        for (int i = 0; i < 3; i++) {
            if (i < local_actuator.vibration_level) {
                if (i == 0) printf(FG_GREEN "▮" RESET);
                else if (i == 1) printf(FG_YELLOW "▮" RESET);
                else printf(FG_RED "▮" RESET);
            } else printf(DIM "▯" RESET);
        }
        printf(" (%d/3)      ", local_actuator.vibration_level);
        printf("Buzzer             : %s\n",
               local_actuator.buzzer_on ? FG_RED BOLD "ON " RESET : DIM "OFF" RESET);

        printf(DIM "\n  ──────────────────────────────────────────────────────────" RESET "\n");

        /* ====== VISUALISATION PIED (temps reel) ====== */
        printf("\n  " BOLD FG_BLUE "▸ VISUALISATION PIED (temps reel)" RESET "\n\n");
        draw_ascii_foot(local_sensor.tilt);

        printf(DIM "\n  ──────────────────────────────────────────────────────────" RESET "\n");

        /* ====== ORDONNANCEMENT ====== */
        printf("\n  " BOLD FG_BLUE "▸ ORDONNANCEMENT TEMPS-REEL (SCHED_FIFO)" RESET "\n\n");
        printf(DIM "  %-18s %3s %5s %8s %7s %7s %3s %5s  Activite" RESET "\n",
               "Thread", "Pri", "Per", "Exec", "Last", "Max", "DL", "Etat");

        /* Trouver le max pour normaliser les barres */
        unsigned long max_exec = 1;
        for (int i = 0; i < NB_THREADS; i++) {
            if (local_stats[i].exec_count > max_exec)
                max_exec = local_stats[i].exec_count;
        }

        /* Tri par priorite decroissante */
        int order[NB_THREADS];
        for (int i = 0; i < NB_THREADS; i++) order[i] = i;
        for (int i = 0; i < NB_THREADS - 1; i++) {
            for (int j = i + 1; j < NB_THREADS; j++) {
                if (local_stats[order[j]].priority > local_stats[order[i]].priority) {
                    int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
                }
            }
        }

        for (int k = 0; k < NB_THREADS; k++) {
            int idx = order[k];
            ThreadStats *t = &local_stats[idx];

            /* Couleur selon la priorite */
            const char *color;
            if      (t->priority >= 80) color = FG_RED;
            else if (t->priority >= 70) color = FG_YELLOW;
            else if (t->priority >= 60) color = FG_CYAN;
            else if (t->priority >= 50) color = FG_GREEN;
            else                        color = DIM;

            /* Etat RUN/idle : RUN si la tache a tourne au moins une
               fois depuis le dernier rafraichissement de l'IHM */
            int active = (t->exec_count > prev_exec[idx]);
            const char *etat = active
                ? FG_GREEN BOLD "RUN " RESET
                : DIM      "idle" RESET;
            prev_exec[idx] = t->exec_count;

            /* Barre proportionnelle (16 chars) */
            int bar_len = (int)((t->exec_count * 16) / max_exec);
            if (bar_len < 0) bar_len = 0;
            if (bar_len > 16) bar_len = 16;

            printf("  %s%-18s %3d %3dms %8lu %5ldus %5ldus %3lu" RESET " %s ",
                   color, t->name, t->priority, t->period_ms,
                   t->exec_count, t->last_exec_us, t->max_exec_us,
                   t->deadline_missed, etat);
            mini_bar(bar_len, 16, color);
            printf("\n");
        }

        printf(DIM "\n  ──────────────────────────────────────────────────────────" RESET "\n");
        printf(DIM "  Ctrl+C pour quitter  |  sudo ./safefeet pour les priorites RT" RESET "\n");

        /* Fin : on calcule la duree et on met a jour les stats */
        long t1 = monotonic_us();
        long dur = t1 - t0;
        int missed = (t1 > next_deadline + 1000) ? 1 : 0;
        next_deadline += 200000L;

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_DISPLAY].is_running    = 0;
        thread_stats[THREAD_DISPLAY].exec_count++;
        thread_stats[THREAD_DISPLAY].last_exec_us  = dur;
        if (dur > thread_stats[THREAD_DISPLAY].max_exec_us)
            thread_stats[THREAD_DISPLAY].max_exec_us = dur;
        if (missed) thread_stats[THREAD_DISPLAY].deadline_missed++;
        pthread_mutex_unlock(&data_mutex);

        fflush(stdout);
        sleep_until_us(next_deadline);
    }

    printf(SHOW_CURSOR);
    printf(CLEAR_SCREEN);
    return NULL;
}
