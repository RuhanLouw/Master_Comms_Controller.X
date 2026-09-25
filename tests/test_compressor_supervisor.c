#include <assert.h>
#include <stdio.h>

#include "../Controller/compressor_supervisor.h"
#include "../system_registers.h"

#define MAN_BYPASS_COMP_TIMERS (1U << 0)

static void set_auto_request(uint16_t min_on_s, uint16_t min_off_s)
{
    REG(MREG_SYSTEM_MODE) = 0U;
    REG(MREG_SAFETY_ACTIVE) = 0U;
    REG(MREG_FAULT_FLAGS) = 0U;
    REG(MREG_COOLING_ACTIVE) = 1U;
    REG(MREG_COMPRESSOR_ALLOWED) = 1U;
    REG(MREG_COMP_MIN_ON_TIME) = min_on_s;
    REG(MREG_COMP_MIN_OFF_TIME) = min_off_s;
    REG(MREG_MANUAL_BYPASS_FLAGS) = 0U;
}

static void assert_compressor(bool expected_on)
{
    uint16_t actuators = REG(MREG_ACTUATOR_STATES);

    assert((REG(MREG_COMPRESSOR_STATE) != 0) == expected_on);
    assert(((actuators & ACT_AC_COMPRESSOR) != 0U) == expected_on);
    assert(((actuators & ACT_AC_CONDENSER_FAN) != 0U) == expected_on);
}

int main(void)
{
    CompressorSupervisor_Init();
    set_auto_request(3U, 2U);

    CompressorSupervisor_Step_1s();
    assert_compressor(false);
    CompressorSupervisor_Step_1s();
    assert_compressor(true);

    REG(MREG_COOLING_ACTIVE) = 0U;
    CompressorSupervisor_Step_1s();
    assert_compressor(true);
    CompressorSupervisor_Step_1s();
    assert_compressor(true);
    CompressorSupervisor_Step_1s();
    assert_compressor(false);

    CompressorSupervisor_Init();
    set_auto_request(100U, 0U);
    CompressorSupervisor_Step_1s();
    assert_compressor(true);
    REG(MREG_SAFETY_ACTIVE) = 1U;
    CompressorSupervisor_Step_1s();
    assert_compressor(false);

    CompressorSupervisor_Init();
    set_auto_request(0U, 5U);
    REG(MREG_MANUAL_BYPASS_FLAGS) = MAN_BYPASS_COMP_TIMERS;
    CompressorSupervisor_Step_1s();
    assert_compressor(false);

    CompressorSupervisor_Init();
    REG(MREG_SYSTEM_MODE) = 1U;
    REG(MREG_SAFETY_ACTIVE) = 0U;
    REG(MREG_FAULT_FLAGS) = 0U;
    REG(MREG_COMP_MIN_ON_TIME) = 100U;
    REG(MREG_COMP_MIN_OFF_TIME) = 100U;
    REG(MREG_MANUAL_BYPASS_FLAGS) = MAN_BYPASS_COMP_TIMERS;
    REG(MREG_MANUAL_ACTUATOR_STATES) = ACT_AC_COMPRESSOR;
    CompressorSupervisor_Step_1s();
    assert_compressor(true);

    REG(MREG_SAFETY_ACTIVE) = 1U;
    CompressorSupervisor_Step_1s();
    assert_compressor(false);

    puts("compressor_supervisor tests passed");
    return 0;
}
