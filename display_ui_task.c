#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "shared_data.h"

/* ===================================== */
/* Fonctions utilitaires d'affichage     */
/* ===================================== */

static const char* fall_state_to_string(FallState state)
{
    switch(state)
    {
        case STATE_NORMAL:
            return "NORMAL";
        case STATE_WARNING:
            return "WARNING";
        case STATE_FALL_RISK:
            return "FALL_RISK";
        case STATE_FALL_IMMINENT:
            return "FALL_IMMINENT";
        default:
            return "UNKNOWN";
    }
}

static const char* led_state_to_string(int led)
{
    switch(led)
    {
        case LED_OFF:
            return "OFF";
        case LED_GREEN:
            return "GREEN";
        case LED_ORANGE:
            return "ORANGE";
        case LED_RED:
            return "RED";
        default:
            return "UNKNOWN";
    }
}

static const char* scenario_to_string(int scenario)
{
    switch(scenario)
    {
        case SCENARIO_NORMAL:
            return "NORMAL";
        case SCENARIO_HOLE:
            return "HOLE";
        case SCENARIO_OBSTACLE:
            return "OBSTACLE";
        case SCENARIO_SLIPPERY:
            return "SLIPPERY";
        case SCENARIO_SLOPE:
            return "SLOPE";
        case SCENARIO_COMBINED:
            return "COMBINED";
        default:
            return "UNKNOWN";
    }
}

/* ===================================== */
/* Thread IHM                            */
/* ===================================== */

void *display_ui_task(void *arg)
{
    (void)arg;

    while(system_running)
    {
        SensorData local_sensor;
        MapData local_map;
        FallState local_fall_state;
        ActuatorState local_actuator;

        /* Lecture protégée des données partagées */
        pthread_mutex_lock(&data_mutex);

        local_sensor = sensor_data;
        local_map = map_data;
        local_fall_state = fall_state;
        local_actuator = actuator_state;

        pthread_mutex_unlock(&data_mutex);

        /* Efface le terminal */
        system("clear");

        printf("====================================================\n");
        printf("                 SAFEFEET BY NJIMA                  \n");
        printf("====================================================\n\n");

        printf("--------------- DONNEES CAPTEURS -------------------\n");
        printf("Scenario              : %s\n", scenario_to_string(local_sensor.mode_scenario));
        printf("Acceleration          : %.2f\n", local_sensor.accel);
        printf("Inclinaison           : %.2f deg\n", local_sensor.tilt);
        printf("Gyroscope             : %.2f\n", local_sensor.gyro);
        printf("Pression pied gauche  : %.2f\n", local_sensor.pressure_left);
        printf("Pression pied droit   : %.2f\n", local_sensor.pressure_right);
        printf("Profondeur sol        : %.2f\n\n", local_sensor.depth);

        printf("---------------- CARTOGRAPHIE ----------------------\n");
        printf("Trou detecte          : %s\n", local_map.hole_detected ? "YES" : "NO");
        printf("Obstacle detecte      : %s\n", local_map.obstacle_detected ? "YES" : "NO");
        printf("Surface glissante     : %s\n", local_map.slippery_surface ? "YES" : "NO");
        printf("Pente terrain         : %.2f\n", local_map.terrain_slope);
        printf("Distance obstacle     : %.2f\n", local_map.obstacle_distance);
        printf("Niveau risque terrain : %d\n\n", local_map.terrain_risk_level);

        printf("---------------- ETAT SYSTEME ----------------------\n");
        printf("Etat global           : %s\n\n", fall_state_to_string(local_fall_state));

        printf("---------------- ACTIONNEURS -----------------------\n");
        printf("Stabilisation         : %s\n", local_actuator.stabilization_on ? "ON" : "OFF");
        printf("Maintien cheville     : %s\n", local_actuator.ankle_lock_on ? "ON" : "OFF");
        printf("Niveau vibration      : %d\n", local_actuator.vibration_level);
        printf("Buzzer                : %s\n", local_actuator.buzzer_on ? "ON" : "OFF");
        printf("LED                   : %s\n", led_state_to_string(local_actuator.led_state));

        printf("\n====================================================\n");

        usleep(200000); /* 200 ms */
    }

    pthread_exit(NULL);
}
