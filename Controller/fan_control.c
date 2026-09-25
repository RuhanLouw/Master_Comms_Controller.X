
#include "fan_control.h"
#include "../system_registers.h"
#include "../mcc_generated_files/system/pins.h"
#include "../mcc_generated_files/system/port.h"
#include <stdbool.h>


/*
 * fan_control.c
 *
 * Controls:
 *   - Heater fans  (ACT_HEAT_FANS, PD1)
 *   - Cool fans    (ACT_COOL_FANS, PD2)
 *
 * Behaviour:
 *   MANUAL mode:
 *     - Uses MREG_HEATER_FAN_STATE and MREG_COOL_FAN_STATE as direct commands
 *       (0 = off, 1 = on), written by the Pi.
 *
 *   AUTO mode:
 *     - Heater fans ON when HEATING_ACTIVE = 1 (and safety is OK).
 *     - Cool fans ON when COOLING_ACTIVE = 1 or FREE_COOL_ACTIVE = 1
 *       (and safety is OK).
 *
 * Safety:
 *   - If MREG_SAFETY_ACTIVE != 0, both sets of fans are forced OFF.
 *
 * Uses:
 *   MREG_SYSTEM_MODE       (0 = Auto, 1 = Manual)
 *   MREG_SAFETY_ACTIVE     (0/1)
 *   MREG_HEATING_ACTIVE    (0/1)
 *   MREG_COOLING_ACTIVE    (0/1)
 *   MREG_FREE_COOL_ACTIVE  (0/1)
 *   MREG_HEATER_FAN_STATE  (0/1)  - manual cmd in MANUAL, status in AUTO
 *   MREG_COOL_FAN_STATE    (0/1)  - manual cmd in MANUAL, status in AUTO
 *
 * Writes:
 *   MREG_HEATER_FAN_STATE  (0/1)  - actual state
 *   MREG_COOL_FAN_STATE    (0/1)  - actual state
 *   MREG_ACTUATOR_STATES   (ACT_HEAT_FANS, ACT_COOL_FANS bits)
 */

#include <stdint.h>
#include <stdbool.h>

