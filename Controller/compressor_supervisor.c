#include "compressor_supervisor.h"
#include "../system_registers.h"

/*
 * compressor_supervisor.c
 *
 * Handles safe on/off control of the compressor:
 *  - Enforces minimum on/off times (AUTO and MANUAL)
 *  - AUTO mode: obeys cooling request from mode_manager
 *  - MANUAL mode: obeys MREG_MANUAL_ACTUATOR_STATES (compressor/fan bits)
 *  - BYPASS bit: in MANUAL mode, can skip min on/off timers
 *  - Checks safety and compressor-related fault bits
 *  - Drives ACT_DC_COMPRESSOR and ACT_AC_CONDENSER_FAN bits
 *
 * Uses:
 *   MREG_SYSTEM_MODE              (0 = Auto, 1 = Manual)
 *   MREG_COOLING_ACTIVE           (0/1)   - cooling requested by mode_manager (AUTO)
 *   MREG_COMPRESSOR_ALLOWED       (0/1)   - basic permission from mode_manager (AUTO)
 *   MREG_SAFETY_ACTIVE            (0/1)   - global safety flag
 *   MREG_FAULT_FLAGS              (bitfield) - compressor-related faults
 *   MREG_COMP_MIN_ON_TIME         (s)     - minimum on-time
 *   MREG_COMP_MIN_OFF_TIME        (s)     - minimum off-time
 *   MREG_MANUAL_ACTUATOR_STATES   (bitfield) - manual actuator requests (MANUAL)
 *   MREG_MANUAL_BYPASS_FLAGS      (bitfield) - engineering bypass flags (MANUAL)
 *
 * Writes:
 *   MREG_COMPRESSOR_STATE         (0/1)   - actual compressor on/off
 *   MREG_COMPRESSOR_ALLOWED       (0/1)   - final allowed flag (safety & faults)
 *   MREG_ACTUATOR_STATES          (bitfield) - ACT_DC_COMPRESSOR, ACT_AC_CONDENSER_FAN
 */

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

typedef enum {
    COMP_STATE_OFF = 0,
    COMP_STATE_ON  = 1
} comp_state_t;

static comp_state_t comp_state         = COMP_STATE_OFF;
static uint16_t     comp_on_timer_s    = 0;
static uint16_t     comp_off_timer_s   = 0;

/* DC Compressor-related fault mask within MREG_FAULT_FLAGS:
 *  - FAULT_COMP_BATTERY_CUTOUT        (1 << 9)
 *  - FAULT_COMP_FAN_OVER_CURRENT      (1 << 10)
 *  - FAULT_COMP_START_ERROR           (1 << 11)
 *  - FAULT_COMP_SPEED_ERROR           (1 << 12)
 *  - FAULT_COMP_THERM_CUTOUT          (1 << 13)
 *  - FAULT_COMP_CONTROL_FAILURE       (1 << 14)
 */
#define COMP_FAULT_MASK  ( \
    (1U << 8)  | \
    (1U << 9)  | \
    (1U << 10) | \
    (1U << 11) | \
    (1U << 12) | \
    (1U << 13) | \
    (1U << 14) )

/* Manual bypass flags (local bit definitions for MREG_MANUAL_BYPASS_FLAGS) */
#define MAN_BYPASS_COMP_TIMERS  (1U << 0)   /* Bypass compressor min on/off timers in MANUAL */

