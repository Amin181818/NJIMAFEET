#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "shared_data.h"

/*
    SafeFeet by Njima
    main.c

    Point d'entree du systeme.
    Initialise les donnees partagees, cree les 5 threads
    et attend leur terminaison.
*/

/* ===================================== */
/* Definition des variables partagees    */
/* ===================================== */

SensorData sensor_data = {0};
MapData map_data = {0};
FallState fall_state = STATE_NORMAL;
ActuatorState actuator_state = {0};

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

int system_running = 1;

/* ===================================== */
/* Gestion de l'arret propre (Ctrl+C)   */
/* ===================================== */

static void signal_handler(int sig)
{
    (void)sig;
    system_running = 0;
}

/* ===================================== */
/* Main                                  */
/* ===================================== */

int main(void)
{
    pthread_t threads[NB_THREADS];

    /* Installation du handler pour Ctrl+C */
    signal(SIGINT, signal_handler);

    printf("====================================================\n");
    printf("         SAFEFEET BY NJIMA - Demarrage...\n");
    printf("====================================================\n\n");

    /* Creation des 5 threads */

    if (pthread_create(&threads[0], NULL, sensor_simulation_task, NULL) != 0)
    {
        perror("Erreur creation thread sensor_simulation");
        return EXIT_FAILURE;
    }
    printf("[OK] Thread sensor_simulation demarre\n");

    if (pthread_create(&threads[1], NULL, mapping_task, NULL) != 0)
    {
        perror("Erreur creation thread mapping");
        return EXIT_FAILURE;
    }
    printf("[OK] Thread mapping demarre\n");

    if (pthread_create(&threads[2], NULL, fall_detection_task, NULL) != 0)
    {
        perror("Erreur creation thread fall_detection");
        return EXIT_FAILURE;
    }
    printf("[OK] Thread fall_detection demarre\n");

    if (pthread_create(&threads[3], NULL, stabilization_alert_task, NULL) != 0)
    {
        perror("Erreur creation thread stabilization_alert");
        return EXIT_FAILURE;
    }
    printf("[OK] Thread stabilization_alert demarre\n");

    if (pthread_create(&threads[4], NULL, display_ui_task, NULL) != 0)
    {
        perror("Erreur creation thread display_ui");
        return EXIT_FAILURE;
    }
    printf("[OK] Thread display_ui demarre\n");

    printf("\nSysteme en cours... Appuyez sur Ctrl+C pour arreter.\n\n");

    /* Attente de la fin de tous les threads */
    for (int i = 0; i < NB_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    /* Nettoyage */
    pthread_mutex_destroy(&data_mutex);

    printf("\n====================================================\n");
    printf("         SAFEFEET BY NJIMA - Arret propre.\n");
    printf("====================================================\n");

    return EXIT_SUCCESS;
}
