#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    fall_detection_task.c
    Tache critique : decide du niveau de danger.
    Priorite : 80 (la plus haute)  -  Periode : 20 ms
*/

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

void *fall_detection_task(void *arg)
{
    (void)arg;

    /* On configure la priorite SCHED_FIFO */
    struct sched_param param;
    param.sched_priority = 80;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        fprintf(stderr, "[fall_detection] WARN: %s (sudo conseille)\n", strerror(ret));
    } else {
        printf("[fall_detection] SCHED_FIFO prio=%d\n", param.sched_priority);
    }

    long next_deadline = 0;

    while (system_running)
    {
        /* Debut : on note l'heure */
        long t0 = monotonic_us();
        if (next_deadline == 0) next_deadline = t0 + 20000L;

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_FALL_DET].is_running = 1;
        pthread_mutex_unlock(&data_mutex);

        SensorData local_sensor;
        MapData local_map;

        /* Lecture des donnees partagees */
        pthread_mutex_lock(&data_mutex);
        local_sensor = sensor_data;
        local_map = map_data;
        pthread_mutex_unlock(&data_mutex);

        /* Score de danger */
        int danger_score = 0;

        /* Acceleration */
        if (local_sensor.accel > 3.0f) danger_score += 3;
        else if (local_sensor.accel > 2.0f) danger_score += 1;

        /* Inclinaison */
        float abs_tilt = fabsf(local_sensor.tilt);
        if (abs_tilt > 20.0f) danger_score += 3;
        else if (abs_tilt > 12.0f) danger_score += 2;
        else if (abs_tilt > 8.0f) danger_score += 1;

        /* Rotation */
        if (local_sensor.gyro > 6.0f) danger_score += 3;
        else if (local_sensor.gyro > 4.0f) danger_score += 2;
        else if (local_sensor.gyro > 2.5f) danger_score += 1;

        /* Difference de pression entre les pieds */
        float pressure_diff = fabsf(local_sensor.pressure_left - local_sensor.pressure_right);
        if (pressure_diff > 40.0f) danger_score += 3;
        else if (pressure_diff > 25.0f) danger_score += 2;
        else if (pressure_diff > 15.0f) danger_score += 1;

        /* Pression totale faible = pied qui decolle */
        float total_pressure = local_sensor.pressure_left + local_sensor.pressure_right;
        if (total_pressure < 40.0f) danger_score += 2;

        /* Terrain */
        if (local_map.hole_detected) danger_score += 3;
        if (local_map.obstacle_detected && local_map.obstacle_distance < 0.5f) danger_score += 2;
        else if (local_map.obstacle_detected) danger_score += 1;
        if (local_map.slippery_surface) danger_score += 2;
        if (local_map.terrain_slope > 15.0f) danger_score += 2;
        else if (local_map.terrain_slope > 10.0f) danger_score += 1;

        /* Decision finale */
        FallState new_state;
        if (danger_score >= 10)     new_state = STATE_FALL_IMMINENT;
        else if (danger_score >= 6) new_state = STATE_FALL_RISK;
        else if (danger_score >= 3) new_state = STATE_WARNING;
        else                        new_state = STATE_NORMAL;

        /* Ecriture de l'etat */
        pthread_mutex_lock(&data_mutex);
        fall_state = new_state;
        pthread_mutex_unlock(&data_mutex);

        /* Fin : on calcule la duree et on met a jour les stats */
        long t1 = monotonic_us();
        long dur = t1 - t0;
        /* Tolerance 1 ms pour absorber le jitter du scheduler */
        int missed = (t1 > next_deadline + 1000) ? 1 : 0;
        next_deadline += 20000L;

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_FALL_DET].is_running    = 0;
        thread_stats[THREAD_FALL_DET].exec_count++;
        thread_stats[THREAD_FALL_DET].last_exec_us  = dur;
        if (dur > thread_stats[THREAD_FALL_DET].max_exec_us)
            thread_stats[THREAD_FALL_DET].max_exec_us = dur;
        if (missed) thread_stats[THREAD_FALL_DET].deadline_missed++;
        pthread_mutex_unlock(&data_mutex);

        /* On dort jusqu'a la prochaine deadline absolue */
        sleep_until_us(next_deadline);
    }

    return NULL;
}
