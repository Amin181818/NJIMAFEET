#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    stabilization_alert_task.c
    Tache qui commande les actionneurs selon l'etat du systeme.
    Priorite : 70  -  Periode : 35 ms
*/

/* Donne le temps en microsecondes */
static long monotonic_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

void *stabilization_alert_task(void *arg)
{
    (void)arg;

    /* On configure la priorite SCHED_FIFO */
    struct sched_param param;
    param.sched_priority = 70;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        fprintf(stderr, "[stabilization] WARN: %s (sudo conseille)\n", strerror(ret));
    } else {
        printf("[stabilization] SCHED_FIFO prio=%d\n", param.sched_priority);
    }

    while (system_running)
    {
        /* Debut : on note l'heure */
        long t0 = monotonic_us();

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_STAB_ALERT].is_running = 1;
        pthread_mutex_unlock(&data_mutex);

        FallState local_state;
        MapData local_map;

        /* Lecture des donnees partagees */
        pthread_mutex_lock(&data_mutex);
        local_state = fall_state;
        local_map = map_data;
        pthread_mutex_unlock(&data_mutex);

        /* Mise a jour des actionneurs */
        pthread_mutex_lock(&data_mutex);
        switch (local_state)
        {
        case STATE_NORMAL:
            actuator_state.stabilization_on = 0;
            actuator_state.ankle_lock_on = 0;
            actuator_state.vibration_level = 0;
            actuator_state.buzzer_on = 0;
            actuator_state.led_state = LED_GREEN;
            break;

        case STATE_WARNING:
            actuator_state.stabilization_on = 0;
            actuator_state.ankle_lock_on = 0;
            actuator_state.vibration_level = 1;
            actuator_state.buzzer_on = 0;
            actuator_state.led_state = LED_ORANGE;
            break;

        case STATE_FALL_RISK:
            actuator_state.stabilization_on = 1;
            actuator_state.ankle_lock_on = 1;
            actuator_state.vibration_level = 2;
            actuator_state.buzzer_on = 0;
            actuator_state.led_state = LED_ORANGE;
            break;

        case STATE_FALL_IMMINENT:
            actuator_state.stabilization_on = 1;
            actuator_state.ankle_lock_on = 1;
            actuator_state.vibration_level = 3;
            actuator_state.buzzer_on = 1;
            actuator_state.led_state = LED_RED;
            break;

        default:
            actuator_state.stabilization_on = 0;
            actuator_state.ankle_lock_on = 0;
            actuator_state.vibration_level = 0;
            actuator_state.buzzer_on = 0;
            actuator_state.led_state = LED_OFF;
            break;
        }

        /* Ajustements selon le terrain */
        if (local_map.slippery_surface) {
            actuator_state.vibration_level = 2;
        }
        if (local_map.hole_detected) {
            actuator_state.vibration_level = 3;
            actuator_state.led_state = LED_RED;
        }
        pthread_mutex_unlock(&data_mutex);

        /* Fin : on calcule la duree et on met a jour les stats */
        long dur = monotonic_us() - t0;
        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_STAB_ALERT].is_running    = 0;
        thread_stats[THREAD_STAB_ALERT].exec_count++;
        thread_stats[THREAD_STAB_ALERT].last_exec_us  = dur;
        if (dur > thread_stats[THREAD_STAB_ALERT].max_exec_us)
            thread_stats[THREAD_STAB_ALERT].max_exec_us = dur;
        if (dur > thread_stats[THREAD_STAB_ALERT].period_ms * 1000L)
            thread_stats[THREAD_STAB_ALERT].deadline_missed++;
        pthread_mutex_unlock(&data_mutex);

        usleep(35000); /* periode 35 ms */
    }

    return NULL;
}
