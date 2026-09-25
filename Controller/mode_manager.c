#include "mode_manager.h"
#include "../system_registers.h"

/*
 * mode_manager.c
 *
 * High-level mode decision for the chamber:
 *   - HEAT / COOL / FREE_COOL / IDLE / MANUAL
 *
 * Uses:
 *   MREG_SYSTEM_MODE              (0 = Auto, 1 = Manual)
 *   MREG_T_CHAMBER                (°C×10) - chamber air
 *   MREG_T_TRAJ                   (°C×10) - trajectory target
 *   MREG_T_EVAP_BOX               (°C×10) - evap box air temp
 *   MREG_TEMP_HYSTERESIS          (°C×10) - deadband around target
 *   MREG_SAFETY_ACTIVE            (0/1)
 *   MREG_MANUAL_ACTUATOR_STATES   (bitfield) - manual actuator bits (for orange pilot)
 *
 * Writes:
 *   MREG_SYS_STATE                (enum: IDLE / HEAT / COOL / FREE_COOL / MANUAL)
 *   MREG_HEATING_ACTIVE           (0/1)
 *   MREG_COOLING_ACTIVE           (0/1)   - request for compressor-based cooling
 *   MREG_FREE_COOL_ACTIVE         (0/1)   - cooling with fans only (no compressor)
 *   MREG_COMPRESSOR_ALLOWED       (0/1)   - basic gate; compressor_supervisor enforces timing, etc.
 */

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Local enum for SYS_STATE (stored in MREG_SYS_STATE)
 * -------------------------------------------------------------------------- */

typedef enum {
    SYS_STATE_IDLE      = 0,
    SYS_STATE_HEAT      = 1,
    SYS_STATE_COOL      = 2,
    SYS_STATE_FREE_COOL = 3,
    SYS_STATE_DEFROST   = 4,  /* reserved for later */
    SYS_STATE_MANUAL    = 5,
    SYS_STATE_FREE_HEATING = 6
} sys_state_t;

