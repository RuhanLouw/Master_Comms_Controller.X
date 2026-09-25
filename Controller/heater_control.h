/* 
 * File:   heater_control.h
 * Author: Ruhan Louw
 *
 * Created on December 4, 2025, 12:52 AM
 */

#ifndef HEATER_CONTROL_H
#define	HEATER_CONTROL_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdint.h>

/* Initialise heater-related state and registers */
void HeaterControl_Init(void);

/* Called once per second ? updates heater PWM / faults / bitfield */
void HeaterControl_Step_1s(void);

#ifdef	__cplusplus
}
#endif

#endif	/* HEATER_CONTROL_H */

