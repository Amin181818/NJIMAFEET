#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include "shared_data.h"

/*
    stabilization_alert.c
    ---------------------
    Tâche critique du système SafeFeet.

    Rôle :
    - Lire l'état du système (fall_state)
    - Commander les actionneurs
    - Réagir immédiatement aux situations dangereuses
*/

/* =============================== */
/* Fonction utilitaire temporisation */
/* =============================== */

static void sleep_ms(int ms)
{
    usleep(ms * 1000);
}


/* =============================== */
/* Thread Stabilisation / Alertes */
/* =============================== */

void *stabilization_alert_task(void *arg)
{
    (void)arg;

    /* Configuration priorité temps réel */

    struct sched_param param;
    param.sched_priority = 60;

    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    if (ret != 0)
    {
        fprintf(stderr,
                "Warning: impossible d'activer SCHED_FIFO (%s)\n",
                strerror(ret));
        fprintf(stderr,
                "Essayez de lancer le programme avec sudo.\n");
    }


    /* Boucle principale */

    while (system_running)
    {
        FallState local_state;
        MapData local_map;

        /* Lecture données partagées */

        pthread_mutex_lock(&data_mutex);

        local_state = fall_state;
        local_map = map_data;

        pthread_mutex_unlock(&data_mutex);


        /* Mise à jour des actionneurs */

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


        /* Ajustements selon terrain */

        if (local_map.slippery_surface)
        {
            actuator_state.vibration_level = 2;
        }

        if (local_map.hole_detected)
        {
            actuator_state.vibration_level = 3;
            actuator_state.led_state = LED_RED;
        }

        pthread_mutex_unlock(&data_mutex);


        /* Fréquence de la tâche */
        sleep_ms(100);
    }

    return NULL;
}
