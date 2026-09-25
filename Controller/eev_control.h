/* 
 * File:   eev_control.h
 * Author: Ruhan Louw
 *
 * Created on December 3, 2025, 11:13 PM
 */

#ifndef EEV_CONTROL_H
#define	EEV_CONTROL_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdint.h>

/* Call once at startup */
void EEV_Control_Init(void);

/* Called every 1 s */
void EEV_Control_Step_1s(void);

/* Called every 6 s ? slower superheat adjustment tick */
void EEV_Control_Tick_6s(void);
void EEV_Control_Tick_1s(void);

#ifdef	__cplusplus
}
#endif

#endif	/* EEV_CONTROL_H */

