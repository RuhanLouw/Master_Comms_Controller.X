
#include "trajectory_generator.h"
#include "../system_registers.h"

/*
 * setpoint_traj.c
 *
 * Generates a smoothed / ramp-limited target temperature T_TRAJ
 * from the main setpoint SP_MAIN and the configured RAMP_RATE.
 *
 * Registers used:
 *   In : MREG_T_SP_MAIN   (°C×10)      - main user setpoint
 *        MREG_RAMP_RATE   (°C/min×100) - ramp rate, e.g. 0.5°C/min -> 50
 *   Out: MREG_T_TRAJ      (°C×10)     - smoothed target used by outer_control
 */

#include <stdint.h>

/* One 0.1°C trajectory step requires 600 ramp-rate accumulator counts. */
#define RAMP_COUNTS_PER_STEP_X10 600U

static uint32_t ramp_accumulator = 0U;
static int8_t ramp_direction = 0;

void SetpointTraj_Init(void)
{
    /* On startup: just start trajectory at the current setpoint.
     * (You can also use the actual chamber temp if you prefer.)
     */
    int16_t sp = (int16_t)REG(MREG_T_SP_MAIN);
    REG(MREG_T_TRAJ) = (uint16_t)sp;
    ramp_accumulator = 0U;
    ramp_direction = 0;
}

void SetpointTraj_Step_1s(void)
{
    int16_t sp = (int16_t)REG(MREG_T_SP_MAIN);       // °C×10
    int16_t traj = (int16_t)REG(MREG_T_TRAJ);        // °C×10
    int16_t ramp_x100 = (int16_t)REG(MREG_RAMP_RATE); // (°C/min)×100

    /* If ramp rate is zero or negative: T_TRAJ follows SP_MAIN instantly */
    if (ramp_x100 <= 0) {
        REG(MREG_T_TRAJ) = (uint16_t)sp;
        ramp_accumulator = 0U;
        ramp_direction = 0;
        return;
    }

    int32_t error = (int32_t)sp - (int32_t)traj;
    if (error == 0) {
        ramp_accumulator = 0U;
        ramp_direction = 0;
        return;
    }

    int8_t direction = (error > 0) ? 1 : -1;
    if (direction != ramp_direction) {
        ramp_accumulator = 0U;
        ramp_direction = direction;
    }

    /* Preserve fractional progress so rates below 6°C/min remain accurate. */
    ramp_accumulator += (uint16_t)ramp_x100;
    uint16_t step_x10 = (uint16_t)(ramp_accumulator / RAMP_COUNTS_PER_STEP_X10);
    ramp_accumulator %= RAMP_COUNTS_PER_STEP_X10;

    if (step_x10 == 0U) {
        return;
    }

    uint32_t distance_x10 = (uint32_t)((error > 0) ? error : -error);
    if (distance_x10 <= step_x10) {
        traj = sp;
        ramp_accumulator = 0U;
        ramp_direction = 0;
    } else {
        traj = (int16_t)((int32_t)traj + ((int32_t)direction * step_x10));
    }

    REG(MREG_T_TRAJ) = (uint16_t)traj;
}


//// ---------------------------------------------------------------------------
//// Trajectory Generator
//// Smoothly moves T_TRAJ toward SETPOINT according to RAMP_RATE.
//// All temps use 0.1°C units. RAMP_RATE in 0.1°C/min.
//// Called once per second.
//// ---------------------------------------------------------------------------
//
//void trajectory_generator_update(void)
//{
//    int16_t T_set  = REG(MREG_SP_MAIN);    // user request
//    int16_t T_traj = REG(MREG_T_TRAJ);      // smoothed setpoint
//    int16_t rate   = REG(MREG_RAMP_RATE);   // ramp in 0.1°C/min
//
//    if (rate < 1)
//        rate = 1;
//
//    // convert 0.1°C/min ? 0.1°C/s allowed change
//    int16_t delta_max = (rate * 10) / 60;
//    if (delta_max < 1)
//        delta_max = 1;
//
//    // move toward the target with limited speed
//    if (T_set > T_traj)
//    {
//        T_traj += delta_max;
//        if (T_traj > T_set)
//            T_traj = T_set;
//    }
//    else if (T_set < T_traj)
//    {
//        T_traj -= delta_max;
//        if (T_traj < T_set)
//            T_traj = T_set;
//    }
//
//    REG(MREG_T_TRAJ) = T_traj;
//}

