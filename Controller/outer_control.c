
#include "outer_control.h"
#include "../system_registers.h"


/*
 * outer_control.c
 *
 * Outer temperature controller for the chamber.
 *
 * Uses:
 *   MREG_T_CHAMBER    (°C×10)    - measured chamber temperature
 *   MREG_T_TRAJ       (°C×10)    - trajectory / smoothed target
 *   MREG_PID_KP       (×100)     - proportional gain
 *   MREG_PID_KI       (×100)     - integral gain
 *   MREG_PID_KD       (×100)     - derivative gain
 *   MREG_PID_I_CLAMP  (same as output units) - integral clamp
 *
 * Writes:
 *   MREG_Q_CMD            (signed, approx -1000..+1000)
 *   MREG_PID_OUTPUT       (same as Q_CMD)
 *   MREG_PID_ERROR        (error in 0.1°C units)
 *   MREG_PID_INTEGRAL     (I term, same units as Q_CMD)
 *   MREG_PID_DERIVATIVE   (D term, same units as Q_CMD)
 *
 * Sign convention:
 *   error = T_traj - T_chamber
 *   Q_CMD > 0  -> heating demand
 *   Q_CMD < 0  -> cooling demand
 */

#include <stdint.h>

/* Internal PID state */
static int16_t  prev_error_x10 = 0;   /* previous error in 0.1°C */
static int32_t  i_term         = 0;   /* integral term in output units */
static uint8_t  first_run      = 1;   /* flag to initialise derivative */

