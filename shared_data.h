#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <pthread.h>

/* ===================================== */
/* CONSTANTES DU PROJET SAFEFEET         */
/* ===================================== */

#define NB_THREADS 5

/* Scénarios de simulation
   Utilisés par : sensor_simulation.c et mapping.c */
#define SCENARIO_NORMAL     0
#define SCENARIO_HOLE       1
#define SCENARIO_OBSTACLE   2
#define SCENARIO_SLIPPERY   3
#define SCENARIO_SLOPE      4
#define SCENARIO_COMBINED   5


/* Etats LED
   Utilisés par : stabilization_alert.c et display_ui.c */
#define LED_OFF     0
#define LED_GREEN   1
#define LED_ORANGE  2
#define LED_RED     3


/* ===================================== */
/* ETAT GLOBAL DU SYSTEME                */
/* ===================================== */

/* Utilisé par :
   - fall_detection.c
   - stabilization_alert.c
   - display_ui.c */

typedef enum {
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_FALL_RISK,
    STATE_FALL_IMMINENT
} FallState;


/* ===================================== */
/* DONNEES CAPTEURS (simulation utilisateur)
   ECRIT PAR : sensor_simulation.c
   LU PAR : mapping.c, fall_detection.c, display_ui.c
===================================== */

typedef struct {

    float accel;          /* acceleration globale */
    float tilt;           /* inclinaison utilisateur */
    float gyro;           /* rotation */
    float pressure_left;  /* pression pied gauche */
    float pressure_right; /* pression pied droit */

    float depth;          /* profondeur sol (capteur profondeur) */

    int mode_scenario;    /* scénario de simulation */

} SensorData;


/* ===================================== */
/* CARTOGRAPHIE DU TERRAIN
   ECRIT PAR : mapping.c
   LU PAR : fall_detection.c et display_ui.c
===================================== */

typedef struct {

    int hole_detected;       /* trou dans la chaussée */
    int obstacle_detected;   /* obstacle détecté */

    int slippery_surface;    /* surface glissante */

    float terrain_slope;     /* pente du terrain */

    float obstacle_distance; /* distance obstacle */

    int terrain_risk_level;  /* niveau global de risque terrain */

} MapData;


/* ===================================== */
/* ACTIONNEURS DU SYSTEME
   ECRIT PAR : stabilization_alert.c
   LU PAR : display_ui.c
===================================== */

typedef struct {

    int stabilization_on;  /* stabilisation active */

    int ankle_lock_on;     /* maintien cheville */

    int vibration_level;   /* niveau vibration */

    int buzzer_on;         /* buzzer alerte */

    int led_state;         /* LED état système */

} ActuatorState;


/* ===================================== */
/* VARIABLES PARTAGEES ENTRE THREADS     */
/* ===================================== */

/* Données capteurs */
extern SensorData sensor_data;

/* Cartographie terrain */
extern MapData map_data;

/* Etat du système */
extern FallState fall_state;

/* Etat des actionneurs */
extern ActuatorState actuator_state;


/* Mutex pour protéger les accès concurrents */
extern pthread_mutex_t data_mutex;


/* Variable globale pour arrêter les threads si besoin */
extern int system_running;


/* ===================================== */
/* PROTOTYPES DES THREADS                */
/* ===================================== */

/* Simulation capteurs */
void *sensor_simulation_task(void *arg);

/* Cartographie terrain */
void *mapping_task(void *arg);

/* Détection de chute */
void *fall_detection_task(void *arg);

/* Stabilisation et alertes */
void *stabilization_alert_task(void *arg);

/* Interface utilisateur */
void *display_ui_task(void *arg);


/* ===================================== */
/* STATISTIQUES D'ORDONNANCEMENT         */
/* ===================================== */

/* Index des threads dans le tableau thread_stats[] */
#define THREAD_SENSOR       0
#define THREAD_MAPPING      1
#define THREAD_FALL_DET     2
#define THREAD_STAB_ALERT   3
#define THREAD_DISPLAY      4

/* Structure de suivi par thread */
typedef struct {
    unsigned long exec_count;   /* nombre total d'executions de la boucle */
    int           priority;     /* priorite SCHED_FIFO configuree */
    const char   *name;         /* nom du thread pour l'affichage */
} ThreadStats;

/* Tableau global des stats (un par thread) */
extern ThreadStats thread_stats[NB_THREADS];


#endif
