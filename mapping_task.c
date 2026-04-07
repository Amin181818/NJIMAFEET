#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    mapping_task.c
    Tache qui analyse le terrain a partir des donnees capteurs.
    Priorite : 60  -  Periode : 50 ms
*/

/* Donne le temps en microsecondes */
static long monotonic_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

void *mapping_task(void *arg)
{
    (void)arg;

    /* On configure la priorite SCHED_FIFO */
    struct sched_param param;
    param.sched_priority = 60;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        fprintf(stderr, "[mapping] WARN: %s (sudo conseille)\n", strerror(ret));
    } else {
        printf("[mapping] SCHED_FIFO prio=%d\n", param.sched_priority);
    }

    long next_deadline = 0;

    while (system_running)
    {
        /* Debut : on note l'heure */
        long t0 = monotonic_us();
        if (next_deadline == 0) next_deadline = t0 + 50000L;

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_MAPPING].is_running = 1;
        pthread_mutex_unlock(&data_mutex);

        SensorData local_sensor;

        /* Lecture des donnees capteurs */
        pthread_mutex_lock(&data_mutex);
        local_sensor = sensor_data;
        pthread_mutex_unlock(&data_mutex);

        /* Analyse du terrain */
        int hole = 0;
        int obstacle = 0;
        int slippery = 0;
        float slope = 0.0f;
        float obs_distance = 0.0f;
        int risk_level = 0;

        /* Trou : profondeur elevee */
        if (local_sensor.depth > 0.15f) {
            hole = 1;
            risk_level += 2;
        }

        /* Obstacle : forte acceleration + profondeur moderee */
        if (local_sensor.accel > 1.8f && local_sensor.depth > 0.05f) {
            obstacle = 1;
            obs_distance = 2.0f - local_sensor.depth * 4.0f;
            if (obs_distance < 0.2f) obs_distance = 0.2f;
            risk_level += 1;
        }

        /* Surface glissante : gyro eleve + pression instable */
        float pressure_diff = fabsf(local_sensor.pressure_left - local_sensor.pressure_right);
        if (local_sensor.gyro > 3.0f && pressure_diff > 15.0f) {
            slippery = 1;
            risk_level += 1;
        }

        /* Pente */
        slope = fabsf(local_sensor.tilt);
        if (slope > 10.0f) risk_level += 1;

        /* Ecriture des donnees terrain */
        pthread_mutex_lock(&data_mutex);
        map_data.hole_detected = hole;
        map_data.obstacle_detected = obstacle;
        map_data.slippery_surface = slippery;
        map_data.terrain_slope = slope;
        map_data.obstacle_distance = obs_distance;
        map_data.terrain_risk_level = risk_level;
        pthread_mutex_unlock(&data_mutex);

        /* Fin : on calcule la duree et on met a jour les stats */
        long t1 = monotonic_us();
        long dur = t1 - t0;
        int missed = (t1 > next_deadline) ? 1 : 0;
        next_deadline += 50000L;

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_MAPPING].is_running    = 0;
        thread_stats[THREAD_MAPPING].exec_count++;
        thread_stats[THREAD_MAPPING].last_exec_us  = dur;
        if (dur > thread_stats[THREAD_MAPPING].max_exec_us)
            thread_stats[THREAD_MAPPING].max_exec_us = dur;
        if (missed) thread_stats[THREAD_MAPPING].deadline_missed++;
        pthread_mutex_unlock(&data_mutex);

        usleep(50000); /* periode 50 ms */
    }

    return NULL;
}
