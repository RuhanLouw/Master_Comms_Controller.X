
#include "eev_control.h"
#include "../system_registers.h"
#include "../mcc_generated_files/timer/tca1.h"
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

/*
 * EEV CONTROL (PWM in %)
 *
 * Registers expected (PWM = 0..100):
 *   MREG_MANUAL_EEV_PWM    0..100
 *   MREG_EEV_PWM           0..100
 *   MREG_EEV_MIN_PWM       0..100
 *   MREG_EEV_MAX_PWM       0..100
 *
 * Superheat/?T units:
 *   MREG_SUPERHEAT         °C×10   (your ?T = Tout - Tin)
 *   MREG_SH_TARGET         °C×10
 *
 * Tuning registers (recommended meanings):
 *   MREG_EEV_KP_X10        (PWM% per °C) ×10
 *      Example: 25 => 2.5 % per °C (velocity P term acts on ?error)
 *
 *   MREG_EEV_KI_X100       (PWM% per °C per second) ×100
 *      Example: 10 => 0.10 %/(°C·s)
 *
 *   MREG_SH_FILT_DIV       1..N  (simple IIR: filt += (meas-filt)/div)
 *   MREG_EEV_MAX_STEP      PWM% per second (slew limit), e.g. 1..20
 */

typedef enum {
    EEV_STATE_OFF     = 0,
    EEV_STATE_STARTUP = 1,
    EEV_STATE_CONTROL = 2,
    EEV_STATE_MANUAL  = 3
} eev_state_t;

/* Startup delay to let evaporator pull down (seconds) */
#define EEV_STARTUP_DELAY_S      6U

/* ?T sanity range (°C×10) */
#define EEV_SH_MIN_X10           0
#define EEV_SH_MAX_X10           600   /* 60.0°C */

/* Deadband around target (°C×10) */
#define EEV_ERR_DEADBAND_X10     5     /* 0.5°C */

/* Fault bits */
#define EEV_FAULT_SH_INVALID     (1U << 0)

/* --------------------------------------------------------------------------
 * Local state
 * -------------------------------------------------------------------------- */
static bool        eev_startup_done    = false;
static uint16_t    eev_startup_timer_s = 0;
static bool        prev_compressor_active = false;

/* Controller internals (velocity PI) */
static int16_t  err_prev_x10  = 0;
static int16_t  sh_filt_x10   = 0;
static bool     sh_filt_init  = false;

/* --------------------------------------------------------------------------
 * Small helpers (no pointers/structs)
 * -------------------------------------------------------------------------- */
