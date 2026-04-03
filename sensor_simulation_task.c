#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    SafeFeet by Njima
    sensor_simulation_task.c

    Simulation des donnees capteurs :
    - accelerometre
    - gyroscope
    - inclinaison (tilt)
    - pression pied gauche / droit
    - capteur de profondeur

    Priorite SCHED_FIFO : 50
    Scenarios simules :
    0 -> marche normale
    1 -> surface glissante
    2 -> obstacle
    3 -> nid de poule
    4 -> perte d'equilibre
*/

static float random_float(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

static void generate_normal_walking(SensorData *data) {
    data->accel = random_float(0.8f, 1.5f);
    data->tilt = random_float(-5.0f, 5.0f);
    data->gyro = random_float(0.0f, 2.0f);
    data->pressure_left = random_float(40.0f, 60.0f);
    data->pressure_right = random_float(40.0f, 60.0f);
    data->depth = random_float(0.00f, 0.05f);
    data->mode_scenario = 0;
}

static void generate_slippery_surface(SensorData *data) {
    data->accel = random_float(1.5f, 2.5f);
    data->tilt = random_float(-12.0f, 12.0f);
    data->gyro = random_float(2.0f, 5.0f);
    data->pressure_left = random_float(20.0f, 45.0f);
    data->pressure_right = random_float(20.0f, 45.0f);
    data->depth = random_float(0.00f, 0.03f);
    data->mode_scenario = 1;
}

static void generate_obstacle(SensorData *data) {
    data->accel = random_float(1.8f, 3.0f);
    data->tilt = random_float(8.0f, 18.0f);
    data->gyro = random_float(3.0f, 6.0f);
    data->pressure_left = random_float(30.0f, 65.0f);
    data->pressure_right = random_float(30.0f, 65.0f);
    data->depth = random_float(0.08f, 0.20f);
    data->mode_scenario = 2;
}

static void generate_pothole(SensorData *data) {
    data->accel = random_float(2.0f, 3.5f);
    data->tilt = random_float(10.0f, 20.0f);
    data->gyro = random_float(4.0f, 7.0f);
    data->pressure_left = random_float(15.0f, 40.0f);
    data->pressure_right = random_float(50.0f, 80.0f);
    data->depth = random_float(0.20f, 0.45f);
    data->mode_scenario = 3;
}

static void generate_loss_of_balance(SensorData *data) {
    data->accel = random_float(2.5f, 4.5f);
    data->tilt = random_float(15.0f, 30.0f);
    data->gyro = random_float(5.0f, 10.0f);
    data->pressure_left = random_float(10.0f, 30.0f);
    data->pressure_right = random_float(60.0f, 90.0f);
    data->depth = random_float(0.00f, 0.10f);
    data->mode_scenario = 4;
}

static void generate_sensor_data(SensorData *data, int scenario) {
    switch (scenario) {
        case 0:
            generate_normal_walking(data);
            break;
        case 1:
            generate_slippery_surface(data);
            break;
        case 2:
            generate_obstacle(data);
            break;
        case 3:
            generate_pothole(data);
            break;
        case 4:
            generate_loss_of_balance(data);
            break;
        default:
            generate_normal_walking(data);
            break;
    }
}

void* sensor_simulation_task(void* arg) {
    (void)arg;

    /* ================================================ */
    /* Configuration SCHED_FIFO - Priorite 50           */
    /* ================================================ */
    /* pthread_setschedparam permet de modifier la       */
    /* politique d'ordonnancement du thread courant.     */
    /* SCHED_FIFO = First In First Out temps-reel :      */
    /* le thread s'execute tant qu'il n'est pas bloque   */
    /* ou preempte par un thread de priorite superieure. */
    /* ================================================ */

    struct sched_param param;
    param.sched_priority = 50;  /* Priorite capteurs : 50 */

    int ret = pthread_setschedparam(
        pthread_self(),    /* Thread courant */
        SCHED_FIFO,        /* Politique temps-reel FIFO */
        &param             /* Parametres (priorite) */
    );

    if (ret != 0)
    {
        fprintf(stderr,
                "[sensor_simulation] WARN: pthread_setschedparam echoue: %s\n",
                strerror(ret));
        fprintf(stderr,
                "[sensor_simulation] Lancez avec sudo pour les priorites RT.\n");
    }
    else
    {
        printf("[sensor_simulation] SCHED_FIFO active, priorite = %d\n",
               param.sched_priority);
    }

    srand((unsigned int)time(NULL));

    int scenario = 0;
    int counter = 0;

    while (system_running) {

        /* change scenario every 8 iterations */
        if (counter % 8 == 0) {
            scenario = rand() % 5;
        }

        pthread_mutex_lock(&data_mutex);
        generate_sensor_data(&sensor_data, scenario);
        thread_stats[THREAD_SENSOR].exec_count++;
        pthread_mutex_unlock(&data_mutex);

        counter++;

        usleep(50000); /* 50 ms — rapide pour montrer l'ordonnancement */
    }

    pthread_exit(NULL);
}