/* Small helper to set/clear bits */
static void FanControl_UpdateActuatorBits(uint16_t heat_on, uint16_t cool_on)
{
    uint16_t bits = REG(MREG_ACTUATOR_STATES);

    if (heat_on) {
        bits |= ACT_HEAT_FANS;
    } else {
        bits &= (uint16_t)~ACT_HEAT_FANS;
    }

    if (cool_on) {
        bits |= ACT_COOL_FANS;
    } else {
        bits &= (uint16_t)~ACT_COOL_FANS;
    }

    REG(MREG_ACTUATOR_STATES) = bits;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void FanControl_Init(void)
{
    REG(MREG_HEATER_FAN_STATE) = 0;
    REG(MREG_COOL_FAN_STATE)   = 0;

    FanControl_UpdateActuatorBits(0, 0);
}

void FanControl_Step_1s(void)
{
    uint16_t system_mode   = REG(MREG_SYSTEM_MODE);   /* 0 = Auto, 1 = Manual */
    uint16_t safety_active = REG(MREG_SAFETY_ACTIVE);
    uint16_t heat_fan_on = 0;
    uint16_t cool_fan_on = 0;

    /* Safety override: both fans OFF */
    if (safety_active) {
        REG(MREG_HEATER_FAN_STATE) = 0;
        REG(MREG_COOL_FAN_STATE)   = 0;
        FanControl_UpdateActuatorBits(0, 0);
        return;
    }

    if (system_mode != 0) {
        /* ---------------- MANUAL MODE ----------------
         * Pi writes the fan states directly into the registers.
         * We mirror them into the actuator bitfield.
         */
        heat_fan_on = (REG(MREG_HEATER_FAN_STATE) != 0) ? 1U : 0U;
        cool_fan_on = (REG(MREG_COOL_FAN_STATE)   != 0) ? 1U : 0U;

    } else {
        /* ---------------- AUTO MODE ----------------
         * Decide fans based on HEATING/COOLING/FREE_COOL flags.
         */
        uint16_t freecool_active  = REG(MREG_FREE_COOL_ACTIVE);
        uint16_t freeheat_active  = REG(MREG_FREE_HEATING_ACTIVE);

        /* Heater fans: on whenever we are actively heating */
        if(freeheat_active != 0){
            heat_fan_on = 1U;
        }
        else {
            heat_fan_on = 0U;
        }

        /* Cool fans: on when we are cooling or using free cooling */
        if (freecool_active != 0) {
            cool_fan_on = 1U;
        } else {
            cool_fan_on = 0U;
        }

        /* Store actual states back into the registers so the Pi can monitor them */
        REG(MREG_HEATER_FAN_STATE) = heat_fan_on;
        REG(MREG_COOL_FAN_STATE)   = cool_fan_on;
    }

    /* Apply to actuator bitfield */
    FanControl_UpdateActuatorBits(heat_fan_on, cool_fan_on);
}


//// PD2 ? evaporator fans
//#define EVAP_FAN_PORT PORTD
//#define EVAP_FAN_PIN  PIN2_bm
//
//void fan_control_init(void)
//{
//    EVAP_FAN_PORT.DIRSET = EVAP_FAN_PIN;
//    EVAP_FAN_PORT.OUTCLR = EVAP_FAN_PIN;
//
//    REG(MREG_COOL_FAN_STATE)   = 0;
//    REG(MREG_FREE_COOL_MARKER) = 0;
//}
//
//void fan_control_update(void)
//{
//    // Safety override: all fans off
//    if (REG(MREG_SAFETY_ACTIVE))
//    {
//        EVAP_FAN_PORT.OUTCLR = EVAP_FAN_PIN;
//        REG(MREG_COOL_FAN_STATE) = 0;
//        REG(MREG_FREE_COOL_MARKER) = 0;
//        return;
//    }
//
//    // -------------------------------------------------------
//    // Manual Mode
//    // -------------------------------------------------------
//    if (REG(MREG_SYSTEM_MODE) == 1)
//    {
//        uint16_t man = REG(MREG_ACTUATOR_STATES);
//
//        if (man & (1 << ACT_COOL_FANS))     // bit9 = COOL_FAN
//        {
//            EVAP_FAN_PORT.OUTSET = EVAP_FAN_PIN;
//            REG(MREG_COOL_FAN_STATE) = 1;
//        }
//        else
//        {
//            EVAP_FAN_PORT.OUTCLR = EVAP_FAN_PIN;
//            REG(MREG_COOL_FAN_STATE) = 0;
//        }
//
//        REG(MREG_FREE_COOL_MARKER) = 0;
//        return;
//    }
//
//    // -------------------------------------------------------
//    // AUTO MODE
//    // -------------------------------------------------------
//
//    int16_t T_evap   = REG(MREG_T_EVAP_BOX);
//    int16_t T_ch     = REG(MREG_T_CHAMBER);
//
//    bool free_req  = REG(MREG_FREE_COOL_MARKER);
//    bool cool_req  = REG(MREG_COOLING_ACTIVE);   // active cooling request
//
//    // Heater fans handled by heater_control, not here
//    // So we only decide EVAP fans here
//
//    // -------------------------------------------------------
//    // FREE COOLING MODE (fan-only)
//    //
//    // Use evaporator air if:
//    //   evap box is colder than chamber by at least 0.5°C
//    // -------------------------------------------------------
//    if (free_req)
//    {
//        if (T_evap + 5 < T_ch)  // +5 = 0.5°C
//        {
//            // Free cooling effective
//            EVAP_FAN_PORT.OUTSET = EVAP_FAN_PIN;
//            REG(MREG_COOL_FAN_STATE)  = 1;
//            REG(MREG_FREE_COOL_MARKER) = 1;
//        }
//        else
//        {
//            // No free cooling benefit
//            EVAP_FAN_PORT.OUTCLR = EVAP_FAN_PIN;
//            REG(MREG_COOL_FAN_STATE)  = 0;
//            REG(MREG_FREE_COOL_MARKER) = 0;
//        }
//        return;
//    }
//
//    // -------------------------------------------------------
//    // ACTIVE COOLING (compressor ON)
//    //
//    // Evap fans ALWAYS ON during active cooling
//    // -------------------------------------------------------
//    if (cool_req)
//    {
//        EVAP_FAN_PORT.OUTSET = EVAP_FAN_PIN;
//        REG(MREG_COOL_FAN_STATE)  = 1;
//        REG(MREG_FREE_COOL_MARKER) = 0;
//        return;
//    }
//
//    // -------------------------------------------------------
//    // Otherwise ? IDLE
//    // -------------------------------------------------------
//    EVAP_FAN_PORT.OUTCLR = EVAP_FAN_PIN;
//    REG(MREG_COOL_FAN_STATE)  = 0;
//    REG(MREG_FREE_COOL_MARKER) = 0;
//}