static int32_t clamp_i32(int32_t x, int32_t lo, int32_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void OuterControl_Init(void)
{
    prev_error_x10 = 0;
    i_term         = 0;
    first_run      = 1;

    REG(MREG_Q_CMD)          = 0;
    REG(MREG_PID_OUTPUT)     = 0;
    REG(MREG_PID_ERROR)      = 0;
    REG(MREG_PID_INTEGRAL)   = 0;
    REG(MREG_PID_DERIVATIVE) = 0;
}

/*
 * Called once per second from main control loop.
 */
void OuterControl_Step_1s(void)
{
    uint16_t system_mode = REG(MREG_SYSTEM_MODE);
    uint16_t safety_active = REG(MREG_SAFETY_ACTIVE);
    if ((system_mode != 0U) || (safety_active != 0U)) {
        prev_error_x10 = 0;
        i_term = 0;
        first_run = 1U;
        REG(MREG_Q_CMD) = 0;
        REG(MREG_PID_OUTPUT) = 0;
        REG(MREG_PID_ERROR) = 0;
        REG(MREG_PID_INTEGRAL) = 0;
        REG(MREG_PID_DERIVATIVE) = 0;
        return;
    }
    
    /* Read temperatures (°C×10) */
    int16_t t_meas_x10 = (int16_t)REG(MREG_T_CHAMBER);
    int16_t t_traj_x10 = (int16_t)REG(MREG_T_TRAJ);

    /* error_x10 in 0.1°C units: positive when we are below target (need heat) */
    int32_t raw_error_x10 = (int32_t)t_traj_x10 - (int32_t)t_meas_x10;
    int16_t error_x10 = (int16_t)clamp_i32(raw_error_x10, INT16_MIN, INT16_MAX);

    /* Read gains (×100) */
    int16_t Kp_x100 = (int16_t)REG(MREG_PID_KP);
    int16_t Ki_x100 = (int16_t)REG(MREG_PID_KI);
    int16_t Kd_x100 = (int16_t)REG(MREG_PID_KD);

    /* Integral clamp (in same units as output) */
    int16_t i_clamp = (int16_t)REG(MREG_PID_I_CLAMP);

    /* Store raw error for diagnostics */
    REG(MREG_PID_ERROR) = (uint16_t)error_x10;

    /* ---------------- P-term ----------------
     * P_term = Kp * error
     * With Kp scaled ×100 and error in 0.1°C, we do:
     *   P_term = (Kp_x100 * error_x10) / 100
     * to bring result back into "output units".
     */
    int32_t P = ((int32_t)Kp_x100 * (int32_t)error_x10) / 100;
    P = clamp_i32(P, INT16_MIN, INT16_MAX);

    /* ---------------- I-term ----------------
     * i_term is stored in output units.
     * Update:
     *   i_term += (Ki_x100 * error_x10) / 100
     * Then clamp to ±i_clamp.
     */
    if (i_clamp > 0) {
        int32_t I_delta = ((int32_t)Ki_x100 * (int32_t)error_x10) / 100;
        i_term += I_delta;
        i_term = clamp_i32(i_term, -(int32_t)i_clamp, (int32_t)i_clamp);
    } else {
        i_term = 0;
    }

    /* ---------------- D-term ----------------
     * D_term = Kd * (error - prev_error)
     * Again with Kd×100 and error in 0.1°C:
     *   D_term = (Kd_x100 * (error_x10 - prev_error_x10)) / 100
     */
    int32_t d_error_x10 = 0;
    if (!first_run) {
        d_error_x10 = (int32_t)error_x10 - (int32_t)prev_error_x10;
    }

    int32_t D = ((int32_t)Kd_x100 * d_error_x10) / 100;
    D = clamp_i32(D, INT16_MIN, INT16_MAX);

    /* First run: force D-term to zero to avoid spike */
    if (first_run) {
        D = 0;
        first_run = 0;
    }

    /* ---------------- Sum ---------------- */
    int32_t out = P + (int32_t)i_term + D;

    /* Clamp final output to a reasonable range, e.g. -1000..+1000 */
    out = clamp_i32(out, -1000, 1000);

    int16_t q_cmd = (int16_t)out;

    /* Store back into registers */
    REG(MREG_Q_CMD)          = (uint16_t)q_cmd;
    REG(MREG_PID_OUTPUT)     = (uint16_t)q_cmd;
    REG(MREG_PID_INTEGRAL)   = (uint16_t)i_term;
    REG(MREG_PID_DERIVATIVE) = (uint16_t)D;

    /* Update history */
    prev_error_x10 = error_x10;
}







// ---------------------------------------------------------------------------
//// Main chamber PID controller.
//// Inputs: T_TRAJ, T_CHAMBER
//// Output: Q_CMD  (signed demand, e.g. -1000?+1000)
//// Called once per second.
//// ---------------------------------------------------------------------------
//
//static int32_t integral = 0;
//static int16_t prev_error = 0;
//
//void outer_control_reset(void)
//{
//    integral = 0;
//    prev_error = 0;
//    REG(MREG_Q_CMD) = 0;
//}
//
//void outer_control_update(void)
//{
//    // Read temperatures (0.1°C units)
//    int16_t T_traj    = REG(MREG_T_TRAJ);
//    int16_t T_chamber = REG(MREG_T_CHAMBER);
//
//    // Compute error
//    int16_t error = T_traj - T_chamber;
//
//    // Read gains (user-tunable via Modbus)
//    int16_t Kp = REG(MREG_PID_KP);
//    int16_t Ki = REG(MREG_PID_KI);     // per second
//    int16_t Kd = REG(MREG_PID_KD);
//
//    // Integral clamp limit
//    int16_t Imax = REG(MREG_PID_I_CLAMP);
//    if (Imax < 0)
//        Imax = 0;
//
//    // -------------------------------------------------------
//    // Integral term
//    // -------------------------------------------------------
//    integral += (int32_t)error * Ki;
//
//    // Clamp integral
//    if (integral > Imax)
//        integral = Imax;
//    else if (integral < -Imax)
//        integral = -Imax;
//
//    // -------------------------------------------------------
//    // Derivative term
//    // -------------------------------------------------------
//    int16_t derivative = error - prev_error;
//    prev_error = error;
//
//    // -------------------------------------------------------
//    // PID sum
//    // (scaled integer math)
//    // -------------------------------------------------------
//    int32_t output =
//          (int32_t)error      * Kp
//        + (int32_t)integral
//        + (int32_t)derivative * Kd;
//
//    // -------------------------------------------------------
//    // Output clamp
//    // Q_CMD recommended range: -1000?+1000
//    // -------------------------------------------------------
//    if (output > 1000)
//        output = 1000;
//    else if (output < -1000)
//        output = -1000;
//
//    REG(MREG_Q_CMD) = (int16_t)output;
//}
//
