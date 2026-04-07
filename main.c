#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "shared_data.h"

/*
    SafeFeet by Njima - main.c
    Cree les 5 threads et attend leur fin.
    Ordonnancement : SCHED_FIFO (priorites configurees dans chaque thread).
*/

/* Variables partagees */
SensorData    sensor_data    = {0};
MapData       map_data       = {0};
FallState     fall_state     = STATE_NORMAL;
ActuatorState actuator_state = {0};

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
int system_running = 1;

/* Stats des taches : { 0, prio, nom, periode, 0, 0, 0, 0 } */
ThreadStats thread_stats[NB_THREADS] = {
    { 0, 50, "sensor_simulation",  80, 0, 0, 0, 0 },
    { 0, 60, "mapping",            50, 0, 0, 0, 0 },
    { 0, 80, "fall_detection",     20, 0, 0, 0, 0 },
    { 0, 70, "stabilization",      35, 0, 0, 0, 0 },
    { 0, 40, "display_ui",        200, 0, 0, 0, 0 },
};

/* Ctrl+C */
static void signal_handler(int sig)
{
    (void)sig;
    system_running = 0;
}

int main(void)
{
    pthread_t threads[NB_THREADS];

    signal(SIGINT, signal_handler);

    printf("=== SAFEFEET BY NJIMA - Demarrage ===\n");
    printf("Ordonnancement SCHED_FIFO (sudo conseille)\n\n");

    /* Lancement des threads */
    if (pthread_create(&threads[0], NULL, sensor_simulation_task, NULL) != 0) {
        perror("sensor_simulation");
        return EXIT_FAILURE;
    }
    if (pthread_create(&threads[1], NULL, mapping_task, NULL) != 0) {
        perror("mapping");
        return EXIT_FAILURE;
    }
    if (pthread_create(&threads[2], NULL, fall_detection_task, NULL) != 0) {
        perror("fall_detection");
        return EXIT_FAILURE;
    }
    if (pthread_create(&threads[3], NULL, stabilization_alert_task, NULL) != 0) {
        perror("stabilization_alert");
        return EXIT_FAILURE;
    }
    if (pthread_create(&threads[4], NULL, display_ui_task, NULL) != 0) {
        perror("display_ui");
        return EXIT_FAILURE;
    }

    /* On attend la fin de tous les threads */
    for (int i = 0; i < NB_THREADS; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&data_mutex);

    printf("\n=== SAFEFEET BY NJIMA - Arret ===\n");
    return EXIT_SUCCESS;
}
