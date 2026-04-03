#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    SafeFeet by Njima
    fall_detection_task.c

    Cerveau securite du systeme.
    Analyse les donnees capteurs et terrain pour determiner
    le niveau de danger : NORMAL, WARNING, FALL_RISK, FALL_IMMINENT.

    Priorite SCHED_FIFO : 80 (la plus elevee du systeme)
    Ce thread est le plus critique : il decide du niveau de danger.
*/

void *fall_detection_task(void *arg)
{
    (void)arg;

    /* ================================================ */
    /* Configuration SCHED_FIFO - Priorite 80           */
    /* ================================================ */
    /* La detection de chute a la priorite la plus       */
    /* elevee du systeme. Elle preempte tous les autres  */
    /* threads pour garantir une reaction immediate      */
    /* en cas de danger.                                 */
    /* ================================================ */

    struct sched_param param;
    param.sched_priority = 80;  /* Priorite detection chute : 80 (max) */

    int ret = pthread_setschedparam(
        pthread_self(),    /* Thread courant */
        SCHED_FIFO,        /* Politique temps-reel FIFO */
        &param             /* Parametres (priorite) */
    );

    if (ret != 0)
    {
        fprintf(stderr,
                "[fall_detection] WARN: pthread_setschedparam echoue: %s\n",
                strerror(ret));
        fprintf(stderr,
                "[fall_detection] Lancez avec sudo pour les priorites RT.\n");
    }
    else
    {
        printf("[fall_detection] SCHED_FIFO active, priorite = %d\n",
               param.sched_priority);
    }

    while (system_running)
    {
        SensorData local_sensor;
        MapData local_map;

        /* Lecture des donnees partagees */
        pthread_mutex_lock(&data_mutex);
        local_sensor = sensor_data;
        local_map = map_data;
        pthread_mutex_unlock(&data_mutex);

        /* Score de danger (plus c'est haut, plus c'est grave) */
        int danger_score = 0;

        /* --- Analyse des capteurs corporels --- */

        /* Forte acceleration = mouvement brusque */
        if (local_sensor.accel > 3.0f)
            danger_score += 3;
        else if (local_sensor.accel > 2.0f)
            danger_score += 1;

        /* Forte inclinaison = desequilibre */
        float abs_tilt = fabsf(local_sensor.tilt);
        if (abs_tilt > 20.0f)
            danger_score += 3;
        else if (abs_tilt > 12.0f)
            danger_score += 2;
        else if (abs_tilt > 8.0f)
            danger_score += 1;

        /* Forte rotation = instabilite */
        if (local_sensor.gyro > 6.0f)
            danger_score += 3;
        else if (local_sensor.gyro > 4.0f)
            danger_score += 2;
        else if (local_sensor.gyro > 2.5f)
            danger_score += 1;

        /* Desequilibre de pression entre les pieds */
        float pressure_diff = fabsf(local_sensor.pressure_left - local_sensor.pressure_right);
        if (pressure_diff > 40.0f)
            danger_score += 3;
        else if (pressure_diff > 25.0f)
            danger_score += 2;
        else if (pressure_diff > 15.0f)
            danger_score += 1;

        /* Pression totale faible = pied qui decolle */
        float total_pressure = local_sensor.pressure_left + local_sensor.pressure_right;
        if (total_pressure < 40.0f)
            danger_score += 2;

        /* --- Analyse du terrain --- */

        if (local_map.hole_detected)
            danger_score += 3;

        if (local_map.obstacle_detected && local_map.obstacle_distance < 0.5f)
            danger_score += 2;
        else if (local_map.obstacle_detected)
            danger_score += 1;

        if (local_map.slippery_surface)
            danger_score += 2;

        if (local_map.terrain_slope > 15.0f)
            danger_score += 2;
        else if (local_map.terrain_slope > 10.0f)
            danger_score += 1;

        /* --- Decision finale --- */
        FallState new_state;

        if (danger_score >= 10)
            new_state = STATE_FALL_IMMINENT;
        else if (danger_score >= 6)
            new_state = STATE_FALL_RISK;
        else if (danger_score >= 3)
            new_state = STATE_WARNING;
        else
            new_state = STATE_NORMAL;

        /* Ecriture de l'etat */
        pthread_mutex_lock(&data_mutex);
        fall_state = new_state;
        thread_stats[THREAD_FALL_DET].exec_count++;
        pthread_mutex_unlock(&data_mutex);

        usleep(50000); /* 50 ms — rapide pour montrer l'ordonnancement */
    }

    return NULL;
}
