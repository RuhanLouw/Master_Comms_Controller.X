/* 
 * File:   thermal_controller.h
 * Author: Ruhan Louw
 *
 * Created on October 30, 2025, 6:57 PM
 */

#ifndef THERMAL_CONTROLLER_H
#define	THERMAL_CONTROLLER_H

#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif

#endif	/* THERMAL_CONTROLLER_H */


//------------------------ Period and Duty Functions ------------------------
//#include <avr/io.h>
//#include <stdint.h>
//
//// Prescaler and CPU frequency
//#define TCA_PRESCALER 1024UL
//#define F_CPU 16000000UL  // 16 MHz
//
//// -------------------------------
//// Convert period in ms to TCA0 PERBUF (dual-slope PWM)
//// -------------------------------
//void set_period_ms(uint16_t period_ms) {
//    // Tick time in seconds
//    double tick_time = (double)TCA_PRESCALER / (double)F_CPU;
//    double period_s = (double)period_ms / 1000.0;
//
//    // Dual-slope: PER counts = half of full period
//    uint16_t per_value = (uint16_t)(period_s / (2.0 * tick_time) + 0.5); // round
//    if(per_value > 0xFFFF) per_value = 0xFFFF; // clamp
//
//    // Write to buffered register
//    TCA0.SINGLE.PERBUF = per_value;
//}
//
//// -------------------------------
//// Convert duty cycle % to TCA0 CMP0BUF
//// -------------------------------
//void set_duty_percent(uint8_t duty_percent) {
//    // Read current PERBUF value
//    uint16_t per_value = TCA0.SINGLE.PERBUF;
//    if(per_value == 0) per_value = 1; // prevent division by zero
//
//    // Clamp duty
//    if(duty_percent > 100) duty_percent = 100;
//
//    // Calculate CMP0 value
//    uint16_t cmp_value = (uint16_t)((duty_percent / 100.0) * per_value + 0.5);
//    if(cmp_value > per_value) cmp_value = per_value;
//
//    // Write to buffered register
//    TCA0.SINGLE.CMP0BUF = cmp_value;
//}
// ---------------------------------------------------------------------------
