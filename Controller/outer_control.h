/* 
 * File:   outer_control.h
 * Author: Ruhan Louw
 *
 * Created on December 4, 2025, 12:10 AM
 */

#ifndef OUTER_CONTROL_H
#define	OUTER_CONTROL_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdint.h>

/* Initialise PID state (error history, integral, etc.) */
void OuterControl_Init(void);

/* Called once per second ? main outer temperature PID */
void OuterControl_Step_1s(void);


#ifdef	__cplusplus
}
#endif

#endif	/* OUTER_CONTROL_H */

