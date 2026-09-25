/* 
 * File:   fan_control.h
 * Author: Ruhan Louw
 *
 * Created on December 4, 2025, 1:01 AM
 */

#ifndef FAN_CONTROL_H
#define	FAN_CONTROL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Initialise fan state registers and bitfield */
void FanControl_Init(void);

/* Called once per second ? updates fan states and actuator bits */
void FanControl_Step_1s(void);



#ifdef	__cplusplus
}
#endif

#endif	/* FAN_CONTROL_H */

