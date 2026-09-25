/* 
 * File:   pwm_sw.h
 * Author: Ruhan Louw
 *
 * Created on August 17, 2025, 6:57 PM
 */

#ifndef PWM_SW_H
#define	PWM_SW_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "../../mcc_generated_files/timer/tca1.h"
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
    
    typedef struct {
        uint16_t eev_p;
        uint8_t finHeater_p;
        uint8_t heatFan_p;
        uint8_t coolFan_p;
        uint8_t headBed_1_p;
        uint8_t heatBed_2_p;
    }pwm_period_t;
    

// PWM pin definitions
#define DC_HEAT_FAN_PWM_bm PIN1_bm // PD1 (WO1/LCMP1)
#define DC_EVAP_FAN_PWM_bm PIN2_bm // PD2 (WO2/LCMP2)
#define DC_Heater_Resistor_1_PWM_bm PIN3_bm // PD3 (WO3/HCMP0)
#define DC_Heater_Resistor_2_PWM_bm PIN4_bm // PD4 (WO4/HCMP1)

// Initialize TCA0 for Split mode PWM
bool PWM_TCA0_Initialize(void);

// Enable TCA0 PWM output
void PWM_TCA0_Enable(void);

// Disable TCA0 PWM output
void PWM_TCA0_Disable(void);

// Set period for Low timer (PD1-PD2) in milliseconds
bool PWM_TCA0_SetLowPeriod(float period_ms);

// Set period for High timer (PD3-PD4) in milliseconds
bool PWM_TCA0_SetHighPeriod(float period_ms);

// Set duty cycle for DC_HEAT_FAN_PWM (PD1, WO1/LCMP1)
bool PWM_TCA0_SetDutyCycle_DC_Heat_Fan(uint8_t duty_cycle);

// Set duty cycle for DC_EVAP_FAN_PWM (PD2, WO2/LCMP2)
bool PWM_TCA0_SetDutyCycle_DC_Evap_Fan(uint8_t duty_cycle);

// Set duty cycle for DC_Heater_Resistor_1_PWM (PD3, WO3/HCMP0)
bool PWM_TCA0_SetDutyCycle_DC_Heater_1(uint8_t duty_cycle);

// Set duty cycle for DC_Heater_Resistor_2_PWM (PD4, WO4/HCMP1)
bool PWM_TCA0_SetDutyCycle_DC_Heater_2(uint8_t duty_cycle);

#ifdef	__cplusplus
}
#endif

#endif	/* PWM_SW_H */

