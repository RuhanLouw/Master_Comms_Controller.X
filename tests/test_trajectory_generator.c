#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../Controller/trajectory_generator.h"
#include "../system_registers.h"

static void reset_trajectory(int16_t start_x10, int16_t target_x10,
                             uint16_t ramp_x100)
{
    REG(MREG_T_SP_MAIN) = start_x10;
    SetpointTraj_Init();
    REG(MREG_T_SP_MAIN) = target_x10;
    REG(MREG_RAMP_RATE) = ramp_x100;
}

static void step_seconds(uint16_t seconds)
{
    for (uint16_t i = 0U; i < seconds; i++) {
        SetpointTraj_Step_1s();
    }
}

int main(void)
{
    reset_trajectory(0, 100, 0U);
    SetpointTraj_Step_1s();
    assert(REG(MREG_T_TRAJ) == 100);

    reset_trajectory(0, 100, 50U);
    step_seconds(11U);
    assert(REG(MREG_T_TRAJ) == 0);
    step_seconds(49U);
    assert(REG(MREG_T_TRAJ) == 5);

    reset_trajectory(0, 300, 1500U);
    step_seconds(60U);
    assert(REG(MREG_T_TRAJ) == 150);

    reset_trajectory(0, -100, 50U);
    step_seconds(60U);
    assert(REG(MREG_T_TRAJ) == -5);

    reset_trajectory(0, 3, 1500U);
    step_seconds(2U);
    assert(REG(MREG_T_TRAJ) == 3);

    reset_trajectory(0, 100, 50U);
    step_seconds(11U);
    REG(MREG_T_SP_MAIN) = -100;
    SetpointTraj_Step_1s();
    assert(REG(MREG_T_TRAJ) == 0);

    puts("trajectory_generator tests passed");
    return 0;
}