/* Apply comp_state to actuator bitfield (AUTO mode use; MANUAL has its own branch) */
static void CompressorSupervisor_UpdateActuators(void)
{
    uint16_t bits = REG(MREG_ACTUATOR_STATES);

    if (comp_state == COMP_STATE_ON) {
        bits |= ACT_AC_COMPRESSOR;
        bits |= ACT_AC_CONDENSER_FAN;
    } else {
        bits &= (uint16_t)~ACT_AC_COMPRESSOR;
        bits &= (uint16_t)~ACT_AC_CONDENSER_FAN;
    }

    REG(MREG_ACTUATOR_STATES) = bits;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void CompressorSupervisor_Init(void)
{
    comp_state       = COMP_STATE_OFF;
    comp_on_timer_s  = 0;
    comp_off_timer_s = 0;

    REG(MREG_COMPRESSOR_STATE)   = 0;
    /* Start with "not allowed" until mode_manager / safety decide otherwise */
    REG(MREG_COMPRESSOR_ALLOWED) = 0;

    CompressorSupervisor_UpdateActuators();
}

/*
 * Called once per second from main control loop.
 */
void CompressorSupervisor_Step_1s(void)
{
    uint16_t system_mode    = REG(MREG_SYSTEM_MODE);          /* 0 = Auto, 1 = Manual */
    uint16_t cooling_active = REG(MREG_COOLING_ACTIVE);
    uint16_t mode_allowed   = REG(MREG_COMPRESSOR_ALLOWED);   /* from mode_manager in AUTO */
    uint16_t safety_active  = REG(MREG_SAFETY_ACTIVE);
    uint16_t faults         = REG(MREG_FAULT_FLAGS);
    uint16_t manual_bits    = REG(MREG_MANUAL_ACTUATOR_STATES);
    uint16_t bypass_flags   = REG(MREG_MANUAL_BYPASS_FLAGS);
    
    if(REG(MREG_SUBCOOLING) > 550) {
        faults |= FAULT_AC_HEATER_ERROR;
    }
    else {
        faults &= ~FAULT_AC_HEATER_ERROR;
    }

    /* Read minimum on/off times from the configuration registers. */
    uint16_t min_on_s  = REG(MREG_COMP_MIN_ON_TIME);
    uint16_t min_off_s = REG(MREG_COMP_MIN_OFF_TIME);

    /* Any compressor-related fault or safety will block the compressor. */
    
    bool has_comp_fault            = ((faults & COMP_FAULT_MASK) != 0U);
    bool allowed_by_fault_and_safety = (!safety_active && !has_comp_fault);

    bool manual_mode   = (system_mode != 0U);
    bool bypass_timers = ((bypass_flags & MAN_BYPASS_COMP_TIMERS) != 0U);

    /* Update timers based on current state (for AUTO and MANUAL, even if bypass) */
    if (comp_state == COMP_STATE_ON) {
        if (comp_on_timer_s < 0xFFFFU) {
            comp_on_timer_s++;
        }
        comp_off_timer_s = 0U;
    } else {
        if (comp_off_timer_s < 0xFFFFU) {
            comp_off_timer_s++;
        }
        comp_on_timer_s = 0U;
    }

    /* ---------------- MANUAL MODE ---------------- */
    if (manual_mode) {
        bool manual_comp_req = ((manual_bits & ACT_AC_COMPRESSOR)    != 0U);
        bool manual_fan_req  = ((manual_bits & ACT_AC_CONDENSER_FAN) != 0U);

        if (!allowed_by_fault_and_safety) {
            /* Safety/fault active: hard-off regardless of manual request. */
            comp_state       = COMP_STATE_OFF;
            comp_off_timer_s = 0U;
            comp_on_timer_s  = 0U;
        } else {
            if (bypass_timers) {
                /* Engineering mode: follow manual request directly, no MinOn/Off.
                 * Safety & faults still enforced.
                 */
                comp_state = manual_comp_req ? COMP_STATE_ON : COMP_STATE_OFF;
            } else {
                /* Normal MANUAL: still respect MinOn/Off times. */
                if (comp_state == COMP_STATE_OFF) {
                    /* OFF -> consider turning ON if requested and min off time satisfied */
                    if (manual_comp_req && (comp_off_timer_s >= min_off_s)) {
                        comp_state       = COMP_STATE_ON;
                        comp_on_timer_s  = 0U;
                        comp_off_timer_s = 0U;
                    }
                } else { /* COMP_STATE_ON */
                    /* ON -> consider turning OFF once min on-time satisfied and request removed */
                    if (!manual_comp_req && (comp_on_timer_s >= min_on_s)) {
                        comp_state       = COMP_STATE_OFF;
                        comp_off_timer_s = 0U;
                        comp_on_timer_s  = 0U;
                    }
                    /* Otherwise stay ON until min_on_s is reached */
                }
            }
        }

        /* Apply to actuators:
         *  - Compressor strictly follows comp_state.
         *  - Condenser fan:
         *      ON if safety ok AND (manual fan requested OR compressor running).
         */
        uint16_t bits = REG(MREG_ACTUATOR_STATES);

        if (comp_state == COMP_STATE_ON) {
            bits |= ACT_AC_COMPRESSOR;
        } else {
            bits &= (uint16_t)~ACT_AC_COMPRESSOR;
        }

        if (allowed_by_fault_and_safety &&
            (manual_fan_req || (comp_state == COMP_STATE_ON))) {
            bits |= ACT_AC_CONDENSER_FAN;
        } else {
            bits &= (uint16_t)~ACT_AC_CONDENSER_FAN;
        }

        REG(MREG_ACTUATOR_STATES)    = bits;
        REG(MREG_COMPRESSOR_STATE)   = (comp_state == COMP_STATE_ON) ? 1U : 0U;
        REG(MREG_COMPRESSOR_ALLOWED) = allowed_by_fault_and_safety ? 1U : 0U;

        return;
    }

    /* ---------------- AUTO MODE ---------------- */

    /* A protection trip always wins over the minimum-on timer. */
    if (!allowed_by_fault_and_safety) {
        comp_state       = COMP_STATE_OFF;
        comp_off_timer_s = 0U;
        comp_on_timer_s  = 0U;
        REG(MREG_COMPRESSOR_ALLOWED) = 0U;
        REG(MREG_COMPRESSOR_STATE) = 0U;
        CompressorSupervisor_UpdateActuators();
        return;
    }

    /* "Cooling request" from the high-level mode + no safety/fault */
    bool cooling_request = (cooling_active != 0U) &&
                           (mode_allowed != 0U) &&
                           allowed_by_fault_and_safety;

    /* Final allowed flag: we expose this back to MREG so Pi can see the result. */
    REG(MREG_COMPRESSOR_ALLOWED) = allowed_by_fault_and_safety ? 1U : 0U;

    /* State machine *******************************************************/

    if (comp_state == COMP_STATE_OFF) {
        /* Decide if we may turn ON */
        if (cooling_request) {
            if (comp_off_timer_s >= min_off_s) {
                /* OK to start the compressor */
                comp_state       = COMP_STATE_ON;
                comp_on_timer_s  = 0U;
                comp_off_timer_s = 0U;
            }

        }
        /* else: remain OFF */
    } else { /* COMP_STATE_ON */
        /* Decide if we may turn OFF */
        if (!cooling_request) {
            if (comp_on_timer_s >= min_on_s) {
                comp_state       = COMP_STATE_OFF;
                comp_off_timer_s = 0U;
                comp_on_timer_s  = 0U;
            }
        }
        /* else: remain ON until min_on_s is reached */
    }

    /* Reflect state into registers + actuator bits */
    REG(MREG_COMPRESSOR_STATE) = (comp_state == COMP_STATE_ON) ? 1U : 0U;

    CompressorSupervisor_UpdateActuators();
}





//
//#include "compressor_supervisor.h"
//#include "../system_registers.h"
//
//
///*
// * compressor_supervisor.c
// *
// * Handles safe on/off control of the (DC) compressor:
// *  - Enforces minimum on/off times
// *  - Obeys cooling request and basic "allowed" flag from mode_manager
// *  - Checks safety and compressor-related fault bits
// *  - Drives ACT_DC_COMPRESSOR and ACT_AC_CONDENSER_FAN bits
// *
// * Uses:
// *   MREG_COOLING_ACTIVE       (0/1)   - cooling requested by mode_manager
// *   MREG_COMPRESSOR_ALLOWED   (0/1)   - basic permission from mode_manager
// *   MREG_SAFETY_ACTIVE        (0/1)   - global safety flag
// *   MREG_FAULT_FLAGS          (bitfield) - compressor-related faults
// *   MREG_COMP_MIN_ON_TIME     (s)     - minimum on-time
// *   MREG_COMP_MIN_OFF_TIME    (s)     - minimum off-time
// *
// * Writes:
// *   MREG_COMPRESSOR_STATE     (0/1)   - actual compressor on/off
// *   MREG_COMPRESSOR_ALLOWED   (0/1)   - final allowed flag (mode_manager & safety & faults)
// *   MREG_ACTUATOR_STATES      (bitfield) - ACT_DC_COMPRESSOR, ACT_AC_CONDENSER_FAN
// */
//
//#include <stdint.h>
//#include <stdbool.h>
//
///* --------------------------------------------------------------------------
// * Internal state
// * -------------------------------------------------------------------------- */
//
//typedef enum {
//    COMP_STATE_OFF = 0,
//    COMP_STATE_ON  = 1
//} comp_state_t;
//
//static comp_state_t comp_state         = COMP_STATE_OFF;
//static uint16_t     comp_on_timer_s    = 0;
//static uint16_t     comp_off_timer_s   = 0;
//
///* Defaults (seconds) if registers are zero */
//#define COMP_MIN_ON_DEFAULT_S   180U
//#define COMP_MIN_OFF_DEFAULT_S  180U
//
///* DC Compressor-related fault mask within MREG_FAULT_FLAGS:
// *  - FAULT_COMP_BATTERY_CUTOUT        (1 << 9)
// *  - FAULT_COMP_FAN_OVER_CURRENT      (1 << 10)
// *  - FAULT_COMP_START_ERROR           (1 << 11)
// *  - FAULT_COMP_SPEED_ERROR           (1 << 12)
// *  - FAULT_COMP_THERM_CUTOUT          (1 << 13)
// *  - FAULT_COMP_CONTROL_FAILURE       (1 << 14)
// */
//#define COMP_FAULT_MASK  (
//    (1U << 9)  |
//    (1U << 10) |
//    (1U << 11) |
//    (1U << 12) |
//    (1U << 13) |
//    (1U << 14) )
//
///* --------------------------------------------------------------------------
// * Helpers
// * -------------------------------------------------------------------------- */
//
//static uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi)
//{
//    if (x < lo) return lo;
//    if (x > hi) return hi;
//    return x;
//}
//
///* Apply comp_state to actuator bitfield */
//static void CompressorSupervisor_UpdateActuators(void)
//{
//    uint16_t bits = REG(MREG_ACTUATOR_STATES);
//
//    if (comp_state == COMP_STATE_ON) {
////        bits |= ACT_DC_COMPRESSOR;
//        bits |= ACT_AC_COMPRESSOR;
//        bits |= ACT_AC_CONDENSER_FAN;
//    } else {
////        bits &= (uint16_t)~ACT_DC_COMPRESSOR;
//        bits &= (uint16_t)~ACT_AC_COMPRESSOR;
//        bits &= (uint16_t)~ACT_AC_CONDENSER_FAN;
//    }
//
//    REG(MREG_ACTUATOR_STATES) = bits;
//}
//
///* --------------------------------------------------------------------------
// * Public API
// * -------------------------------------------------------------------------- */
//
//void CompressorSupervisor_Init(void)
//{
//    comp_state       = COMP_STATE_OFF;
//    comp_on_timer_s  = 0;
//    comp_off_timer_s = 0;
//
//    REG(MREG_COMPRESSOR_STATE)   = 0;
//    /* Start with "not allowed" until mode_manager decides otherwise */
//    REG(MREG_COMPRESSOR_ALLOWED) = 0;
//
//    CompressorSupervisor_UpdateActuators();
//}
//
//void CompressorSupervisor_Step_1s(void)
//{
//    uint16_t cooling_active      = REG(MREG_COOLING_ACTIVE);
//    uint16_t mode_allowed        = REG(MREG_COMPRESSOR_ALLOWED); /* from mode_manager */
//    uint16_t safety_active       = REG(MREG_SAFETY_ACTIVE);
//    uint16_t faults              = REG(MREG_FAULT_FLAGS);
//
//    /* Read min on/off from registers, apply defaults if zero */
//    uint16_t min_on_s  = REG(MREG_COMP_MIN_ON_TIME);
//    uint16_t min_off_s = REG(MREG_COMP_MIN_OFF_TIME);
//
//    if (min_on_s == 0)  min_on_s  = COMP_MIN_ON_DEFAULT_S;
//    if (min_off_s == 0) min_off_s = COMP_MIN_OFF_DEFAULT_S;
//
//    /* Any compressor-related fault or safety will block the compressor. */
//    bool has_comp_fault = ((faults & COMP_FAULT_MASK) != 0U);
//    bool allowed_by_fault_and_safety = (!safety_active && !has_comp_fault);
//
//    /* "Cooling request" from the high-level mode + no safety/fault */
//    bool cooling_request = (cooling_active != 0U) && (mode_allowed != 0U) && allowed_by_fault_and_safety;
//
//    /* Final allowed flag: we expose this back to MREG so Pi can see the result. */
//    REG(MREG_COMPRESSOR_ALLOWED) = allowed_by_fault_and_safety ? 1U : 0U;
//
//    /* Update timers */
//    if (comp_state == COMP_STATE_ON) {
//        if (comp_on_timer_s < 0xFFFF) {
//            comp_on_timer_s++;
//        }
//        comp_off_timer_s = 0;
//    } else {
//        if (comp_off_timer_s < 0xFFFF) {
//            comp_off_timer_s++;
//        }
//        comp_on_timer_s = 0;
//    }
//
//    /* State machine *******************************************************/
//
//    if (comp_state == COMP_STATE_OFF) {
//        /* Decide if we may turn ON */
//
//        if (!cooling_request) {
//            /* No demand (or safety/fault issue) -> stay off. */
//        } else {
//            /* There is a cooling request and no safety/fault.
//             * Enforce minimum OFF time before re-starting.
//             */
//            if (comp_off_timer_s >= min_off_s) {
//                /* OK to start the compressor */
//                comp_state       = COMP_STATE_ON;
//                comp_on_timer_s  = 0;
//                comp_off_timer_s = 0;
//            }
//        }
//    } else { /* COMP_STATE_ON */
//        /* Decide if we may turn OFF */
//
//        /* If there's no cooling request or we lost safety/fault permission,
//         * we *want* to turn off, but must honour minimum ON time.
//         */
//        if (!cooling_request || !allowed_by_fault_and_safety) {
//            if (comp_on_timer_s >= min_on_s) {
//                comp_state       = COMP_STATE_OFF;
//                comp_off_timer_s = 0;
//                comp_on_timer_s  = 0;
//            }
//            /* else: remain ON until min_on_s is reached */
//        } else {
//            /* Still requested and allowed: remain ON. */
//        }
//    }
//
//    /* Reflect state into registers + actuator bits */
//    REG(MREG_COMPRESSOR_STATE) = (comp_state == COMP_STATE_ON) ? 1U : 0U;
//
//    CompressorSupervisor_UpdateActuators();
//}


//static uint16_t on_timer = 0;
//static uint16_t off_timer = 0;
//static bool compressor_on = false;
//
//// ---------------------------------------------------------------------------
//// Compressor Supervisor
//// Protects compressor with min ON/OFF times, safety lockouts,
//// and evaporator-box "free cooling" logic.
//// Called once per second.
//// ---------------------------------------------------------------------------
//
//void compressor_supervisor_init(void)
//{
//    compressor_on = false;
//    on_timer = 0;
//    off_timer = 0;
//
//    REG(MREG_COMPRESSOR_STATE)   = 0;
//    REG(MREG_COMPRESSOR_ALLOWED) = 0;
//}
//
//void compressor_supervisor_update(void)
//{
//    // Safety overrides everything
//    if (REG(MREG_SAFETY_ACTIVE))
//    {
//        compressor_on = false;
//        on_timer = 0;
//
//        REG(MREG_COMPRESSOR_STATE)   = 0;
//        REG(MREG_COMPRESSOR_ALLOWED) = 0;
//        return;
//    }
//
//    // Update timers
//    if (compressor_on)
//        on_timer++;
//    else
//        off_timer++;
//
//    // Read inputs
//    bool cooling_req   = REG(MREG_COOLING_ACTIVE);     // from mode manager
//    int16_t T_evap_box = REG(MREG_T_EVAP_BOX);
//    int16_t T_chamber  = REG(MREG_T_CHAMBER);
//
//    uint16_t min_on  = REG(MREG_COMP_MIN_ON_TIME);
//    uint16_t min_off = REG(MREG_COMP_MIN_OFF_TIME);
//
//    // -------------------------------------------------------
//    // Evaporator-box free cooling logic:
//    // If evap box is already cold, skip compressor start.
//    //
//    // Example threshold: evap box 1.0°C colder than chamber.
//    // -------------------------------------------------------
//    if (cooling_req)
//    {
//        if (T_evap_box + 10 < T_chamber) // +10 = 1.0°C
//        {
//            // free cooling available, do not start compressor
//            cooling_req = false;
//        }
//    }
//
//    // -------------------------------------------------------
//    // DECIDE NEW COMPRESSOR STATE
//    // -------------------------------------------------------
//    bool new_state = compressor_on;
//
//    if (cooling_req)
//    {
//        // Request cooling ? compressor ON
//
//        // Respect minimum OFF time
//        if (!compressor_on && off_timer >= min_off)
//        {
//            new_state = true;
//            on_timer = 0;
//        }
//    }
//    else
//    {
//        // No cooling or free cooling ? compressor OFF
//
//        // Respect minimum ON time
//        if (compressor_on && on_timer >= min_on)
//        {
//            new_state = false;
//            off_timer = 0;
//        }
//    }
//
//    compressor_on = new_state;
//
//    // -------------------------------------------------------
//    // Update REG outputs
//    // -------------------------------------------------------
//    REG(MREG_COMPRESSOR_ALLOWED) = compressor_on ? 1 : 0;
//    REG(MREG_COMPRESSOR_STATE)   = compressor_on ? 1 : 0;
//}
//
