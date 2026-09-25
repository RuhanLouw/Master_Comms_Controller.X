
/*
    Ruhan Louw
    Master Thermal Controller
 
 */

#include "thermal_elements.h"
#include "system_registers.h"
#include "mcc_generated_files/timer/tca1.h"
#include "definitions.h"
#include "mcc_generated_files/system/pins.h"

#define EEV_PERIOD              6   // seconds
#define HEATER_PERIOD           1   // seconds
#define EEV_PRESCALER           1024
#define EEV_PRESCALER_bits      TCA_SINGLE_CLKSEL_DIV1024_gc

#define EEV_Start()             do { if(!(REG(MREG_FAULT_FLAGS)&FAULT_EEV_ERROR)){TCA1_Start();} } while(0)
#define EEV_Stop()              do { TCA1_Stop(); } while(0)

#define HEATER_Start()          do { if(!(REG(MREG_FAULT_FLAGS)&FAULT_AC_HEATER_ERROR)){/*////*/;} } while(0)
#define HEATER_Stop()           do { /*////*/} while(0)
   

void EEV_Init(void)
{
    TCA1_Stop();
    // Keep DIV1024 (MCC default)
    TCA1.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc;
    // Dual-slope, only WO2 (CMP2) enabled
    TCA1.SINGLE.CTRLB = TCA_SINGLE_WGMODE_DSTOP_gc | TCA_SINGLE_CMP2EN_bm;
    // EXACT 6.000 s period
    TCA1.SINGLE.PER = 0xB71A;   // 46 874
    // Start at 0%
    TCA1.SINGLE.CMP2 = 0;
    TCA1_Start();
}

void EEV_REG_update(void){
    if(REG(MREG_FAULT_FLAGS) & FAULT_EEV_ERROR){
//        LED_ERR_ON();
        // Handle ERROR
        return;
    }
    uint16_t duty = REG(MREG_EEV_PWM);
    // Dual-slope: high time = 2 × CMP2
    // So CMP2 = (percent × PER) / 100
    uint32_t cmp = (uint32_t)duty * 0xB71A;  // PER = 46874
    TCA1.SINGLE.CMP2 = (uint16_t)(cmp / 100UL);
}

void HEATER_init(void){
   PORTD.DIRSET |= PIN0_bm;
   PORTD.OUTCLR |= PIN0_bm;
}

void HEATER_REG_update(void){
    if(REG(MREG_ACTUATOR_STATES) & FAULT_AC_HEATER_ERROR){
        // turn off heater indefinitly 
//        LED_ERR_ON();
        return;
    }; 
    
    // use a timer for the heater:
    //      no TImer PWM
    //      PERIOD = OVERFLOW
    //      DUTYCYCLE = CATCH COMPARE
    
}


