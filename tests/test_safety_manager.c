#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../Controller/safety_manager.h"
#include "../system_registers.h"

int main(void)
{
    REG(MREG_FAULT_FLAGS) = 0;
    REG(MREG_SYS_STATUS) = SYS_STATUS_CONTRL_CARD_OK;
    REG(MREG_T_MIN_CHAMBER) = -300;
    REG(MREG_T_MAX_CHAMBER) = 600;
    REG(MREG_T_CHAMBER) = 200;
    SafetyManager_Init();

    SafetyManager_Step_1s();
    assert(REG(MREG_SAFETY_ACTIVE) == 0);
    assert((REG(MREG_FAULT_FLAGS) & FAULT_OVER_UNDER_TEMP) == 0);

    REG(MREG_ACTUATOR_STATES) = ACT_AC_COMPRESSOR | ACT_AC_DEFROST |
                                ACT_DC_COMPRESSOR | ACT_HEAT_FANS |
                                ACT_COOL_FANS | ACT_PWM_HEAT_ELEMENT |
                                ACT_PWM_COOL_EEV;
    REG(MREG_T_CHAMBER) = 601;
    SafetyManager_Step_1s();
    assert(REG(MREG_SAFETY_ACTIVE) == 1);
    assert(REG(MREG_SAFETY_LATCHED) == 1);
    assert(REG(MREG_SAFETY_CODE) == 3);
    assert((REG(MREG_FAULT_FLAGS) & FAULT_OVER_UNDER_TEMP) != 0);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_FAULT_ACTIVE) != 0);
    assert((REG(MREG_ACTUATOR_STATES) & ACT_DC_RED_PILOT) != 0);
    assert((REG(MREG_ACTUATOR_STATES) & ACT_AC_COMPRESSOR) == 0);
    assert((REG(MREG_ACTUATOR_STATES) & ACT_DC_COMPRESSOR) == 0);
    assert((REG(MREG_ACTUATOR_STATES) & ACT_PWM_HEAT_ELEMENT) == 0);

    REG(MREG_T_CHAMBER) = 200;
    SafetyManager_Step_1s();
    assert((REG(MREG_FAULT_FLAGS) & FAULT_OVER_UNDER_TEMP) == 0);
    assert(REG(MREG_SAFETY_ACTIVE) == 1);

    REG(MREG_SAFETY_LATCHED) = 0;
    SafetyManager_Step_1s();
    assert(REG(MREG_SAFETY_ACTIVE) == 0);
    assert(REG(MREG_SAFETY_CODE) == 0);
    assert((REG(MREG_SYS_STATUS) & SYS_STATUS_FAULT_ACTIVE) == 0);

    puts("safety_manager tests passed");
    return 0;
}