static inline uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline int32_t clamp_i32(int32_t x, int32_t lo, int32_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void EEV_SetState(eev_state_t st)
{
    REG(MREG_EEV_STATE) = (uint16_t)st;
}

static void EEV_ResetController(void)
{
    err_prev_x10 = 0;
    sh_filt_x10  = 0;
    sh_filt_init = false;
}

/* Apply REG(MREG_EEV_PWM) (0..100 %) to TCA1 CMP2BUF */
static void EEV_ApplyPwmToHardware(void)
{
    uint16_t pwm = REG(MREG_EEV_PWM);
    if (pwm > 100) pwm = 100;

    uint16_t per = TCA1.SINGLE.PER;
    uint32_t ticks = ((uint32_t)pwm * (uint32_t)per) / 100U;

    TCA1_Compare2BufferSet((uint16_t)ticks);
}

/* Update ACT_PWM_COOL_EEV bit */
static void EEV_UpdateActuatorBit(void)
{
    uint16_t bits = REG(MREG_ACTUATOR_STATES);

    if (REG(MREG_EEV_PWM) > 0) bits |=  ACT_PWM_COOL_EEV;
    else                       bits &= (uint16_t)~ACT_PWM_COOL_EEV;

    REG(MREG_ACTUATOR_STATES) = bits;
}

/* Fully close valve and mark OFF */
static void EEV_CloseValve(void)
{
    REG(MREG_EEV_PWM)            = 0;
    REG(MREG_EEV_STARTUP_ACTIVE) = 0;
    EEV_SetState(EEV_STATE_OFF);
    EEV_UpdateActuatorBit();
    EEV_ApplyPwmToHardware();
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void EEV_Control_Init(void)
{
    eev_startup_done    = false;
    eev_startup_timer_s = 0;
    prev_compressor_active = false;

    REG(MREG_EEV_PWM)            = 0;
    REG(MREG_EEV_STARTUP_ACTIVE) = 0;
    REG(MREG_EEV_FAULT)          = 0;
    EEV_SetState(EEV_STATE_OFF);

    EEV_ResetController();
    EEV_CloseValve();
}

/*
 * Call every 1 second.
 * Handles: safety, manual, startup, and applying PWM to hardware.
 */
void EEV_Control_Step_1s(void)
{
    uint16_t system_mode      = REG(MREG_SYSTEM_MODE);       /* 0=Auto, 1=Manual */
    uint16_t safety_active    = REG(MREG_SAFETY_ACTIVE);
    uint16_t compressor_state = REG(MREG_COMPRESSOR_STATE);  /* 0/1 */

    /* ---------------- Safety override ---------------- */
    if (safety_active) {
        eev_startup_done    = false;
        eev_startup_timer_s = 0;
        EEV_ResetController();
        EEV_CloseValve();
        REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_EEV_ERROR;
        prev_compressor_active = false;
        return;
    }

    /* ---------------- MANUAL MODE ---------------- */
    if (system_mode != 0) {
        uint16_t man_pwm = REG(MREG_MANUAL_EEV_PWM);
        uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
        uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);

        /* sanitize limits */
        if (min_pwm > max_pwm) { uint16_t t=min_pwm; min_pwm=max_pwm; max_pwm=t; }

        /* Allow 0% in manual even if min_pwm > 0 (so you can close valve) */
        if (man_pwm > 0) man_pwm = clamp_u16(man_pwm, min_pwm, max_pwm);
        else             man_pwm = 0;

        REG(MREG_EEV_PWM)            = man_pwm;
        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
        REG(MREG_EEV_FAULT)          = 0;
        EEV_SetState(EEV_STATE_MANUAL);

        /* Don?t keep old control memory when in manual */
        EEV_ResetController();

        EEV_UpdateActuatorBit();
        EEV_ApplyPwmToHardware();

        prev_compressor_active = false;
        return;
    }

    /* ---------------- AUTO MODE: basic checks ---------------- */

    if ((compressor_state == 0)) {
        /* Not actively cooling -> valve closed */
        eev_startup_done    = false;
        eev_startup_timer_s = 0;
        EEV_ResetController();
        EEV_CloseValve();
        REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_EEV_ERROR;
        prev_compressor_active = false;
        return;
    }

    /* AUTO, safety ok, compressor running */

    /* Start with the valve closed whenever the compressor starts. */
    if (!prev_compressor_active) {
        eev_startup_done    = false;
        eev_startup_timer_s = EEV_STARTUP_DELAY_S;
        REG(MREG_EEV_STARTUP_ACTIVE) = 1;
        REG(MREG_EEV_PWM)            = 0;
        EEV_SetState(EEV_STATE_STARTUP);

        /* Reset controller at the start of a cooling cycle */
        EEV_ResetController();
    }

    /* Startup countdown: keep valve closed */
    if (!eev_startup_done && (eev_startup_timer_s > 0)) {
        eev_startup_timer_s--;
        REG(MREG_EEV_PWM) = 0;
        EEV_SetState(EEV_STATE_STARTUP);
        EEV_UpdateActuatorBit();
        EEV_ApplyPwmToHardware();
        prev_compressor_active = true;
        return;
    }

    /* Startup done: open to MIN PWM once */
    if (!eev_startup_done && (eev_startup_timer_s <= 0)) {
        uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
        uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
        if (min_pwm > max_pwm) { uint16_t t=min_pwm; min_pwm=max_pwm; max_pwm=t; }

        /* clamp min within 0..max */
        if (min_pwm > 100) min_pwm = 100;
        if (max_pwm > 100) max_pwm = 100;
        if (min_pwm > max_pwm) min_pwm = max_pwm;

        REG(MREG_EEV_PWM)            = min_pwm;
        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
        EEV_SetState(EEV_STATE_CONTROL);
        eev_startup_done             = true;

        /* reset controller memory so first control step is clean */
        EEV_ResetController();
    }

    /* Apply current PWM to hardware once per second */
    EEV_UpdateActuatorBit();
    EEV_ApplyPwmToHardware();

    prev_compressor_active = true;
}

/*
 * Recommended: call every 1 second (not 6s).
 * Closed-loop controller (velocity PI + rate limit).
 *
 * Works well for a plant where PWM leads and ?T lags (~6s).
 */
void EEV_Control_Tick_1s(void)
{
    uint16_t system_mode      = REG(MREG_SYSTEM_MODE);
    uint16_t safety_active    = REG(MREG_SAFETY_ACTIVE);
    uint16_t compressor_state = REG(MREG_COMPRESSOR_STATE);

    /* Only run in AUTO while actively cooling */
    if (system_mode != 0){  
        return;
    }
    if (safety_active){
        eev_startup_done    = false;
        eev_startup_timer_s = 0;
        EEV_ResetController();
        EEV_CloseValve();
        REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_EEV_ERROR;
        prev_compressor_active = false;
        return;
    }
    if (compressor_state == 0) {
        eev_startup_done    = false;
        eev_startup_timer_s = 0;
        EEV_ResetController();
        EEV_CloseValve();
        REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_EEV_ERROR;
        prev_compressor_active = false;
        return;
    }
    if (!eev_startup_done) return;

    int16_t sh_x10     = (int16_t)REG(MREG_SUPERHEAT);   /* ?T (°C×10) */
    int16_t sh_tgt_x10 = (int16_t)REG(MREG_SH_TARGET);

    /* Filter divider (avoid /0, keep sane) */
    int16_t div = (int16_t)REG(MREG_SH_FILT_DIV);
    if (div < 1) div = 1;
    if (div > 50) div = 50;   /* avoid extreme lag */

    /* 1st-order IIR filter: removes bounce, minimal lag */
    if (!sh_filt_init) {
        sh_filt_x10  = sh_x10;
        sh_filt_init = true;
    } else {
        int32_t filter_delta = ((int32_t)sh_x10 - (int32_t)sh_filt_x10) / div;
        sh_filt_x10 = (int16_t)((int32_t)sh_filt_x10 + filter_delta);
    }

    /* Sanity check */
    if ((sh_filt_x10 < (int16_t)EEV_SH_MIN_X10) || (sh_filt_x10 > (int16_t)EEV_SH_MAX_X10)) {
        REG(MREG_EEV_FAULT) |= EEV_FAULT_SH_INVALID;
        REG(MREG_FAULT_FLAGS) |= FAULT_EEV_ERROR;
        EEV_CloseValve();
        return;
    } else {
        REG(MREG_EEV_FAULT) &= (uint16_t)~EEV_FAULT_SH_INVALID;
        REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_EEV_ERROR;
    }

    /* Error: positive => ?T too high => open valve more => PWM UP */
    int32_t error = (int32_t)sh_filt_x10 - (int32_t)sh_tgt_x10;

    /* Deadband */
    if ((error >= -(int32_t)EEV_ERR_DEADBAND_X10) &&
        (error <= (int32_t)EEV_ERR_DEADBAND_X10)) {
        error = 0;
    }
    int16_t err_x10 = (int16_t)clamp_i32(error, INT16_MIN, INT16_MAX);

    /* Read gains */
    int16_t kp_x10  = (int16_t)REG(MREG_EEV_KP_X10);    /* (PWM%/°C)×10 */
    int16_t ki_x100 = (int16_t)REG(MREG_EEV_KI_X100);   /* (PWM%/(°C·s))×100 */
    

    

    /* Velocity PI:
     *  delta_pwm = Kp*(e - e_prev) + Ki*e
     *
     * Scaling:
     *  e is °C×10
     *  Kp term (PWM%): (derr_x10 * kp_x10) / 100
     *  Ki term (PWM%): (err_x10  * ki_x100) / 1000
     */
    int32_t derr_x10 = (int32_t)err_x10 - (int32_t)err_prev_x10;
    err_prev_x10 = err_x10;

    int32_t delta = 0;
    delta += ((int32_t)derr_x10 * (int32_t)kp_x10) / 100;
    delta += ((int32_t)err_x10  * (int32_t)ki_x100) / 1000;

    /* Rate limit (PWM% per second) */
    int16_t max_step = (int16_t)REG(MREG_EEV_MAX_STEP);
    if (max_step < 1)   max_step = 1;
    if (max_step > 100)  max_step = 100;   /* prevent insane steps */

    delta = clamp_i32(delta, -(int32_t)max_step, (int32_t)max_step);

    /* Apply delta and clamp to [min..max] */
    int32_t pwm = (int32_t)REG(MREG_EEV_PWM) + delta;
    
    int32_t min_pwm = (int32_t)REG(MREG_EEV_MIN_PWM);
    int32_t max_pwm = (int32_t)REG(MREG_EEV_MAX_PWM);

    
////   
    if(sh_x10 <= 400) {
        min_pwm = 35; 
        max_pwm = 60;
    }
    if(sh_x10 <= 280) {
        min_pwm = 35; 
        max_pwm = 50;
    }
    if(sh_x10 <= 250) {
        min_pwm = 10; 
        max_pwm = 30;
    }
    if(sh_x10 <= 170) {
        min_pwm = 10; 
        max_pwm = 25;
    }
    if(sh_x10 <= 100) {
        min_pwm = 10; 
        max_pwm = 17;
    }
    if(sh_x10 <= 50)  {
        min_pwm = 10; 
        max_pwm = 15;
    }
//    
//    if (REG(MREG_EEV_MIN_PWM) != 0){
//        min_pwm = REG(MREG_EEV_MIN_PWM);
//    }
//    if (REG(MREG_EEV_MAX_PWM) != 0){
//        max_pwm = REG(MREG_EEV_MAX_PWM);
//    }


    /* sanitize limits */
    if (min_pwm > max_pwm) { int32_t t=min_pwm; min_pwm=max_pwm; max_pwm=t; }

    /* keep within valid 0..100 always */
    min_pwm = clamp_i32(min_pwm, 0, 100);
    max_pwm = clamp_i32(max_pwm, 0, 100);

    

    pwm = clamp_i32(pwm, min_pwm, max_pwm);
    


    REG(MREG_EEV_PWM) = (uint16_t)pwm;
    EEV_SetState(EEV_STATE_CONTROL);

    EEV_UpdateActuatorBit();
    EEV_ApplyPwmToHardware();
}

/* Backwards-compatible wrapper.
 * If your scheduler still calls this every 6s, control will be sluggish.
 * Prefer calling EEV_Control_Tick_1s() every 1s.
 */
void EEV_Control_Tick_6s(void)
{
    EEV_Control_Tick_1s();
}






//#include "eev_control.h"
//#include "../system_registers.h"
//#include "../mcc_generated_files/timer/tca1.h"
//#include <stdint.h>
//#include <stdbool.h>
//
///*
// * eev_control.c
// *
// * Electronic Expansion Valve control logic.
// *
// * Modes:
// *   - MANUAL:  uses MREG_MANUAL_EEV_PWM directly.
// *   - AUTO:
// *       * On cooling start: valve closed, startup delay (EEV_STARTUP_DELAY_S).
// *       * After startup: open to MREG_EEV_MIN_PWM.
// *       * Every 6s: adjust PWM to track superheat target.
// *
// * Safety:
// *   - If MREG_SAFETY_ACTIVE != 0, valve closed.
// *   - Requires CO0LING_ACTIVE = 1 and COMPRESSOR_STATE = 1 to operate.
// *
// * Uses:
// *   MREG_SYSTEM_MODE       (0 = Auto, 1 = Manual)
// *   MREG_SAFETY_ACTIVE     (0/1)
// *   MREG_COOLING_ACTIVE    (0/1)
// *   MREG_COMPRESSOR_STATE  (0/1)
// *   MREG_SUPERHEAT         (0.1 K)
// *   MREG_SH_TARGET         (0.1 K)
// *   MREG_EEV_MIN_PWM       (0..1000)
// *   MREG_EEV_MAX_PWM       (0..1000)
// *   MREG_MANUAL_EEV_PWM    (0..1000)
// *
// * Writes:
// *   MREG_EEV_PWM           (0..1000)
// *   MREG_EEV_STATE         (enum: OFF/STARTUP/CONTROL/MANUAL)
// *   MREG_EEV_STARTUP_ACTIVE(0/1)
// *   MREG_EEV_FAULT         (local EEV faults)
// *   MREG_ACTUATOR_STATES   (ACT_PWM_COOL_EEV bit)
// */
//
//#include <avr/io.h>
//#include "../mcc_generated_files/uart/usart1.h"
///* --------------------------------------------------------------------------
// * Local types / constants
// * -------------------------------------------------------------------------- */
//
//typedef enum {
//    EEV_STATE_OFF     = 0,
//    EEV_STATE_STARTUP = 1,
//    EEV_STATE_CONTROL = 2,
//    EEV_STATE_MANUAL  = 3
//} eev_state_t;
//
///* Startup delay to let evaporator pull down (seconds) */
//#define EEV_STARTUP_DELAY_S      5U
//
///* Superheat controller parameters (0.1 K units for SH) */
//#define EEV_KP                   10   /* PWM change per 1.0K error ~ (2/10)*err_x10 */
//#define EEV_ERR_DEADBAND_X10     5   /* 0.5K deadband */
//
///* Superheat sanity range (0.1 K) */
//#define EEV_SH_MIN_X10           0
//#define EEV_SH_MAX_X10           600 /* 60K */
//
///* EEV_FAULT bit definitions (inside MREG_EEV_FAULT) */
//#define EEV_FAULT_SH_INVALID     (1U << 0)
//
///* --------------------------------------------------------------------------
// * Local state
// * -------------------------------------------------------------------------- */
//
//static bool        eev_startup_done    = false;
//static uint16_t    eev_startup_timer_s = 0;
//static bool        prev_cooling_active = false;
//
///* --- tuning registers (recommended) ---
// * kp_x10  = (PWM per K) * 10
// * ki_x100 = (PWM per K per tick) * 100     (tick = 6s here)
// */
//static int16_t  err_prev_x10 = 0;
//static int16_t  sh_filt_x10  = 0;
//static bool     sh_filt_init = false;
//
//#define SH_FILT_DIV      4     // 1st order LPF: new += (meas-new)/4  (alpha=0.25)
//#define EEV_MAX_STEP     40    // max PWM change per 6s tick (tune this)
//#define I_FREEZE_DB_X10  5     // don't integrate in deadband (0.5K)
//
//
///* --------------------------------------------------------------------------
// * Helpers
// * -------------------------------------------------------------------------- */
//
//static eev_state_t EEV_GetState(void)
//{
//    uint16_t s = REG(MREG_EEV_STATE);
//    if (s > (uint16_t)EEV_STATE_MANUAL) {
//        s = (uint16_t)EEV_STATE_OFF;
//    }
//    return (eev_state_t)s;
//}
//
//static void EEV_SetState(eev_state_t st)
//{
//    REG(MREG_EEV_STATE) = (uint16_t)st;
//}
//
//static uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi)
//{
//    if (x < lo) return lo;
//    if (x > hi) return hi;
//    return x;
//}
//
///* Apply REG(MREG_EEV_PWM) (0..100) to TCA1 CMP2BUF */
//static void EEV_ApplyPwmToHardware(void)
//{
//    uint16_t pwm = REG(MREG_EEV_PWM);
//    if (pwm > 1000) pwm = 100;
//
//    uint16_t per = TCA1.SINGLE.PER;
//    uint32_t ticks = ((uint32_t)pwm * (uint32_t)per) / 100U;
//
//    TCA1_Compare2BufferSet((uint16_t)ticks);
//}
//
///* Update ACT_PWM_COOL_EEV bit */
//static void EEV_UpdateActuatorBit(void)
//{
//    uint16_t bits = REG(MREG_ACTUATOR_STATES);
//
//    if (REG(MREG_EEV_PWM) > 0) {
//        bits |= ACT_PWM_COOL_EEV;
//    } else {
//        bits &= (uint16_t)~ACT_PWM_COOL_EEV;
//    }
//
//    REG(MREG_ACTUATOR_STATES) = bits;
//}
//
///* Fully close valve and mark OFF */
//static void EEV_CloseValve(void)
//{
//    REG(MREG_EEV_PWM)            = 0;
//    REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//    EEV_SetState(EEV_STATE_OFF);
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//}
//
///* --------------------------------------------------------------------------
// * Public API
// * -------------------------------------------------------------------------- */
//
//void EEV_Control_Init(void)
//{
//    eev_startup_done    = false;
//    eev_startup_timer_s = 0;
//    prev_cooling_active = false;
//
//    REG(MREG_EEV_PWM)            = 0;
//    REG(MREG_EEV_STATE)          = (uint16_t)EEV_STATE_OFF;
//    REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//    REG(MREG_EEV_FAULT)          = 0;
//
//    EEV_CloseValve();
//}
//
///*
// * Called every 1 s from main loop.
// * - Handles manual vs auto
// * - Handles startup delay when cooling starts
// * - Enforces safety + compressor / cooling conditions
// */
//void EEV_Control_Step_1s(void)
//{
//    uint16_t system_mode       = REG(MREG_SYSTEM_MODE);       /* 0 = Auto, 1 = Manual */
//    uint16_t safety_active     = REG(MREG_SAFETY_ACTIVE);
//    uint16_t cooling_active    = REG(MREG_COOLING_ACTIVE);
//    uint16_t compressor_state  = REG(MREG_COMPRESSOR_STATE);  /* 0/1 */
//
//    /* ---------------- Safety override ---------------- */
//    if (safety_active) {
//        eev_startup_done    = false;
//        eev_startup_timer_s = 0;
//        EEV_CloseValve();
//        prev_cooling_active = (cooling_active != 0);
//        return;
//    }
//
//    /* ---------------- MANUAL MODE ---------------- */
//    if (system_mode != 0) {
//        uint16_t man_pwm = REG(MREG_MANUAL_EEV_PWM);
//        uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//        uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//
//        man_pwm = clamp_u16(man_pwm, min_pwm, max_pwm);
//
//        REG(MREG_EEV_PWM)            = man_pwm;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//        REG(MREG_EEV_FAULT)          = 0;
//        EEV_SetState(EEV_STATE_MANUAL);
//
//        EEV_UpdateActuatorBit();
//        EEV_ApplyPwmToHardware();
//
//        prev_cooling_active = (cooling_active != 0);
//        return;
//    }
//
//    /* ---------------- AUTO MODE: basic checks ---------------- */
//
//    if ((cooling_active == 0) || (compressor_state == 0)) {
//        /* No cooling request or compressor off -> keep valve closed */
//        eev_startup_done    = false;
//        eev_startup_timer_s = 0;
//        EEV_CloseValve();
//        prev_cooling_active = (cooling_active != 0);
//        return;
//    }
//
//    /* At this point: AUTO mode, safety ok, cooling_active = 1, compressor = 1 */
//
//    /* Detect rising edge of cooling_active */
//    if ((cooling_active != 0) && !prev_cooling_active) {
//        eev_startup_done    = false;
//        eev_startup_timer_s = EEV_STARTUP_DELAY_S;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 1;
//        REG(MREG_EEV_PWM)            = 0;
//        EEV_SetState(EEV_STATE_STARTUP);
//    }
//
//    /* Handle startup countdown */
//    if (!eev_startup_done && (eev_startup_timer_s > 0)) {
//        eev_startup_timer_s--;
//        REG(MREG_EEV_PWM) = 0;
//        EEV_SetState(EEV_STATE_STARTUP);
//    }
//
//    /* When startup timer expires, open to MIN PWM */
//    if (!eev_startup_done && (eev_startup_timer_s == 0)) {
//        uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//        uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//        min_pwm = clamp_u16(min_pwm, 0, max_pwm);
//
//        REG(MREG_EEV_PWM)            = min_pwm;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//        EEV_SetState(EEV_STATE_CONTROL);
//        eev_startup_done             = true;
//    }
//
//    /* Apply current PWM to hardware once per second */
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//
//    prev_cooling_active = (cooling_active != 0);
//}
//
//void EEV_Control_Tick_6s(void)
//{
//    uint16_t system_mode       = REG(MREG_SYSTEM_MODE);
//    uint16_t safety_active     = REG(MREG_SAFETY_ACTIVE);
//    uint16_t cooling_active    = REG(MREG_COOLING_ACTIVE);
//    uint16_t compressor_state  = REG(MREG_COMPRESSOR_STATE);
//
//    if (system_mode != 0)  return;
//    if (safety_active)     return;
//    if ((cooling_active == 0) || (compressor_state == 0)) return;
//    if (!eev_startup_done) return;
//
//    int16_t sh_x10     = (int16_t)REG(MREG_SUPERHEAT);
//    int16_t sh_tgt_x10 = (int16_t)REG(MREG_SH_TARGET);
//
//    /* filter SH */
//    if (!sh_filt_init) {
//        sh_filt_x10 = sh_x10;
//        sh_filt_init = true;
//    } else {
//        sh_filt_x10 += (int16_t)((sh_x10 - sh_filt_x10) / REG(MREG_SH_FILT_DIV));
//    }
//
//    /* sanity check */
//    if ((sh_filt_x10 < (int16_t)EEV_SH_MIN_X10) || (sh_filt_x10 > (int16_t)EEV_SH_MAX_X10)) {
//        REG(MREG_EEV_FAULT) |= EEV_FAULT_SH_INVALID;
//        return;
//    } else {
//        REG(MREG_EEV_FAULT) &= (uint16_t)~EEV_FAULT_SH_INVALID;
//    }
//
//    int16_t err_x10 = (int16_t)(sh_filt_x10 - sh_tgt_x10);
//
//    /* deadband */
//    if (err_x10 > -EEV_ERR_DEADBAND_X10 && err_x10 < EEV_ERR_DEADBAND_X10) {
//        err_x10 = 0;
//    }
//
//    /* READ THE GAINS */
//    int16_t kp_x10  = (int16_t)REG(MREG_EEV_KP_X10);     
//    int16_t ki_x100 = (int16_t)REG(MREG_EEV_KI_X100);    
//
//    int16_t derr_x10 = (int16_t)(err_x10 - err_prev_x10);
//    err_prev_x10 = err_x10;
//
//    /* velocity PI:
//       delta_pwm = Kp*(e - e_prev) + Ki*e
//       scaling:
//         Kp term: (derr_x10 * kp_x10)/100  -> PWM
//         Ki term: (err_x10  * ki_x100)/1000 -> PWM (because x10 * x100 / (10*100) )
//    */
//    int32_t delta = 0;
//
//    delta += ((int32_t)derr_x10 * (int32_t)kp_x10) / 100;
//
//    if (err_x10 != 0) {
//        delta += ((int32_t)err_x10 * (int32_t)ki_x100) / 1000;
//    }
//
//    /* rate limit */
//    if (delta >  REG(MREG_EEV_MAX_STEP)) delta =  EEV_MAX_STEP;
//    if (delta < -EEV_MAX_STEP) delta = -EEV_MAX_STEP;
//
//    int32_t pwm = (int32_t)REG(MREG_EEV_PWM) + delta;
//
//    uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//    uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//    if (pwm < (int32_t)min_pwm) pwm = (int32_t)min_pwm;
//    if (pwm > (int32_t)max_pwm) pwm = (int32_t)max_pwm;
//
//    REG(MREG_EEV_PWM) = (uint16_t)pwm;
//    EEV_SetState(EEV_STATE_CONTROL);
//
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//}


/*
 * Called every 6 s ? slow superheat loop.
 *
 * Only runs in AUTO mode, safety OK, cooling_active = 1, compressor = 1,
 * and after startup is done.
 */
//void EEV_Control_Tick_6s(void)
//{
//    uint16_t system_mode       = REG(MREG_SYSTEM_MODE);
//    uint16_t safety_active     = REG(MREG_SAFETY_ACTIVE);
//    uint16_t cooling_active    = REG(MREG_COOLING_ACTIVE);
//    uint16_t compressor_state  = REG(MREG_COMPRESSOR_STATE);
//
//    if (system_mode != 0)  return;  /* manual mode */
//    if (safety_active)     return;
//    if ((cooling_active == 0) || (compressor_state == 0)) return;
//    if (!eev_startup_done) return;
//
//    /* Read superheat and target (0.1K) */
//    int16_t sh     = (int16_t)REG(MREG_SUPERHEAT);
//    int16_t sh_tgt = (int16_t)REG(MREG_SH_TARGET);
//
//    uint16_t eev_faults = REG(MREG_EEV_FAULT);
//
//    /* Basic sanity check */
//    if ((sh < (int16_t)EEV_SH_MIN_X10) || (sh > (int16_t)EEV_SH_MAX_X10)) {
//        eev_faults |= EEV_FAULT_SH_INVALID;
//        REG(MREG_EEV_FAULT) = eev_faults;
//        return;
//    } else {
//        eev_faults &= (uint16_t)~EEV_FAULT_SH_INVALID;
//        REG(MREG_EEV_FAULT) = eev_faults;
//    }
//
//    /* error_x10 > 0 -> SH too high -> valve too closed -> open more */
//    int16_t err_x10 = sh - sh_tgt;
//
//    /* Deadband */
//    if (err_x10 > -EEV_ERR_DEADBAND_X10 && err_x10 < EEV_ERR_DEADBAND_X10) {
//        err_x10 = 0;
//    }
//
//    int16_t pwm     = (int16_t)REG(MREG_EEV_PWM);
//    int16_t delta   = (int16_t)((err_x10 * REG(MREG_EEV_KP_X10)) / 100);  /* err_x10 is ×10 */
//
//    pwm += delta;
//    
//    printf("pwm delta: %d , ", delta);   
//    printf("pwm error: %d\n", err_x10);
//
//    uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//    uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//    pwm = (uint16_t)clamp_u16((uint16_t)pwm, min_pwm, max_pwm);
//
//    REG(MREG_EEV_PWM) = (uint16_t)pwm;
//    EEV_SetState(EEV_STATE_CONTROL);
//
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//}

/////////////////////////////////////////////////////////////

///*
// * eev_control.c
// *
// * Electronic Expansion Valve control logic.
// * - Startup sequence to let evaporator pressure pull down
// * - Superheat-based control in AUTO mode
// * - Manual override via MREG_MANUAL_EEV_PWM
// * - 6-second timing base for SH adjustments
// */
//
//#include <avr/io.h>
//
///* --------------------------------------------------------------------------
// * Local types / constants
// * -------------------------------------------------------------------------- */
//
//typedef enum {
//    EEV_STATE_OFF = 0,
//    EEV_STATE_STARTUP = 1,
//    EEV_STATE_CONTROL = 2,
//    EEV_STATE_MANUAL = 3
//} eev_state_t;
//
///* How long to keep EEV fully closed after cooling starts (s) */
//#define EEV_STARTUP_DELAY_S      30U
//
///* Superheat controller gain and deadband
// * - SH, SH_TARGET in 0.1 K units
// * - EEV_KP is dimensionless, integer
// */
//#define EEV_KP                   2      // PWM change per 1.0 K error ~ (2/10)*err_x10
//#define EEV_ERR_DEADBAND_X10     5      // 0.5 K deadband
//
///* Simple sanity limits for superheat, in 0.1 K */
//#define EEV_SH_MIN_X10           0      // don't trust negative SH
//#define EEV_SH_MAX_X10           600    // 60 K upper sanity bound
//
///* --------------------------------------------------------------------------
// * Local state
// * -------------------------------------------------------------------------- */
//
//static bool        eev_startup_done = false;
//static uint16_t    eev_startup_timer_s = 0;
//static bool        prev_cooling_active = false;
//
///* --------------------------------------------------------------------------
// * Helpers
// * -------------------------------------------------------------------------- */
//
///* Get current EEV state from register as enum */
//static eev_state_t EEV_GetState(void)
//{
//    uint16_t s = REG(MREG_EEV_STATE);
//    if (s > (uint16_t)EEV_STATE_MANUAL) {
//        s = (uint16_t)EEV_STATE_OFF;
//    }
//    return (eev_state_t)s;
//}
//
///* Set EEV state into register */
//static void EEV_SetState(eev_state_t st)
//{
//    REG(MREG_EEV_STATE) = (uint16_t)st;
//}
//
///* Clamp helper */
//static uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi)
//{
//    if (x < lo) return lo;
//    if (x > hi) return hi;
//    return x;
//}
//
///* Apply REG(MREG_EEV_PWM) (0..1000) to TCA1 CMP2BUF.
// * Assumes TCA1.SINGLE.PER already set by MCC for a 6 s period.
// */
//static void EEV_ApplyPwmToHardware(void)
//{
//    uint16_t pwm = REG(MREG_EEV_PWM);  // 0..1000 => 0..100.0%
//    if (pwm > 1000) pwm = 1000;
//
//    uint16_t per = TCA1.SINGLE.PER;
//    uint32_t ticks = ((uint32_t)pwm * (uint32_t)per) / 1000U;
//
//    TCA1_Compare2BufferSet((uint16_t)ticks);
//}
//
///* Set/clear the EEV actuator bit based on PWM > 0 */
//static void EEV_UpdateActuatorBit(void)
//{
//    uint16_t bits = REG(MREG_ACTUATOR_STATES);
//
//    if (REG(MREG_EEV_PWM) > 0) {
//        bits |= ACT_PWM_COOL_EEV;
//    } else {
//        bits &= (uint16_t)~ACT_PWM_COOL_EEV;
//    }
//
//    REG(MREG_ACTUATOR_STATES) = bits;
//}
//
///* Convenience to fully close EEV and mark OFF state */
//static void EEV_CloseValve(void)
//{
//    REG(MREG_EEV_PWM) = 0;
//    EEV_SetState(EEV_STATE_OFF);
//    REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//}
//
///* --------------------------------------------------------------------------
// * Public API
// * -------------------------------------------------------------------------- */
//
//void EEV_Control_Init(void)
//{
//    eev_startup_done   = false;
//    eev_startup_timer_s = 0;
//    prev_cooling_active = false;
//
//    REG(MREG_EEV_PWM)             = 0;
//    REG(MREG_EEV_STATE)           = (uint16_t)EEV_STATE_OFF;
//    REG(MREG_EEV_STARTUP_ACTIVE)  = 0;
//    REG(MREG_EEV_FAULT)           = 0;
//
//    EEV_CloseValve();
//}
//
///*
// * Called every 1 s from main loop.
// * - Handles AUTO vs MANUAL mode
// * - Handles cooling start detection and startup timing
// * - Enforces safety/compressor constraints
// */
//void EEV_Control_Step_1s(void)
//{
//    uint16_t system_mode       = REG(MREG_SYSTEM_MODE);       // 0 = Auto, 1 = Manual
//    uint16_t safety_active     = REG(MREG_SAFETY_ACTIVE);
//    uint16_t cooling_active    = REG(MREG_COOLING_ACTIVE);
//    uint16_t compressor_state  = REG(MREG_COMPRESSOR_STATE);  // 0/1 or enum
//
//    /* ---------------- MANUAL MODE ----------------- */
//    if (system_mode != 0) {
//        uint16_t man_pwm = REG(MREG_MANUAL_EEV_PWM);
//        uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//        uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//
//        man_pwm = clamp_u16(man_pwm, min_pwm, max_pwm);
//
//        REG(MREG_EEV_PWM)            = man_pwm;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//        REG(MREG_EEV_FAULT)          = 0;
//        EEV_SetState(EEV_STATE_MANUAL);
//
//        EEV_UpdateActuatorBit();
//        EEV_ApplyPwmToHardware();
//
//        prev_cooling_active = (cooling_active != 0);
//        return;
//    }
//
//    /* ---------------- AUTO MODE: safety & compressor checks ----------------- */
//
//    if ((safety_active != 0) || (cooling_active == 0) || (compressor_state == 0)) {
//        // Any unsafe condition: close valve and reset startup logic
//        eev_startup_done    = false;
//        eev_startup_timer_s = 0;
//        EEV_CloseValve();
//        prev_cooling_active = (cooling_active != 0);
//        return;
//    }
//
//    /* At this point: AUTO mode, safety OK, cooling active, compressor running */
//
//    // Detect cooling start edge
//    if ((cooling_active != 0) && !prev_cooling_active) {
//        // Cooling just started: begin startup delay with valve fully closed
//        eev_startup_done    = false;
//        eev_startup_timer_s = EEV_STARTUP_DELAY_S;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 1;
//        REG(MREG_EEV_PWM)            = 0;
//        EEV_SetState(EEV_STATE_STARTUP);
//    }
//
//    // Handle startup timing: keep valve closed while timer > 0
//    if (!eev_startup_done && (eev_startup_timer_s > 0)) {
//        eev_startup_timer_s--;
//        REG(MREG_EEV_PWM) = 0;
//        EEV_SetState(EEV_STATE_STARTUP);
//    }
//
//    // When startup timer expires, open to minimum PWM and hand over to control
//    if (!eev_startup_done && (eev_startup_timer_s == 0)) {
//        uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//        uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//
//        min_pwm = clamp_u16(min_pwm, 0, max_pwm);
//
//        REG(MREG_EEV_PWM)            = min_pwm;
//        EEV_SetState(EEV_STATE_CONTROL);
//        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//        eev_startup_done             = true;
//    }
//
//    // Always update actuator bit + hardware once per second
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//
//    prev_cooling_active = (cooling_active != 0);
//}
//
///*
// * Called every 6 s from main loop.
// * - Only active in AUTO mode, safety OK, cooling + compressor ON
// * - Only runs after startup_done = true
// * - Adjusts MREG_EEV_PWM based on superheat error
// */
//void EEV_Control_Tick_6s(void)
//{
//    uint16_t system_mode       = REG(MREG_SYSTEM_MODE);
//    uint16_t safety_active     = REG(MREG_SAFETY_ACTIVE);
//    uint16_t cooling_active    = REG(MREG_COOLING_ACTIVE);
//    uint16_t compressor_state  = REG(MREG_COMPRESSOR_STATE);
//
//    if (system_mode != 0) {
//        // Manual mode: SH loop disabled
//        return;
//    }
//
//    if ((safety_active != 0) || (cooling_active == 0) || (compressor_state == 0)) {
//        // Any unsafe condition: let 1s step close the valve
//        return;
//    }
//
//    if (!eev_startup_done) {
//        // Still in startup phase: don't run SH control yet
//        return;
//    }
//
//    // Read superheat and target (0.1 K units)
//    int16_t sh      = (int16_t)REG(MREG_SUPERHEAT);
//    int16_t sh_tgt  = (int16_t)REG(MREG_SH_TARGET);
//
//    // Basic sanity check on SH
//    if ((sh < (int16_t)EEV_SH_MIN_X10) || (sh > (int16_t)EEV_SH_MAX_X10)) {
//        // Mark an EEV fault bit if you want
//        REG(MREG_EEV_FAULT) |= (1U << 0);  // example: bit0 = SH invalid
//        return;
//    } else {
//        // Clear that bit if SH is sane again
//        REG(MREG_EEV_FAULT) &= (uint16_t)~(1U << 0);
//    }
//
//    // Error = measured - target (if SH too high, we want to open more)
//    int16_t err_x10 = sh - sh_tgt;
//
//    // Deadband: ignore small errors
//    if (err_x10 > -EEV_ERR_DEADBAND_X10 && err_x10 < EEV_ERR_DEADBAND_X10) {
//        err_x10 = 0;
//    }
//
//    // Proportional-only tweak on PWM
//    int16_t pwm     = (int16_t)REG(MREG_EEV_PWM);
//    int16_t delta   = (int16_t)((err_x10 * EEV_KP) / 10);  // err_x10 is ×10
//
//    pwm += delta;
//
//    // Clamp to configured min/max
//    uint16_t min_pwm = REG(MREG_EEV_MIN_PWM);
//    uint16_t max_pwm = REG(MREG_EEV_MAX_PWM);
//
//    pwm = (int16_t)clamp_u16((uint16_t)pwm, min_pwm, max_pwm);
//    REG(MREG_EEV_PWM) = (uint16_t)pwm;
//
//    // State is definitely CONTROL here
//    EEV_SetState(EEV_STATE_CONTROL);
//
//    // Apply to actuator bit + hardware
//    EEV_UpdateActuatorBit();
//    EEV_ApplyPwmToHardware();
//}
//
//


//// Internal EEV state
//static uint8_t eev_pwm = 0;
//static uint8_t startup_counter = 0;
//
//// ---------------------------------------------------------------------------
//// Initialize EEV module
//// ---------------------------------------------------------------------------
//void eev_control_init(void)
//{
//    eev_pwm = 0;
//    startup_counter = 0;
//
//    REG(MREG_EEV_PWM)            = 0;
//    REG(MREG_EEV_STATE)          = 0;
//    REG(MREG_EEV_STARTUP_ACTIVE) = 1;
//    REG(MREG_EEV_FAULT)          = 0;
//
//    TCA1_Compare2BufferSet(0);   // fully closed
//}
//
//// ---------------------------------------------------------------------------
//// Helper: Apply PWM (0?100%) to TCA1 WO2
//// ---------------------------------------------------------------------------
//static void apply_pwm(uint8_t duty)
//{
//    uint16_t per = TCA1.SINGLE.PER;
//    uint16_t cmp = (per * duty) / 100;
//    TCA1_Compare2BufferSet(cmp);
//}
//
//// ---------------------------------------------------------------------------
//// EEV Control (called once per second)
//// ---------------------------------------------------------------------------
//void eev_control_update(void)
//{
//    // Safety forces valve closed
//    if (REG(MREG_SAFETY_ACTIVE))
//    {
//        eev_pwm = 0;
//        apply_pwm(0);
//
//        REG(MREG_EEV_PWM)            = 0;
//        REG(MREG_EEV_STATE)          = 0;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//        REG(MREG_EEV_FAULT)          = 0;
//
//        startup_counter = 0;
//        return;
//    }
//
//    // Manual override
//    if (REG(MREG_SYSTEM_MODE) == 1)
//    {
//        eev_pwm = REG(MREG_MANUAL_EEV_PWM);
//        if (eev_pwm > 100)
//            eev_pwm = 100;
//
//        apply_pwm(eev_pwm);
//
//        REG(MREG_EEV_PWM)            = eev_pwm;
//        REG(MREG_EEV_STATE)          = (eev_pwm > 0);
//        REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//        return;
//    }
//
//    // Compressor off ? valve closed
//    if (!REG(MREG_COMPRESSOR_STATE))
//    {
//        eev_pwm = 0;
//        apply_pwm(0);
//
//        REG(MREG_EEV_PWM)            = 0;
//        REG(MREG_EEV_STATE)          = 0;
//        REG(MREG_EEV_STARTUP_ACTIVE) = 1;
//        startup_counter = 0;
//        return;
//    }
//
//    // -------------------------------------------------------------------
//    // STARTUP: 10 seconds of low PWM to stabilize suction pressure
//    // -------------------------------------------------------------------
//    if (startup_counter < 10)
//    {
//        eev_pwm = REG(MREG_EEV_MIN_PWM);
//
//        apply_pwm(eev_pwm);
//
//        REG(MREG_EEV_PWM)            = eev_pwm;
//        REG(MREG_EEV_STATE)          = (eev_pwm > 0);
//        REG(MREG_EEV_STARTUP_ACTIVE) = 1;
//
//        startup_counter++;
//        return;
//    }
//
//    REG(MREG_EEV_STARTUP_ACTIVE) = 0;
//
//    // -------------------------------------------------------------------
//    // SUPERHEAT CONTROL
//    // -------------------------------------------------------------------
//    int16_t T_in  = REG(MREG_T_EVAP_IN);
//    int16_t T_out = REG(MREG_T_EVAP_OUT);
//    int16_t sh        = T_out - T_in;             // actual SH
//    int16_t sh_target = REG(MREG_SH_TARGET);      // desired SH
//
//    int16_t error = sh_target - sh;               // positive ? close valve
//
//    int16_t step = 1; // adjust 1% per second
//
//    if (error > 5)  // SH too low ? close a bit
//    {
//        if (eev_pwm > step)
//            eev_pwm -= step;
//        else
//            eev_pwm = 0;
//    }
//    else if (error < -5)  // SH too high ? open a bit
//    {
//        if (eev_pwm + step <= 100)
//            eev_pwm += step;
//    }
//
//    // Clamp to limits
//    uint8_t minp = REG(MREG_EEV_MIN_PWM);
//    uint8_t maxp = REG(MREG_EEV_MAX_PWM);
//
//    if (eev_pwm < minp)
//        eev_pwm = minp;
//    if (eev_pwm > maxp)
//        eev_pwm = maxp;
//
//    // Apply PWM
//    apply_pwm(eev_pwm);
//
//    REG(MREG_EEV_PWM)   = eev_pwm;
//    REG(MREG_EEV_STATE) = (eev_pwm > 0);
//}
