/* 
 * File:   thermal_elements.h
 * Author: Ruhan Louw
 *
 * Created on October 30, 2025, 6:57 PM
 */

#ifndef THERMAL_ELEMENTS_H
#define	THERMAL_ELEMENTS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define HEATER_DIRSET()     IO_PD0


void EEV_Init(void);
void HEATER_init(void);
void EEV_REG_update(void);

#ifdef	__cplusplus
}
#endif

#endif	/* THERMAL_ELEMENTS_H */

