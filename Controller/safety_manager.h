/* 
 * File:   safety_manager.h
 * Author: Ruhan Louw
 *
 * Created on December 4, 2025, 1:06 AM
 */

#ifndef SAFETY_MANAGER_H
#define	SAFETY_MANAGER_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>

/* Initialise safety registers / internal state */
void SafetyManager_Init(void);

/* Called once per second ? evaluates faults and temps, updates safety flags */
void SafetyManager_Step_1s(void);


#ifdef	__cplusplus
}
#endif

#endif	/* SAFETY_MANAGER_H */

