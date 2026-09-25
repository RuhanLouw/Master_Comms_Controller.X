
#include "safety_manager.h"
#include "../system_registers.h"
#include <stdbool.h>


/*
 * safety_manager.c
 *
 * Evaluates global fault flags and key temperatures to decide
 * whether the system should enter or stay in a "safety stop".
 *
 * Uses:
 *   MREG_FAULT_FLAGS        (bitfield, FAULT_*)
 *   MREG_T_CHAMBER          (°C×10)
 *   MREG_T_MAX_CHAMBER      (°C×10)  - upper bound
 *   MREG_T_MIN_CHAMBER      (°C×10)  - lower bound
 *   MREG_SAFETY_LATCHED     (0/1)    - written by Pi to clear latch
 *
 * Writes:
 *   MREG_SAFETY_ACTIVE      (0/1)
 *   MREG_SAFETY_LATCHED     (0/1)
 *   MREG_SAFETY_CODE        (SAFETY_CODE_*)
 */

#include <stdint.h>

/* Safety codes */
#define SAFETY_CODE_NONE             0
#define SAFETY_CODE_SENSOR_FAULT     1
#define SAFETY_CODE_COMM_SENSOR      2
#define SAFETY_CODE_CHAMBER_OOR      3
#define SAFETY_CODE_COMPRESSOR_FAULT 4
#define SAFETY_CODE_EEV_FAULT        5
#define SAFETY_CODE_HEATER_FAULT     6

/* Fault bit masks (from your map) */
#define F_NTC_ERROR              (1U << 0)
#define F_KTYPE_ERROR            (1U << 1)
#define F_DHT22_ERROR            (1U << 2)
#define F_EEPROM_ERROR           (1U << 3)
#define F_COMM_SENSOR_CARD       (1U << 4)
#define F_OVER_UNDER_TEMP        (1U << 5)
#define F_COIL                   (1U << 6)
#define F_EEV_ERROR              (1U << 7)
#define F_AC_HEATER_ERROR        (1U << 8)

/* Compressor-related faults */
#define F_COMP_BATTERY_CUTOUT        (1U << 9)
#define F_COMP_FAN_OVER_CURRENT      (1U << 10)
#define F_COMP_START_ERROR           (1U << 11)
#define F_COMP_SPEED_ERROR           (1U << 12)
#define F_COMP_THERM_CUTOUT          (1U << 13)
#define F_COMP_CONTROL_FAILURE       (1U << 14)

#define COMP_FAULT_MASK  ( \
    F_COMP_BATTERY_CUTOUT       | \
    F_COMP_FAN_OVER_CURRENT     | \
    F_COMP_START_ERROR          | \
    F_COMP_SPEED_ERROR          | \
    F_COMP_THERM_CUTOUT         | \
    F_COMP_CONTROL_FAILURE )

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void SafetyManager_Init(void)
{
    REG(MREG_SAFETY_ACTIVE)  = 0;
    REG(MREG_SAFETY_LATCHED) = 0;
    REG(MREG_SAFETY_CODE)    = SAFETY_CODE_NONE;
}

/*
 * Called every 1 s from main loop.
 *
 * Note:
 *  - If any critical condition is detected, we set SAFETY_ACTIVE=1,
 *    SAFETY_LATCHED=1 and store a SAFETY_CODE.
 *  - To clear safety, Pi must:
 *      1) clear the underlying faults (so they no longer appear in FAULT_FLAGS),
 *      2) write MREG_SAFETY_LATCHED = 0.
 */
