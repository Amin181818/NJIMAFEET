#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include "shared_data.h"

/*
    SafeFeet by Njima
    main.c

    Point d'entree du systeme.
    Initialise les donnees partagees, cree les 5 threads
    avec ordonnancement SCHED_FIFO et priorites explicites,
    et attend leur terminaison.

    Politique d'ordonnancement : SCHED_FIFO
    ----------------------------------------
    SCHED_FIFO est le plus adapte pour un systeme de securite
    temps-reel : un thread de haute priorite preempte immediatement
    les threads de priorite inferieure, sans attendre de time-slice.
    Cela garantit que la detection de chute (priorite 60) reagit
    avant la stabilisation (55), les capteurs (45), la cartographie
    (40) et enfin l'IHM (10).

    Priorites SCHED_FIFO (echelle Linux : 1 - 99) :
      fall_detection      : 60  (reaction critique)
      stabilization_alert : 55  (commande actionneurs)
      sensor_simulation   : 45  (acquisition donnees)
      mapping             : 40  (traitement terrain)
      display_ui          : 10  (affichage non critique)

    Note : SCHED_FIFO necessite les privileges root (sudo).
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
/* Configuration d'un thread SCHED_FIFO  */
/* ===================================== */

typedef struct {
    void *(*func)(void *);
    int   priority;
    const char *name;
} ThreadConfig;

/*
 * Cree un thread avec politique SCHED_FIFO et priorite explicite.
 * Retourne 0 en succes, -1 en echec.
 */
static int create_rt_thread(pthread_t *tid, const ThreadConfig *cfg)
{
    pthread_attr_t attr;
    struct sched_param param;
    int ret;

    pthread_attr_init(&attr);

    /* Politique temps-reel SCHED_FIFO */
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret != 0)
    {
        fprintf(stderr, "[WARN] SCHED_FIFO indisponible pour %s (%s)\n",
                cfg->name, strerror(ret));
        /* Fallback : politique par defaut */
        pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    }

    /* Priorite explicite (ignore la priorite heritee du parent) */
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    param.sched_priority = cfg->priority;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(tid, &attr, cfg->func, NULL);

    pthread_attr_destroy(&attr);
    return ret;
}


/* ===================================== */
/* Main                                  */
/* ===================================== */

int main(void)
{
    pthread_t threads[NB_THREADS];

    /*
     * Table de configuration des threads.
     * Ordre de definition : du plus prioritaire au moins prioritaire.
     * Politique : SCHED_FIFO (preemption stricte par priorite).
     */
    ThreadConfig configs[NB_THREADS] = {
        { fall_detection_task,      60, "fall_detection"      },
        { stabilization_alert_task, 55, "stabilization_alert" },
        { sensor_simulation_task,   45, "sensor_simulation"   },
        { mapping_task,             40, "mapping"             },
        { display_ui_task,          10, "display_ui"          },
    };

    /* Installation du handler pour Ctrl+C */
    signal(SIGINT, signal_handler);

    printf("====================================================\n");
    printf("         SAFEFEET BY NJIMA - Demarrage...\n");
    printf("====================================================\n");
    printf(" Ordonnancement : SCHED_FIFO\n");
    printf(" (sudo requis pour les priorites temps-reel)\n");
    printf("====================================================\n\n");

    /* Creation des threads avec leurs priorites */
    for (int i = 0; i < NB_THREADS; i++)
    {
        if (create_rt_thread(&threads[i], &configs[i]) != 0)
        {
            fprintf(stderr, "Erreur creation thread %s\n", configs[i].name);
            system_running = 0;
            /* Attendre les threads deja lances */
            for (int j = 0; j < i; j++)
                pthread_join(threads[j], NULL);
            return EXIT_FAILURE;
        }
        printf("[OK] %-25s priorite SCHED_FIFO = %d\n",
               configs[i].name, configs[i].priority);
    }

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
