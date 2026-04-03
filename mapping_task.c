#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include "shared_data.h"

/*
    SafeFeet by Njima
    mapping_task.c

    Simule la cartographie de l'environnement a partir des donnees capteurs.
    Analyse les donnees des cameras et du capteur de profondeur
    pour detecter : trou, obstacle, surface glissante, pente.
*/

void *mapping_task(void *arg)
{
    (void)arg;

    while (system_running)
    {
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

        /* Detection de trou : profondeur elevee */
        if (local_sensor.depth > 0.15f)
        {
            hole = 1;
            risk_level += 2;
        }

        /* Detection d'obstacle : forte acceleration + profondeur moderee */
        if (local_sensor.accel > 1.8f && local_sensor.depth > 0.05f)
        {
            obstacle = 1;
            obs_distance = 2.0f - local_sensor.depth * 4.0f;
            if (obs_distance < 0.2f)
                obs_distance = 0.2f;
            risk_level += 1;
        }

        /* Detection surface glissante : gyro eleve + pression instable */
        float pressure_diff = fabsf(local_sensor.pressure_left - local_sensor.pressure_right);
        if (local_sensor.gyro > 3.0f && pressure_diff > 15.0f)
        {
            slippery = 1;
            risk_level += 1;
        }

        /* Calcul de la pente a partir de l'inclinaison */
        slope = fabsf(local_sensor.tilt);

        if (slope > 10.0f)
        {
            risk_level += 1;
        }

        /* Ecriture des donnees terrain */
        pthread_mutex_lock(&data_mutex);

        map_data.hole_detected = hole;
        map_data.obstacle_detected = obstacle;
        map_data.slippery_surface = slippery;
        map_data.terrain_slope = slope;
        map_data.obstacle_distance = obs_distance;
        map_data.terrain_risk_level = risk_level;

        pthread_mutex_unlock(&data_mutex);

        usleep(300000); /* 300 ms */
    }

    return NULL;
}