void SafetyManager_Step_1s(void)
{
    uint16_t faults          = REG(MREG_FAULT_FLAGS);
    uint16_t safety_latched  = REG(MREG_SAFETY_LATCHED);
    uint16_t safety_active   = REG(MREG_SAFETY_ACTIVE);
    uint16_t safety_code     = REG(MREG_SAFETY_CODE);

    /* ---------------- Check chamber temperature ---------------- */

    int16_t t_ch_x10   = (int16_t)REG(MREG_T_CHAMBER);
    int16_t t_max_x10  = (int16_t)REG(MREG_T_MAX_CHAMBER);
    int16_t t_min_x10  = (int16_t)REG(MREG_T_MIN_CHAMBER);

    bool chamber_oor = false;

    if (t_max_x10 > t_min_x10) {
        if ((t_ch_x10 > t_max_x10) || (t_ch_x10 < t_min_x10)) {
            chamber_oor = true;
        }
    }

    if (chamber_oor) {
        /* Set the generic over/under temp fault bit */
        faults |= F_OVER_UNDER_TEMP;
    } else {
        /* This module owns the live chamber-range fault bit. */
        faults &= (uint16_t)~F_OVER_UNDER_TEMP;
    }
    REG(MREG_FAULT_FLAGS) = faults;

    /* ---------------- Determine if we should trip safety ---------------- */

    bool want_safety = false;
    uint16_t new_code = SAFETY_CODE_NONE;

    /* 1) Sensor faults */
    if (faults & (F_NTC_ERROR | F_KTYPE_ERROR | F_DHT22_ERROR)) {
        want_safety = true;
        new_code    = SAFETY_CODE_SENSOR_FAULT;
    }

    /* 2) Sensor comm faults */
    if (!want_safety && (faults & F_COMM_SENSOR_CARD)) {
        want_safety = true;
        new_code    = SAFETY_CODE_COMM_SENSOR;
    }

    /* 3) Chamber temperature out of range */
    if (!want_safety && chamber_oor) {
        want_safety = true;
        new_code    = SAFETY_CODE_CHAMBER_OOR;
    }

    /* 4) Compressor-related faults */
    if (!want_safety && (faults & COMP_FAULT_MASK)) {
        want_safety = true;
        new_code    = SAFETY_CODE_COMPRESSOR_FAULT;
    }

    /* 5) EEV faults */
    if (!want_safety && (faults & F_EEV_ERROR)) {
        want_safety = true;
        new_code    = SAFETY_CODE_EEV_FAULT;
    }

    /* 6) Heater fault */
    if (!want_safety && (faults & F_AC_HEATER_ERROR)) {
        want_safety = true;
        new_code    = SAFETY_CODE_HEATER_FAULT;
    }

    /* ---------------- Latch logic ----------------
     *
     * If we want safety now:
     *   - set SAFETY_ACTIVE = 1
     *   - set SAFETY_LATCHED = 1
     *   - update SAFETY_CODE (only if not already latched)
     *
     * Clearing:
     *   - Only if:
     *       * no "want_safety"
     *       * Pi has written SAFETY_LATCHED = 0
     *   - Then we set SAFETY_ACTIVE = 0 and SAFETY_CODE = NONE
     */

    if (want_safety) {
        safety_active  = 1;
        safety_latched = 1;

        /* Only overwrite code if we weren't already in a latched state,
         * so we keep the first cause visible.
         */
        if (safety_code == SAFETY_CODE_NONE) {
            safety_code = new_code;
        }
    } else {
        /* No current reason to be in safety. Allow clear if latch is 0. */
        if (safety_latched == 0) {
            safety_active = 0;
            safety_code   = SAFETY_CODE_NONE;
        } else {
            /* Latch is still set, keep safety_active = 1. */
            safety_active = 1;
        }
    }

    /* Store results */
    REG(MREG_SAFETY_ACTIVE)  = safety_active;
    REG(MREG_SAFETY_LATCHED) = safety_latched;
    REG(MREG_SAFETY_CODE)    = safety_code;

    uint16_t status = (uint16_t)REG(MREG_SYS_STATUS);
    if (safety_active) {
        status |= SYS_STATUS_FAULT_ACTIVE;
    } else {
        status &= (uint16_t)~SYS_STATUS_FAULT_ACTIVE;
    }
    REG(MREG_SYS_STATUS) = status;
    
        /* --------------------------------------------------
     * Pilot LEDs: GREEN / RED
     *   - GREEN: safety_active == 0   ("OK")
     *   - RED  : safety_active != 0   ("NOT OK")
     * -------------------------------------------------- */
    uint16_t bits = REG(MREG_ACTUATOR_STATES);

    if (safety_active) {
        /* De-energize process outputs and leave only the fault pilot active. */
        bits &= (uint16_t)~(ACT_AC_COMPRESSOR | ACT_AC_CONDENSER_FAN |
                            ACT_AC_DEFROST | ACT_RESERVED_3 |
                            ACT_DC_COMPRESSOR | ACT_DC_ORANGE_PILOT |
                            ACT_HEAT_FANS | ACT_COOL_FANS |
                            ACT_PWM_HEAT_ELEMENT | ACT_PWM_COOL_EEV);
        bits |=  ACT_DC_RED_PILOT;
        bits &= (uint16_t)~ACT_DC_GREEN_PILOT;
    } else {
        /* OK: green on, red off */
        bits |=  ACT_DC_GREEN_PILOT;
        bits &= (uint16_t)~ACT_DC_RED_PILOT;
    }

    REG(MREG_ACTUATOR_STATES) = bits;
    
    
}