static void ModeManager_UpdateStatus(sys_state_t state)
{
    uint16_t status = (uint16_t)REG(MREG_SYS_STATUS);
    status &= (uint16_t)~(SYS_STATUS_IDLE | SYS_STATUS_COOLING |
                          SYS_STATUS_HEATING | SYS_STATUS_DEFROST |
                          SYS_STATUS_MANUAL_MODE);

    switch (state) {
        case SYS_STATE_HEAT:
        case SYS_STATE_FREE_HEATING:
            status |= SYS_STATUS_HEATING;
            break;
        case SYS_STATE_COOL:
        case SYS_STATE_FREE_COOL:
            status |= SYS_STATUS_COOLING;
            break;
        case SYS_STATE_DEFROST:
            status |= SYS_STATUS_DEFROST;
            break;
        case SYS_STATE_MANUAL:
            status |= SYS_STATUS_MANUAL_MODE;
            break;
        case SYS_STATE_IDLE:
        default:
            status |= SYS_STATUS_IDLE;
            break;
    }

    REG(MREG_SYS_STATUS) = status;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void ModeManager_Init(void)
{
    REG(MREG_SYS_STATE)          = (uint16_t)SYS_STATE_IDLE;
    REG(MREG_HEATING_ACTIVE)     = 0;
    REG(MREG_COOLING_ACTIVE)     = 0;
    REG(MREG_FREE_COOL_ACTIVE)   = 0;
    REG(MREG_COMPRESSOR_ALLOWED) = 0;
}

/*
 * Called once per second from main control loop.
 */
void ModeManager_Step_1s(void)
{
    uint16_t system_mode   = REG(MREG_SYSTEM_MODE);    /* 0 = Auto, 1 = Manual */
    uint16_t safety_active = REG(MREG_SAFETY_ACTIVE);

    /* ---------------- MANUAL MODE ----------------
     * In manual mode we don't decide HEAT/COOL. We just:
     *   - mark the state as MANUAL
     *   - clear automatic mode flags
     *   - leave compressor permission & actuator bits to other modules
     *   - drive orange pilot based on manual actuators
     */
    if (system_mode != 0U) {
        REG(MREG_SYS_STATE)          = (uint16_t)SYS_STATE_MANUAL;
        REG(MREG_HEATING_ACTIVE)     = 0U;
        REG(MREG_FREE_HEATING_ACTIVE)= 0U;
        REG(MREG_COOLING_ACTIVE)     = 0U;
        REG(MREG_FREE_COOL_ACTIVE)   = 0U;
        REG(MREG_COMPRESSOR_ALLOWED) = 0U;
        ModeManager_UpdateStatus(SYS_STATE_MANUAL);

        uint16_t bits        = REG(MREG_ACTUATOR_STATES);
        uint16_t manual_bits = REG(MREG_MANUAL_ACTUATOR_STATES);

        uint16_t mask = (ACT_DC_COMPRESSOR | ACT_AC_COMPRESSOR);

        if ((manual_bits & mask) != 0) {
            bits |= ACT_DC_ORANGE_PILOT;
        } else {
            bits &= (uint16_t)~ACT_DC_ORANGE_PILOT;
        }

        REG(MREG_ACTUATOR_STATES) = bits;
        return;
    }

    /* ---------------- AUTO MODE ---------------- */

    int16_t t_ch_x10   = (int16_t)REG(MREG_T_CHAMBER);        /* °C×10 */
    int16_t t_traj_x10 = (int16_t)REG(MREG_T_TRAJ);           /* °C×10 */
    int16_t t_box_x10  = (int16_t)REG(MREG_T_EVAP_BOX);        /* °C×10 */

    /* error_x10 > 0  -> chamber is below target -> need heating
     * error_x10 < 0  -> chamber is above target -> need cooling
     */
    int16_t error_x10 = t_traj_x10 - t_ch_x10;

    sys_state_t new_state     = SYS_STATE_IDLE;
    uint16_t heating_active   = 0U;
    uint16_t freeheat_active  = 0U;
    uint16_t cooling_active   = 0U;
    uint16_t freecool_active  = 0U;
    uint16_t comp_allowed     = 0U;

    if (safety_active) {
        new_state = SYS_STATE_IDLE;
    } else {
        // Heating -------
        if (t_ch_x10 < t_traj_x10) {
            // Heating element
            new_state = SYS_STATE_HEAT;
            heating_active = 1U;
        }
        // Heating fans?
        if (t_ch_x10 < (t_traj_x10 - 3)) freeheat_active = 1U;
        else freeheat_active = 0U;
        
        // Cooling -------
        if (error_x10 < 0) {
            /* Well above target -> cooling of some form */
            /* Check if we can do free cooling:
             * - evap box is at least FREECOOL_MIN_DT_X10 cooler than chamber
             */

            // FreeCool Fans?
            if (t_ch_x10 > (t_traj_x10 + 2)) {
                new_state       = SYS_STATE_FREE_COOL;
                freecool_active = 1U;
//                new_state      = SYS_STATE_COOL;
                /* Free cool does NOT require compressor */
            }
            else freecool_active = 0U;
            
            // Compressor?
            if((t_ch_x10 < (t_traj_x10 - 100)) || (t_box_x10 > (t_traj_x10 - 350) )) { 
                new_state      = SYS_STATE_COOL;
                cooling_active = 1U;
            }
            else cooling_active = 0U;
            
        }
    }

    if (!safety_active && (new_state == SYS_STATE_COOL)) {
        comp_allowed = 1U;
    } else {
        comp_allowed = 0U;
    }

    REG(MREG_SYS_STATE)          = (uint16_t)new_state;
    REG(MREG_HEATING_ACTIVE)     = heating_active;
    REG(MREG_FREE_HEATING_ACTIVE)= freeheat_active;
    REG(MREG_COOLING_ACTIVE)     = cooling_active;
    REG(MREG_FREE_COOL_ACTIVE)   = freecool_active;
    REG(MREG_COMPRESSOR_ALLOWED) = comp_allowed;
    ModeManager_UpdateStatus(new_state);
    
    /* Orange pilot in AUTO: ON when doing thermal work */
    uint16_t bits = REG(MREG_ACTUATOR_STATES);

    if (heating_active || cooling_active || freecool_active) {
        bits |= ACT_DC_ORANGE_PILOT;
    } else {
        bits &= (uint16_t)~ACT_DC_ORANGE_PILOT;
    }

    REG(MREG_ACTUATOR_STATES) = bits;
}






//
//#include "mode_manager.h"
//#include "../system_registers.h"
//
//
///*
// * mode_manager.c
// *
// * High-level mode decision for the chamber:
// *   - HEAT / COOL / FREE_COOL / IDLE / MANUAL
// *
// * Uses:
// *   MREG_SYSTEM_MODE       (0 = Auto, 1 = Manual)
// *   MREG_T_CHAMBER         (°C×10) - chamber air
// *   MREG_T_TRAJ            (°C×10) - trajectory target
// *   MREG_T_EVAP_BOX        (°C×10) - evap box air temp
// *   MREG_TEMP_HYSTERESIS   (°C×10) - deadband around target
// *   MREG_SAFETY_ACTIVE     (0/1)
// *
// * Writes:
// *   MREG_SYS_STATE         (enum: IDLE / HEAT / COOL / FREE_COOL / MANUAL)
// *   MREG_HEATING_ACTIVE    (0/1)
// *   MREG_COOLING_ACTIVE    (0/1)   - request for compressor-based cooling
// *   MREG_FREE_COOL_ACTIVE  (0/1)   - cooling with fans only (no compressor)
// *   MREG_COMPRESSOR_ALLOWED(0/1)   - basic gate; compressor_supervisor enforces timing, etc.
// */
//
//#include <stdint.h>
//
///* --------------------------------------------------------------------------
// * Local enum for SYS_STATE (stored in MREG_SYS_STATE)
// * -------------------------------------------------------------------------- */
//
//typedef enum {
//    SYS_STATE_IDLE      = 0,
//    SYS_STATE_HEAT      = 1,
//    SYS_STATE_COOL      = 2,
//    SYS_STATE_FREE_COOL = 3,
//    SYS_STATE_DEFROST   = 4,  /* reserved for later */
//    SYS_STATE_MANUAL    = 5
//} sys_state_t;
//
///* --------------------------------------------------------------------------
// * Config constants
// * -------------------------------------------------------------------------- */
//
///* Minimum difference between chamber and evap box to consider "free cool"
// * in 0.1°C units, e.g. 10 = 1.0°C cooler.
// */
////#define FREECOOL_MIN_DT_X10  7  /* 1.0°C */
//
///* Hysteresis default if register is zero or negative (0.5°C) */
//#define HYST_DEFAULT_X10     5
//
///* Clamp helper */
//static int16_t clamp_i16(int16_t x, int16_t lo, int16_t hi)
//{
//    if (x < lo) return lo;
//    if (x > hi) return hi;
//    return x;
//}
//
///* --------------------------------------------------------------------------
// * Public API
// * -------------------------------------------------------------------------- */
//
//void ModeManager_Init(void)
//{
//    REG(MREG_SYS_STATE)         = (uint16_t)SYS_STATE_IDLE;
//    REG(MREG_HEATING_ACTIVE)    = 0;
//    REG(MREG_COOLING_ACTIVE)    = 0;
//    REG(MREG_FREE_COOL_ACTIVE)  = 0;
//    REG(MREG_COMPRESSOR_ALLOWED)= 0;
//}
//
///*
// * Called once per second from main control loop.
// */
//void ModeManager_Step_1s(void)
//{
//    uint16_t system_mode   = REG(MREG_SYSTEM_MODE);    /* 0 = Auto, 1 = Manual */
//    uint16_t safety_active = REG(MREG_SAFETY_ACTIVE);
//
//    /* ---------------- MANUAL MODE ----------------
//     * In manual mode we don't decide HEAT/COOL. We just mark the state as MANUAL
//     * and clear the automatic mode flags. Manual logic (manual_* registers +
//     * actuator bits) is handled elsewhere.
//     */
//    if (system_mode != 0) {
//        REG(MREG_SYS_STATE)         = (uint16_t)SYS_STATE_MANUAL;
////        REG(MREG_HEATING_ACTIVE)    = 0;
////        REG(MREG_COOLING_ACTIVE)    = 0;
//        REG(MREG_FREE_COOL_ACTIVE)  = 0;
//        /* In MANUAL, we let other code decide if compressor is allowed. For
//         * now, keep this at 0 so automatic cooling doesn't start.
//         */
////        REG(MREG_COMPRESSOR_ALLOWED)= 0;
//        return;
//    }
//
//    /* ---------------- AUTO MODE ---------------- */
//
//    int16_t t_ch_x10   = (int16_t)REG(MREG_T_CHAMBER);   /* °C×10 */
//    int16_t t_traj_x10 = (int16_t)REG(MREG_T_TRAJ);      /* °C×10 */
//    int16_t t_box_x10  = (int16_t)(REG(MREG_T_EVAP_BOX)/10);  /* °C×10 */
//
//    int16_t hyst_x10   = (int16_t)REG(MREG_TEMP_HYSTERESIS);
//    if (hyst_x10 <= 0) {
//        hyst_x10 = HYST_DEFAULT_X10;
//    }
//
//    /* error_x10 > 0  -> chamber is below target -> need heating
//     * error_x10 < 0  -> chamber is above target -> need cooling
//     */
//    int16_t error_x10 = t_traj_x10 - t_ch_x10;
//
//    sys_state_t new_state;
//    uint16_t heating_active   = 0;
//    uint16_t cooling_active   = 0;
//    uint16_t freecool_active  = 0;
//    uint16_t comp_allowed     = 0;
//
//    if (safety_active) {
//        /* If safety is active, force IDLE from the perspective of mode
//         * decision; safety_manager will enforce actual shutdown.
//         */
//        new_state = SYS_STATE_IDLE;
//    } else {
//        /* Decide heat / cool / idle / freecool based on error and evap box */
//        if (error_x10 > hyst_x10) {
//            /* Well below target -> heating */
//            new_state      = SYS_STATE_HEAT;
//            heating_active = 1;
//        }
//        else if (error_x10 < -hyst_x10) {
//            /* Well above target -> cooling of some form */
//            /* Check if we can do free cooling:
//             * - evap box is at least FREECOOL_MIN_DT_X10 cooler than chamber
//             */
//            if ((t_box_x10 + FREECOOL_MIN_DT_X10) == t_ch_x10) {
////                new_state       = SYS_STATE_FREE_COOL;
////                freecool_active = 1;
//                new_state      = SYS_STATE_COOL;
//                cooling_active = 1;
//                /* Free cool does NOT require compressor */
//            } else {
//                new_state      = SYS_STATE_COOL;
//                cooling_active = 1;
//            }
//        }
//        else {
//            /* Within hysteresis band -> idle */
//            new_state = SYS_STATE_IDLE;
//        }
//    }
//
//    /* Basic compressor permission:
//     * - Only allow compressor if we are in COOL mode and safety is not active.
//     * - compressor_supervisor will further enforce min on/off, etc.
//     */
//    if (!safety_active && (new_state == SYS_STATE_COOL)) {
//        comp_allowed = 1;
//    } else {
//        comp_allowed = 0;
//    }
//
//    /* Store results into registers */
//    REG(MREG_SYS_STATE)         = (uint16_t)new_state;
//    REG(MREG_HEATING_ACTIVE)    = heating_active;
//    REG(MREG_COOLING_ACTIVE)    = cooling_active;
//    REG(MREG_FREE_COOL_ACTIVE)  = freecool_active;
//    REG(MREG_COMPRESSOR_ALLOWED)= comp_allowed;
//    
//    /* --------------------------------------------------
//     * Pilot LED: ORANGE ("yellow") when doing thermal work
//     *   - ON  if heating_active || cooling_active || freecool_active
//     *   - OFF otherwise
//     *   This is independent of safety (red/green).
//     * -------------------------------------------------- */
//    uint16_t bits = REG(MREG_ACTUATOR_STATES);
//
//    if (heating_active || cooling_active || freecool_active) {
//        bits |= ACT_DC_ORANGE_PILOT;
//    } else {
//        bits &= (uint16_t)~ACT_DC_ORANGE_PILOT;
//    }
//
//    REG(MREG_ACTUATOR_STATES) = bits;
//}


//static mode_t current_mode = MODE_IDLE;
//
//mode_t mode_manager_get(void)
//{
//    return current_mode;
//}
//
//// ---------------------------------------------------------------------------
//// Mode Manager
//// Decides operating mode from:
////   - Manual override
////   - PID request (Q_CMD)
////   - Safety manager
//// Outputs mode flags for the actuator modules.
//// Called once per second.
//// ---------------------------------------------------------------------------
//
//void mode_manager_update(void)
//{
//    // Manual mode override
//    if (REG(MREG_SYSTEM_MODE) == 1)
//    {
//        current_mode = MODE_MANUAL;
//
//        REG(MREG_SYS_STATE)       = MODE_MANUAL;
//        REG(MREG_HEATING_ACTIVE)  = 0;
//        REG(MREG_COOLING_ACTIVE)  = 0;
//        REG(MREG_FREE_COOL_ACTIVE)= 0;
//
//        return;
//    }
//
//    // Safety shutdown -> force IDLE
//    if (REG(MREG_SAFETY_ACTIVE) == 1)
//    {
//        current_mode = MODE_IDLE;
//
//        REG(MREG_SYS_STATE)       = MODE_IDLE;
//        REG(MREG_HEATING_ACTIVE)  = 0;
//        REG(MREG_COOLING_ACTIVE)  = 0;
//        REG(MREG_FREE_COOL_ACTIVE)= 0;
//
//        return;
//    }
//
//    int16_t Q = REG(MREG_Q_CMD);
//
//    // -------------------------------------------------------------------
//    // HEATING
//    // Positive Q_CMD = heating request
//    // -------------------------------------------------------------------
//    if (Q > 0)
//    {
//        current_mode = MODE_HEAT;
//
//        REG(MREG_SYS_STATE)        = MODE_HEAT;
//        REG(MREG_HEATING_ACTIVE)   = 1;
//        REG(MREG_COOLING_ACTIVE)   = 0;
//        REG(MREG_FREE_COOL_ACTIVE) = 0;
//
//        return;
//    }
//
//    // -------------------------------------------------------------------
//    // COOLING
//    // Negative Q_CMD = cooling request
//    // - Small negative - use fans only (free cooling)
//    // - Larger negative - compressor + evap (active cooling)
//    //
//    // Threshold can be tuned later.
//    // Example:
//    //   |Q| < 100  - free cooling
//    //   |Q| >=100  - full cooling
//    // -------------------------------------------------------------------
//    if (Q < 0)
//    {
//        if (Q > -100)
//        {
//            current_mode = MODE_COOL_FANS_ONLY;
//
//            REG(MREG_SYS_STATE)        = MODE_COOL_FANS_ONLY;
//            REG(MREG_HEATING_ACTIVE)   = 0;
//            REG(MREG_COOLING_ACTIVE)   = 0;   // no compressor
//            REG(MREG_FREE_COOL_ACTIVE) = 1;
//
//            return;
//        }
//        else
//        {
//            current_mode = MODE_COOL_ACTIVE;
//
//            REG(MREG_SYS_STATE)        = MODE_COOL_ACTIVE;
//            REG(MREG_HEATING_ACTIVE)   = 0;
//            REG(MREG_COOLING_ACTIVE)   = 1;   // compressor allowed
//            REG(MREG_FREE_COOL_ACTIVE) = 0;
//
//            return;
//        }
//    }
//
//    // -------------------------------------------------------------------
//    // Otherwise - IDLE
//    // (Q_CMD is near zero)
//    // -------------------------------------------------------------------
//    current_mode = MODE_IDLE;
//
//    REG(MREG_SYS_STATE)        = MODE_IDLE;
//    REG(MREG_HEATING_ACTIVE)   = 0;
//    REG(MREG_COOLING_ACTIVE)   = 0;
//    REG(MREG_FREE_COOL_ACTIVE) = 0;
//}
