/* 
 * File:   trajectory_generator.h
 * Author: Ruhan Louw
 *
 * Created on December 3, 2025, 11:58 PM
 */

#ifndef TRAJECTORY_GENERATOR_H
#define	TRAJECTORY_GENERATOR_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Initialise trajectory to current chamber setpoint or temperature */
void SetpointTraj_Init(void);

/* Called once per second (1 s tick) */
void SetpointTraj_Step_1s(void);

#ifdef	__cplusplus
}
#endif

#endif	/* TRAJECTORY_GENERATOR_H */