//// Safety codes
//#define SAFE_NONE           0
//#define SAFE_HEATER_OVERT   1
//#define SAFE_CHAMBER_HI     2
//#define SAFE_CHAMBER_LO     3
//#define SAFE_EVAP_ICING     4
//#define SAFE_SENSOR_FAIL    5
//
//// Internal latch
//static bool safety_latched = false;
//
//void safety_manager_init(void)
//{
//    REG(MREG_SAFETY_ACTIVE)  = 0;
//    REG(MREG_SAFETY_CODE)    = SAFE_NONE;
//    REG(MREG_SAFETY_LATCHED) = 0;
//
//    safety_latched = false;
//}
//
//// ---------------------------------------------------------------------------
//// Safety Manager
//// Called once per second.
//// If any fault occurs, safety_latched = true.
//// Pi can clear by writing SAFETY_LATCHED = 0.
//// ---------------------------------------------------------------------------
//void safety_manager_update(void)
//{
//    // If latched ? remain in safety mode until Pi clears it
//    if (safety_latched)
//    {
//        REG(MREG_SAFETY_ACTIVE) = 1;
//        REG(MREG_SAFETY_LATCHED) = 1;
//        return;
//    }
//
//    int16_t T_htr     = REG(MREG_T_HEATER_ELEMENT);
//    int16_t T_htr_max = REG(MREG_HEATER_MAX_TEMP);
//
//    int16_t T_ch      = REG(MREG_T_CHAMBER);
//    int16_t T_ch_max  = REG(MREG_T_MAX_CHAMBER);
//    int16_t T_ch_min  = REG(MREG_T_MIN_CHAMBER);
//
//    int16_t T_evapbox = REG(MREG_T_EVAP_BOX);
//    int16_t T_evap_min = REG(MREG_T_EVAP_MIN_ALLOWED);
//
//    uint16_t sensor_flags = REG(MREG_SREG_SENSOR_ENABLE);
//
//    // -------------------------------------------------------
//    // Fault checks
//    // -------------------------------------------------------
//
//    if (T_htr >= T_htr_max)
//    {
//        safety_latched = true;
//        REG(MREG_SAFETY_CODE) = SAFE_HEATER_OVERT;
//    }
//    else if (T_ch >= T_ch_max)
//    {
//        safety_latched = true;
//        REG(MREG_SAFETY_CODE) = SAFE_CHAMBER_HI;
//    }
//    else if (T_ch <= T_ch_min)
//    {
//        safety_latched = true;
//        REG(MREG_SAFETY_CODE) = SAFE_CHAMBER_LO;
//    }
//    else if (T_evapbox <= T_evap_min)
//    {
//        safety_latched = true;
//        REG(MREG_SAFETY_CODE) = SAFE_EVAP_ICING;
//    }
//    else if (sensor_flags != 0)
//    {
//        safety_latched = true;
//        REG(MREG_SAFETY_CODE) = SAFE_SENSOR_FAIL;
//    }
//
//    // -------------------------------------------------------
//    // Update output registers
//    // -------------------------------------------------------
//    if (safety_latched)
//    {
//        REG(MREG_SAFETY_ACTIVE)  = 1;
//        REG(MREG_SAFETY_LATCHED) = 1;
//    }
//    else
//    {
//        REG(MREG_SAFETY_ACTIVE)  = 0;
//        REG(MREG_SAFETY_CODE)    = SAFE_NONE;
//        REG(MREG_SAFETY_LATCHED) = 0;
//    }
//}
//
