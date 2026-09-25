/* 
 * File:   compressor_supervisor.h
 * Author: Ruhan Louw
 *
 * Created on December 4, 2025, 12:43 AM
 */

#ifndef COMPRESSOR_SUPERVISOR_H
#define	COMPRESSOR_SUPERVISOR_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>

/* Call once at startup */
void CompressorSupervisor_Init(void);

/* Call once per second ? decides actual compressor ON/OFF */
void CompressorSupervisor_Step_1s(void);


#ifdef	__cplusplus
}
#endif

#endif	/* COMPRESSOR_SUPERVISOR_H */

