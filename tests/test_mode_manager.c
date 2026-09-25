#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../Controller/mode_manager.h"
#include "../system_registers.h"

static void set_auto_conditions(int16_t chamber_x10, int16_t target_x10,
                                int16_t evap_box_x10)
{
    REG(MREG_SYSTEM_MODE) = 0;
    REG(MREG_SAFETY_ACTIVE) = 0;
    REG(MREG_T_CHAMBER) = chamber_x10;
    REG(MREG_T_TRAJ) = target_x10;
    REG(MREG_T_EVAP_BOX) = evap_box_x10;
}

int main(void)
{
    REG(MREG_SYS_STATUS) = SYS_STATUS_CONTRL_CARD_OK;
    ModeManager_Init();

    set_auto_conditions(0, 100, 0);
    ModeManager_Step_1s();
    assert(REG(MREG_SYS_STATE) == 1);
    assert(REG(MREG_HEATING_ACTIVE) == 1);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_HEATING) != 0);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_IDLE) == 0);

    set_auto_conditions(100, 100, 0);
    ModeManager_Step_1s();
    assert(REG(MREG_SYS_STATE) == 0);
    assert(REG(MREG_HEATING_ACTIVE) == 0);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_IDLE) != 0);

    set_auto_conditions(101, 100, 0);
    ModeManager_Step_1s();
    assert(REG(MREG_SYS_STATE) == 2);
    assert(REG(MREG_COOLING_ACTIVE) == 1);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_COOLING) != 0);

    REG(MREG_SYSTEM_MODE) = 1;
    REG(MREG_HEATING_ACTIVE) = 1;
    REG(MREG_COOLING_ACTIVE) = 1;
    REG(MREG_FREE_COOL_ACTIVE) = 1;
    REG(MREG_FREE_HEATING_ACTIVE) = 1;
    ModeManager_Step_1s();
    assert(REG(MREG_SYS_STATE) == 5);
    assert(REG(MREG_HEATING_ACTIVE) == 0);
    assert(REG(MREG_COOLING_ACTIVE) == 0);
    assert(REG(MREG_FREE_COOL_ACTIVE) == 0);
    assert(REG(MREG_FREE_HEATING_ACTIVE) == 0);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_MANUAL_MODE) != 0);

    puts("mode_manager tests passed");
    return 0;
}
