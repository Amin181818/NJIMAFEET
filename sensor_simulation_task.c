#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    sensor_simulation_task.c
    Tache qui genere des donnees capteurs aleatoires.
    Priorite : 50  -  Periode : 80 ms
*/

/* Renvoie un float aleatoire entre min et max */
static float random_float(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

/* Marche normale : rien d'anormal */
static void generate_normal_walking(SensorData *d) {
    d->accel = random_float(0.8f, 1.5f);
    d->tilt  = random_float(-5.0f, 5.0f);
    d->gyro  = random_float(0.0f, 2.0f);
    d->pressure_left  = random_float(40.0f, 60.0f);
    d->pressure_right = random_float(40.0f, 60.0f);
    d->depth = random_float(0.00f, 0.05f);
    d->mode_scenario = 0;
}

/* Surface glissante */
static void generate_slippery_surface(SensorData *d) {
    d->accel = random_float(1.5f, 2.5f);
    d->tilt  = random_float(-12.0f, 12.0f);
    d->gyro  = random_float(2.0f, 5.0f);
    d->pressure_left  = random_float(20.0f, 45.0f);
    d->pressure_right = random_float(20.0f, 45.0f);
    d->depth = random_float(0.00f, 0.03f);
    d->mode_scenario = 1;
}

/* Obstacle devant */
static void generate_obstacle(SensorData *d) {
    d->accel = random_float(1.8f, 3.0f);
    d->tilt  = random_float(8.0f, 18.0f);
    d->gyro  = random_float(3.0f, 6.0f);
    d->pressure_left  = random_float(30.0f, 65.0f);
    d->pressure_right = random_float(30.0f, 65.0f);
    d->depth = random_float(0.08f, 0.20f);
    d->mode_scenario = 2;
}

/* Trou dans la chaussee */
static void generate_pothole(SensorData *d) {
    d->accel = random_float(2.0f, 3.5f);
    d->tilt  = random_float(10.0f, 20.0f);
    d->gyro  = random_float(4.0f, 7.0f);
    d->pressure_left  = random_float(15.0f, 40.0f);
    d->pressure_right = random_float(50.0f, 80.0f);
    d->depth = random_float(0.20f, 0.45f);
    d->mode_scenario = 3;
}

/* Perte d'equilibre */
static void generate_loss_of_balance(SensorData *d) {
    d->accel = random_float(2.5f, 4.5f);
    d->tilt  = random_float(15.0f, 30.0f);
    d->gyro  = random_float(5.0f, 10.0f);
    d->pressure_left  = random_float(10.0f, 30.0f);
    d->pressure_right = random_float(60.0f, 90.0f);
    d->depth = random_float(0.00f, 0.10f);
    d->mode_scenario = 4;
}

/* Choisit le bon scenario */
static void generate_sensor_data(SensorData *d, int scenario) {
    switch (scenario) {
        case 0: generate_normal_walking(d); break;
        case 1: generate_slippery_surface(d); break;
        case 2: generate_obstacle(d); break;
        case 3: generate_pothole(d); break;
        case 4: generate_loss_of_balance(d); break;
        default: generate_normal_walking(d); break;
    }
}

/* Donne le temps en microsecondes */
static long monotonic_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

void *sensor_simulation_task(void *arg) {
    (void)arg;

    /* On configure la priorite SCHED_FIFO */
    struct sched_param param;
    param.sched_priority = 50;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        fprintf(stderr, "[sensor] WARN: %s (sudo conseille)\n", strerror(ret));
    } else {
        printf("[sensor] SCHED_FIFO prio=%d\n", param.sched_priority);
    }

    srand((unsigned int)time(NULL));
    int scenario = 0;
    int counter  = 0;

    while (system_running) {
        /* Debut : on note l'heure */
        long t0 = monotonic_us();

        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_SENSOR].is_running = 1;
        pthread_mutex_unlock(&data_mutex);

        /* On change de scenario toutes les 8 iterations */
        if (counter % 8 == 0) scenario = rand() % 5;

        /* Generation des donnees */
        pthread_mutex_lock(&data_mutex);
        generate_sensor_data(&sensor_data, scenario);
        pthread_mutex_unlock(&data_mutex);

        counter++;

        /* Fin : on calcule la duree et on met a jour les stats */
        long dur = monotonic_us() - t0;
        pthread_mutex_lock(&data_mutex);
        thread_stats[THREAD_SENSOR].is_running    = 0;
        thread_stats[THREAD_SENSOR].exec_count++;
        thread_stats[THREAD_SENSOR].last_exec_us  = dur;
        if (dur > thread_stats[THREAD_SENSOR].max_exec_us)
            thread_stats[THREAD_SENSOR].max_exec_us = dur;
        if (dur > thread_stats[THREAD_SENSOR].period_ms * 1000L)
            thread_stats[THREAD_SENSOR].deadline_missed++;
        pthread_mutex_unlock(&data_mutex);

        usleep(80000); /* periode 80 ms */
    }

    return NULL;
}
