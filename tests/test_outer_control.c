#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../Controller/outer_control.h"
#include "../system_registers.h"

static void configure_pid(uint16_t kp_x100, uint16_t ki_x100,
                          uint16_t kd_x100, uint16_t i_clamp)
{
    REG(MREG_PID_KP) = kp_x100;
    REG(MREG_PID_KI) = ki_x100;
    REG(MREG_PID_KD) = kd_x100;
    REG(MREG_PID_I_CLAMP) = i_clamp;
    REG(MREG_SYSTEM_MODE) = 0;
    REG(MREG_SAFETY_ACTIVE) = 0;
    OuterControl_Init();
}

int main(void)
{
    configure_pid(100U, 0U, 0U, 0U);
    REG(MREG_T_TRAJ) = 100;
    REG(MREG_T_CHAMBER) = 0;
    OuterControl_Step_1s();
    assert((int16_t)REG(MREG_Q_CMD) == 100);

    REG(MREG_T_CHAMBER) = 200;
    OuterControl_Step_1s();
    assert((int16_t)REG(MREG_Q_CMD) == -100);

    configure_pid(10000U, 0U, 0U, 0U);
    REG(MREG_T_TRAJ) = INT16_MAX;
    REG(MREG_T_CHAMBER) = INT16_MIN;
    OuterControl_Step_1s();
    assert((int16_t)REG(MREG_Q_CMD) == 1000);

    configure_pid(0U, 100U, 0U, 25U);
    REG(MREG_T_TRAJ) = 10;
    REG(MREG_T_CHAMBER) = 0;
    OuterControl_Step_1s();
    OuterControl_Step_1s();
    OuterControl_Step_1s();
    assert((int16_t)REG(MREG_PID_INTEGRAL) == 25);

    REG(MREG_SYSTEM_MODE) = 1;
    OuterControl_Step_1s();
    assert((int16_t)REG(MREG_Q_CMD) == 0);
    assert((int16_t)REG(MREG_PID_INTEGRAL) == 0);

    REG(MREG_SYSTEM_MODE) = 0;
    REG(MREG_SAFETY_ACTIVE) = 1;
    REG(MREG_Q_CMD) = 500;
    OuterControl_Step_1s();
    assert((int16_t)REG(MREG_Q_CMD) == 0);

    puts("outer_control tests passed");
    return 0;
}
