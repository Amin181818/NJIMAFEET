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

    while (system_running)
    {
        /* Debut : on note l'heure */
        long t0 = monotonic_us();

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
        printf(BOLD FG_CYAN "  ╔════════ SAFEFEET BY NJIMA ════════╗" RESET);
        printf("   Etat : %s %s " RESET, fall_state_color(local_fall), fall_state_text(local_fall));
        printf("  LED : %s\n", led_str(local_actuator.led_state));

        /* ====== CAPTEURS + TERRAIN sur 2 colonnes ====== */
        printf(BOLD FG_BLUE "  ▸ CAPTEURS" RESET "                          " BOLD FG_BLUE "▸ TERRAIN" RESET "\n");
        printf("   Accel  : %5.2f   Tilt   : %6.1f deg    Trou      : %s\n",
               local_sensor.accel, local_sensor.tilt,
               local_map.hole_detected ? FG_RED "OUI" RESET : FG_GREEN "NON" RESET);
        printf("   Gyro   : %5.2f   Depth  : %6.2f m      Obstacle  : %s\n",
               local_sensor.gyro, local_sensor.depth,
               local_map.obstacle_detected ? FG_RED "OUI" RESET : FG_GREEN "NON" RESET);
        printf("   PressG : %5.1f   PressD : %5.1f         Glissant  : %s\n",
               local_sensor.pressure_left, local_sensor.pressure_right,
               local_map.slippery_surface ? FG_RED "OUI" RESET : FG_GREEN "NON" RESET);
        printf("   Pente  : %5.1f deg                       Risque    : %d\n",
               local_map.terrain_slope, local_map.terrain_risk_level);

        /* ====== ACTIONNEURS sur une ligne ====== */
        printf(BOLD FG_BLUE "  ▸ ACTIONNEURS  " RESET);
        printf("Stab:%s  ", local_actuator.stabilization_on ? FG_YELLOW "ON " RESET : DIM "OFF" RESET);
        printf("Cheville:%s  ", local_actuator.ankle_lock_on ? FG_YELLOW "ON " RESET : DIM "OFF" RESET);
        printf("Vibr:");
        for (int i = 0; i < 3; i++) {
            if (i < local_actuator.vibration_level) {
                if (i == 0) printf(FG_GREEN "▮" RESET);
                else if (i == 1) printf(FG_YELLOW "▮" RESET);
                else printf(FG_RED "▮" RESET);
            } else printf(DIM "▯" RESET);
        }
        printf("  Buzzer:%s\n", local_actuator.buzzer_on ? FG_RED BOLD "ON " RESET : DIM "OFF" RESET);

        /* ====== ORDONNANCEMENT ====== */
        printf(BOLD FG_BLUE "  ▸ ORDONNANCEMENT TEMPS-REEL (SCHED_FIFO)" RESET "\n");
        printf(DIM "  %-18s %3s %4s %8s %6s %6s %3s %4s  Activite" RESET "\n",
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

            /* Etat RUN/idle */
            const char *etat = t->is_running
                ? FG_GREEN BOLD "RUN " RESET
                : DIM      "idle" RESET;

            /* Last et Max en ms (avec 1 decimale) */
            float last_ms = t->last_exec_us / 1000.0f;
            float max_ms  = t->max_exec_us  / 1000.0f;

            /* Barre proportionnelle (12 chars) */
            int bar_len = (int)((t->exec_count * 12) / max_exec);
            if (bar_len < 0) bar_len = 0;
            if (bar_len > 12) bar_len = 12;

            printf("  %s%-18s %3d %3dms %8lu %5.1fm %5.1fm %3lu" RESET " %s ",
                   color, t->name, t->priority, t->period_ms,
                   t->exec_count, last_ms, max_ms, t->deadline_missed, etat);
            mini_bar(bar_len, 12, color);
            printf("\n");
        }

        printf(DIM "  Ctrl+C pour quitter  |  sudo ./safefeet pour les priorites RT" RESET "\n");

        /* Fin : on calcule la duree et on met a jour les stats */
        long dur = monotonic_us() - t0;
        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_DISPLAY].is_running    = 0;
        thread_stats[THREAD_DISPLAY].exec_count++;
        thread_stats[THREAD_DISPLAY].last_exec_us  = dur;
        if (dur > thread_stats[THREAD_DISPLAY].max_exec_us)
            thread_stats[THREAD_DISPLAY].max_exec_us = dur;
        if (dur > thread_stats[THREAD_DISPLAY].period_ms * 1000L)
            thread_stats[THREAD_DISPLAY].deadline_missed++;
        pthread_mutex_unlock(&data_mutex);

        fflush(stdout);
        usleep(200000); /* periode 200 ms */
    }

    printf(SHOW_CURSOR);
    printf(CLEAR_SCREEN);
    return NULL;
}
