#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "application_registers.h"

int main(void)
{
    ApplicationRegisters_Initialize();

    assert(ApplicationRegisters_Get(APP_REG_FW_VERSION) == 206U);
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_MODE) == 1U);
    assert((int16_t)ApplicationRegisters_Get(APP_REG_MIN_TEMPERATURE) == -300);
    assert(ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) == 0U);

    uint16_t value = 300U;
    assert(ApplicationRegisters_Write(APP_REG_MAIN_SETPOINT, &value, 1U) ==
           APP_WRITE_OK);
    assert(ApplicationRegisters_Get(APP_REG_MAIN_SETPOINT) == 300U);

    value = 700U;
    assert(ApplicationRegisters_Write(APP_REG_MAIN_SETPOINT, &value, 1U) ==
           APP_WRITE_ILLEGAL_VALUE);
    assert(ApplicationRegisters_Get(APP_REG_MAIN_SETPOINT) == 300U);

    value = 123U;
    assert(ApplicationRegisters_Write(APP_REG_FW_VERSION, &value, 1U) ==
           APP_WRITE_ILLEGAL_ADDRESS);
    assert(ApplicationRegisters_Get(APP_REG_FW_VERSION) == 206U);

    uint16_t multi[2] = {20U, 1U};
    assert(ApplicationRegisters_Write(APP_REG_EEV_MAX_STEP, multi, 2U) ==
           APP_WRITE_ILLEGAL_ADDRESS);
    assert(ApplicationRegisters_Get(APP_REG_EEV_MAX_STEP) == 10U);

    assert(ApplicationRegisters_WriteCoil(112U, true) == APP_WRITE_OK);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) & 1U) != 0U);
    assert(ApplicationRegisters_WriteCoil(112U, false) == APP_WRITE_OK);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) & 1U) == 0U);

    value = 0U;
    assert(ApplicationRegisters_Write(APP_REG_SYSTEM_MODE, &value, 1U) ==
           APP_WRITE_OK);
    assert(ApplicationRegisters_WriteCoil(112U, true) ==
           APP_WRITE_ILLEGAL_VALUE);
    assert(ApplicationRegisters_WriteCoil(111U, true) ==
           APP_WRITE_ILLEGAL_ADDRESS);

    ApplicationRegisters_Tick1s();
    ApplicationRegisters_Tick1s();
    assert(ApplicationRegisters_Get(APP_REG_UPTIME_LOW) == 2U);
    assert(ApplicationRegisters_Get(APP_REG_UPTIME_HIGH) == 0U);

    puts("application register tests passed");
    return 0;
}
