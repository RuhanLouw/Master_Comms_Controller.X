
#include "heater_control.h"
#include "../mcc_generated_files/timer/tca0.h"
#include "../system_registers.h"


/*
 * heater_control.c
 *
 * Maps Q_CMD / HEATING_ACTIVE (in AUTO) or MANUAL_HEATER_PWM (in MANUAL)
 * into a heater PWM command, with:
 *  - Safety override
 *  - Heater over-temperature protection
 *
 * Uses:
 *   MREG_SYSTEM_MODE        (0 = Auto, 1 = Manual)
 *   MREG_SAFETY_ACTIVE      (0/1)
 *   MREG_HEATING_ACTIVE     (0/1)
 *   MREG_Q_CMD              (signed, approx -1000..+1000)
 *   MREG_T_HEATER_ELEMENT   (°C×10)
 *   MREG_HEATER_MAX_TEMP    (°C×10)
 *   MREG_MANUAL_HEATER_PWM  (0..1000)
 *
 * Writes:
 *   MREG_HEATER_PWM         (0..1000)
 *   MREG_HEATER_FAULT       (bitfield, bit0 = over-temp)
 *   MREG_ACTUATOR_STATES    (ACT_PWM_HEAT_ELEMENT bit)
 */

#include <stdint.h>
#include <stdbool.h>

/* Heater fault bits (inside MREG_HEATER_FAULT) */
#define HEATER_FAULT_OVERTEMP   (1U << 0)

