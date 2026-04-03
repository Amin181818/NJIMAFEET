#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#include "shared_data.h"

/*
    stabilization_alert_task.c
    -------------------------
    Tache critique du systeme SafeFeet.

    Role :
    - Lire l'etat du systeme (fall_state)
    - Commander les actionneurs
    - Reagir immediatement aux situations dangereuses

    Priorite SCHED_FIFO : 70
*/

void *stabilization_alert_task(void *arg)
{
    (void)arg;

    /* ================================================ */
    /* Configuration SCHED_FIFO - Priorite 70           */
    /* ================================================ */
    /* La stabilisation doit reagir vite apres la        */
    /* detection de chute (80) pour commander les         */
    /* actionneurs sans delai.                           */
    /* ================================================ */

    struct sched_param param;
    param.sched_priority = 60;  /* Priorite stabilisation : 70 */

    int ret = pthread_setschedparam(
        pthread_self(),    /* Thread courant */
        SCHED_FIFO,        /* Politique temps-reel FIFO */
        &param             /* Parametres (priorite) */
    );

    if (ret != 0)
    {
        fprintf(stderr,
                "[stabilization] WARN: pthread_setschedparam echoue: %s\n",
                strerror(ret));
        fprintf(stderr,
                "[stabilization] Lancez avec sudo pour les priorites RT.\n");
    }
    else
    {
        printf("[stabilization] SCHED_FIFO active, priorite = %d\n",
               param.sched_priority);
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

        thread_stats[THREAD_STAB_ALERT].exec_count++;

        pthread_mutex_unlock(&data_mutex);


        /* Fréquence de la tâche */
        usleep(50000); /* 50 ms — rapide pour montrer l'ordonnancement */
    }

    return NULL;
}
