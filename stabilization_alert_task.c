#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "shared_data.h"

/*
    stabilization_alert.c
    ---------------------
    Tache critique du systeme SafeFeet.

    Role :
    - Lire l'etat du systeme (fall_state)
    - Commander les actionneurs
    - Reagir immediatement aux situations dangereuses

    Priorite : 55 (SCHED_FIFO) — definie dans main.c
*/

static void sleep_ms(int ms)
{
    usleep(ms * 1000);
}

void *stabilization_alert_task(void *arg)
{
    (void)arg;

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