/* Clamp helpers */
static uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int16_t clamp_i16(int16_t x, int16_t lo, int16_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Apply REG(MREG_HEATER_PWM) (0..1000) to TCA0 CMP0BUF */
static void HEATER_ApplyPwmToHardware(void)
{
    uint16_t pwm = REG(MREG_HEATER_PWM);
    if (pwm > 1000) pwm = 1000;

    uint16_t per = TCA0.SINGLE.PER;
    uint32_t ticks = ((uint32_t)pwm * (uint32_t)per) / 1000U;

    TCA0_Compare0BufferSet((uint16_t)ticks);
}

/* Set/clear ACT_PWM_HEAT_ELEMENT bit based on current heater PWM */
static void HeaterControl_UpdateActuatorBit(uint16_t pwm, bool safety_active)
{
    uint16_t bits = REG(MREG_ACTUATOR_STATES);

    if (!safety_active && pwm > 0) {
        bits |= ACT_PWM_HEAT_ELEMENT;
    } else {
        bits &= (uint16_t)~ACT_PWM_HEAT_ELEMENT;
    }

    REG(MREG_ACTUATOR_STATES) = bits;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void HeaterControl_Init(void)
{
    REG(MREG_HEATER_PWM)   = 0;
    REG(MREG_HEATER_FAULT) = 0;

    /* Make sure heater PWM bit starts cleared */
    HeaterControl_UpdateActuatorBit(0, false);
    HEATER_ApplyPwmToHardware();
}

void HeaterControl_Step_1s(void)
{
    uint16_t system_mode   = REG(MREG_SYSTEM_MODE);   /* 0 = Auto, 1 = Manual */
    uint16_t safety_active = REG(MREG_SAFETY_ACTIVE);

    uint16_t heater_faults = REG(MREG_HEATER_FAULT);
    uint16_t pwm_cmd       = 0;  /* final PWM we will apply (0..1000) */

    /* ---------------- Temperature limit check ---------------- */
    int16_t t_heater_x10   = (int16_t)REG(MREG_T_HEATER_ELEMENT);
    int16_t t_max_x10      = (int16_t)REG(MREG_HEATER_MAX_TEMP);

    bool over_temp = false;
    if (t_max_x10 > 0) {
        if (t_heater_x10 >= t_max_x10) {
            over_temp = true;
        }
    }

    if (over_temp) {
        heater_faults |= HEATER_FAULT_OVERTEMP;
        REG(MREG_FAULT_FLAGS) |= FAULT_AC_HEATER_ERROR;
    } else {
        heater_faults &= (uint16_t)~HEATER_FAULT_OVERTEMP;
        REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_AC_HEATER_ERROR;
    }
    REG(MREG_HEATER_FAULT) = heater_faults;

    /* ---------------- Safety override ----------------
     * Keep evaluating the temperature fault while stopped so it can recover.
     */
    if (safety_active) {
        REG(MREG_HEATER_PWM) = 0U;
        HeaterControl_UpdateActuatorBit(0U, true);
        HEATER_ApplyPwmToHardware();
        return;
    }

    /* If over-temp fault is active, force heater OFF */
    if (over_temp) {
        pwm_cmd = 0;
        REG(MREG_HEATER_PWM) = pwm_cmd;
        HeaterControl_UpdateActuatorBit(pwm_cmd, false);
        HEATER_ApplyPwmToHardware();
        return;
    }

    /* ---------------- Manual vs Auto mode ---------------- */

    if (system_mode != 0) {
        /* MANUAL MODE:
         *  Use manual PWM register directly (clamped).
         */
        uint16_t man_pwm = REG(MREG_MANUAL_HEATER_PWM);
        man_pwm = clamp_u16(man_pwm, 0, 1000);
        pwm_cmd = man_pwm;
    } else {
        /* AUTO MODE:
         *  Use Q_CMD and HEATING_ACTIVE flag.
         *  Q_CMD > 0 : heating demand
         *  Q_CMD < 0 : cooling demand (ignored here)
         */
        uint16_t heating_active = REG(MREG_HEATING_ACTIVE);

        if (!heating_active) {
            pwm_cmd = 0;
        } else {
            int16_t q_cmd = (int16_t)REG(MREG_Q_CMD);  /* -1000..+1000 */

            if (q_cmd <= 0) {
                pwm_cmd = 0;
            } else {
                /* Map Q_CMD directly to PWM in 0..1000 range.
                 * Clamp in case Q_CMD overshoots.
                 */
                q_cmd  = clamp_i16(q_cmd, 0, 1000);
                pwm_cmd = (uint16_t)q_cmd;
            }
        }
    }

    /* Store output and update actuator bitfield */
    REG(MREG_HEATER_PWM) = pwm_cmd;
    HeaterControl_UpdateActuatorBit(pwm_cmd, false);
    HEATER_ApplyPwmToHardware();
}


//// PD1 ? heater fans
//#define HEAT_FAN_PORT PORTD
//#define HEAT_FAN_PIN  PIN0_bm
//
//// Internal
//static uint8_t heater_pwm = 0;
//
//// ---------------------------------------------------------------------------
//// Initialize heater control module
//// ---------------------------------------------------------------------------
//void heater_control_init(void)
//{
//    heater_pwm = 0;
//    REG(MREG_HEATER_PWM) = 0;
//    REG(MREG_HEATER_FAN_STATE) = 0;
//    REG(MREG_HEATER_FAULT) = 0;
//
//    HEAT_FAN_PORT.DIRSET = HEAT_FAN_PIN;
//    HEAT_FAN_PORT.OUTCLR = HEAT_FAN_PIN;  // fans off
//}
//
//// ---------------------------------------------------------------------------
//// Apply PWM using TCA0 WO0 (0?100%)
//// ---------------------------------------------------------------------------
//static void apply_pwm(uint8_t duty)
//{
//    uint16_t per = TCA0.SINGLE.PER;
//    uint16_t cmp = (per * duty) / 100;
//    TCA0.SINGLE.CMP0BUF = cmp;
//}
//
//// ---------------------------------------------------------------------------
//// Main Heater Control
//// Called once per second.
//// ---------------------------------------------------------------------------
//void heater_control_update(void)
//{
//    int16_t T_heater = REG(MREG_T_HEATER_ELEMENT);
//    int16_t T_limit  = REG(MREG_HEATER_MAX_TEMP);
//
//    // -------------------------------------------------------------------
//    // Safety clamp: if heater too hot ? force off
//    // -------------------------------------------------------------------
//    if (T_heater >= T_limit)
//    {
//        heater_pwm = 0;
//        apply_pwm(0);
//        HEAT_FAN_PORT.OUTCLR = HEAT_FAN_PIN;  // fans off
//
//        REG(MREG_HEATER_PWM)       = 0;
//        REG(MREG_HEATER_FAN_STATE) = 0;
//        REG(MREG_HEATER_FAULT)     = 1;
//
//        return;
//    }
//
//    REG(MREG_HEATER_FAULT) = 0;
//
//    // -------------------------------------------------------------------
//    // Manual mode override
//    // -------------------------------------------------------------------
//    if (REG(MREG_SYSTEM_MODE) == 1)
//    {
//        heater_pwm = REG(MREG_MANUAL_HEATER_PWM);
//        if (heater_pwm > 100)
//            heater_pwm = 100;
//
//        apply_pwm(heater_pwm);
//
//        // Fan ON if heater PWM > 0
//        if (heater_pwm > 0)
//        {
//            HEAT_FAN_PORT.OUTSET = HEAT_FAN_PIN;
//            REG(MREG_HEATER_FAN_STATE) = 1;
//        }
//        else
//        {
//            HEAT_FAN_PORT.OUTCLR = HEAT_FAN_PIN;
//            REG(MREG_HEATER_FAN_STATE) = 0;
//        }
//
//        REG(MREG_HEATER_PWM) = heater_pwm;
//        return;
//    }
//
//    // -------------------------------------------------------------------
//    // AUTO mode: only respond if HEATING_ACTIVE
//    // -------------------------------------------------------------------
//    if (!REG(MREG_HEATING_ACTIVE))
//    {
//        heater_pwm = 0;
//        apply_pwm(0);
//
//        HEAT_FAN_PORT.OUTCLR = HEAT_FAN_PIN;
//        REG(MREG_HEATER_FAN_STATE) = 0;
//        REG(MREG_HEATER_PWM) = 0;
//
//        return;
//    }
//
//    // -------------------------------------------------------------------
//    // Convert Q_CMD (positive) into PWM percent
//    // Q_CMD range is -1000?+1000, so we scale:
//    //
//    //   heater_pwm = (Q_CMD / 1000) * 100
//    //
//    // ? PWM = Q_CMD / 10
//    // -------------------------------------------------------------------
//    int16_t Q = REG(MREG_Q_CMD);
//
//    if (Q <= 0)
//        heater_pwm = 0;
//    else
//    {
//        int16_t pwm = Q / 10;
//        if (pwm > 100)
//            pwm = 100;
//
//        heater_pwm = (uint8_t)pwm;
//    }
//
//    // Apply PWM
//    apply_pwm(heater_pwm);
//
//    // Fans ON when heating
//    if (heater_pwm > 0)
//    {
//        HEAT_FAN_PORT.OUTSET = HEAT_FAN_PIN;
//        REG(MREG_HEATER_FAN_STATE) = 1;
//    }
//    else
//    {
//        HEAT_FAN_PORT.OUTCLR = HEAT_FAN_PIN;
//        REG(MREG_HEATER_FAN_STATE) = 0;
//    }
//
//    REG(MREG_HEATER_PWM) = heater_pwm;
//}
//
