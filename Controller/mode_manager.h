/* 
 * File:   mode_manager.h
 * Author: Ruhan Louw
 *
 * Created on December 4, 2025, 12:24 AM
 */

#ifndef MODE_MANAGER_H
#define	MODE_MANAGER_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdint.h>

/* Initialise mode-related registers/state */
void ModeManager_Init(void);

/* Called once per second ? decides HEAT/COOL/FREE_COOL/IDLE/MANUAL */
void ModeManager_Step_1s(void);


#ifdef	__cplusplus
}
#endif

#endif	/* MODE_MANAGER_H */

