#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <pthread.h>

/* SafeFeet by Njima - donnees partagees entre threads */

#define NB_THREADS 5

/* Scenarios de simulation */
#define SCENARIO_NORMAL     0
#define SCENARIO_HOLE       1
#define SCENARIO_OBSTACLE   2
#define SCENARIO_SLIPPERY   3
#define SCENARIO_SLOPE      4
#define SCENARIO_COMBINED   5

/* Etats LED */
#define LED_OFF     0
#define LED_GREEN   1
#define LED_ORANGE  2
#define LED_RED     3

/* Etats du systeme */
typedef enum {
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_FALL_RISK,
    STATE_FALL_IMMINENT
} FallState;

/* Donnees des capteurs */
typedef struct {
    float accel;
    float tilt;
    float gyro;
    float pressure_left;
    float pressure_right;
    float depth;
    int   mode_scenario;
} SensorData;

/* Donnees du terrain */
typedef struct {
    int   hole_detected;
    int   obstacle_detected;
    int   slippery_surface;
    float terrain_slope;
    float obstacle_distance;
    int   terrain_risk_level;
} MapData;

/* Etat des actionneurs */
typedef struct {
    int stabilization_on;
    int ankle_lock_on;
    int vibration_level;
    int buzzer_on;
    int led_state;
} ActuatorState;

/* Variables partagees */
extern SensorData    sensor_data;
extern MapData       map_data;
extern FallState     fall_state;
extern ActuatorState actuator_state;

extern pthread_mutex_t data_mutex;
extern int system_running;

/* Prototypes des threads */
void *sensor_simulation_task(void *arg);
void *mapping_task(void *arg);
void *fall_detection_task(void *arg);
void *stabilization_alert_task(void *arg);
void *display_ui_task(void *arg);


/* ===================================== */
/* STATS D'ORDONNANCEMENT                */
/* ===================================== */

/* Index dans thread_stats[] */
#define THREAD_SENSOR       0
#define THREAD_MAPPING      1
#define THREAD_FALL_DET     2
#define THREAD_STAB_ALERT   3
#define THREAD_DISPLAY      4

typedef struct {
    unsigned long exec_count;       /* nombre de fois que la tache a tourne */
    int           priority;         /* priorite SCHED_FIFO */
    const char   *name;             /* nom de la tache */
    int           period_ms;        /* periode et deadline (ms) */
    long          last_exec_us;     /* duree de la derniere execution (us) */
    long          max_exec_us;      /* duree max observee (us) */
    unsigned long deadline_missed;  /* nombre de deadlines ratees */
    int           is_running;       /* 1 si en cours d'execution */
} ThreadStats;

extern ThreadStats thread_stats[NB_THREADS];

#endif
