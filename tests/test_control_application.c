#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "application_registers.h"
#include "control_application.h"

static void initialize_healthy_system(void)
{
    ApplicationRegisters_Initialize();
    ControlApplication_Initialize();
    ApplicationRegisters_SetInternal(
        APP_REG_SYSTEM_STATUS,
        APP_STATUS_IDLE | APP_STATUS_SENSOR_CARD_OK | APP_STATUS_CONTROLLER_OK);
    ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS, 0U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 250U);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 250U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 250U);
    ApplicationRegisters_SetInternal(APP_REG_SUPERHEAT, 100U);
}

static void test_manual_outputs(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_MANUAL_HEATER_PWM, 250U);
    ApplicationRegisters_SetInternal(APP_REG_MANUAL_EEV_PWM, 20U);
    assert(ApplicationRegisters_WriteCoil(122U, true) == APP_WRITE_OK);
    assert(ApplicationRegisters_WriteCoil(123U, true) == APP_WRITE_OK);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 250U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 20U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
           APP_EEV_STATE_MANUAL);
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) == APP_STATE_MANUAL);
}

static void test_automatic_heating(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 300U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 200U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) ==
           APP_STATE_HEATING);
    assert(ApplicationRegisters_Get(APP_REG_HEATING_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) > 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_HEAT_FANS) != 0U);
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
}

static void test_automatic_refrigeration(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 200U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 300U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 295U);
    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_OFF_TIME, 0U);
    ApplicationRegisters_SetInternal(APP_REG_COMP_MIN_ON_TIME, 0U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
           APP_EEV_STATE_STARTUP);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) == 0U);

    for (uint8_t second = 0U; second < 7U; second++) {
        ControlApplication_Step1s();
    }
    assert(ApplicationRegisters_Get(APP_REG_EEV_STATE) ==
           APP_EEV_STATE_CONTROL);
    assert(ApplicationRegisters_Get(APP_REG_EEV_PWM) >=
           ApplicationRegisters_Get(APP_REG_EEV_MIN_PWM));
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_AC_COMPRESSOR) != 0U);
}

static void test_free_cooling(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_MODE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_RAMP_RATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_MAIN_SETPOINT, 200U);
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE, 300U);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE, 250U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SYSTEM_STATE) ==
           APP_STATE_FREE_COOLING);
    assert(ApplicationRegisters_Get(APP_REG_FREE_COOL_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_COMPRESSOR_STATE) == 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_COOL_FANS) != 0U);
}

static void test_latched_heater_safety(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_MANUAL_HEATER_PWM, 500U);
    assert(ApplicationRegisters_WriteCoil(122U, true) == APP_WRITE_OK);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 1400U);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_LATCHED) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) ==
           APP_SAFETY_HEATER);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_PWM) == 0U);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_RED_PILOT) != 0U);

    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE, 250U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    ApplicationRegisters_SetInternal(APP_REG_SAFETY_LATCHED, 0U);
    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 0U);
}

static void test_manual_compressor_evaporator_limit(void)
{
    initialize_healthy_system();
    ApplicationRegisters_SetInternal(APP_REG_EVAP_MIN_TEMPERATURE,
                                     (uint16_t)(int16_t)-100);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE,
                                     (uint16_t)(int16_t)-120);
    assert(ApplicationRegisters_WriteCoil(112U, true) == APP_WRITE_OK);

    ControlApplication_Step1s();
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 1U);
    assert(ApplicationRegisters_Get(APP_REG_SAFETY_CODE) ==
           APP_SAFETY_EVAPORATOR_LIMIT);
    assert((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
            APP_ACT_AC_COMPRESSOR) == 0U);
}

int main(void)
{
    test_manual_outputs();
    test_automatic_heating();
    test_automatic_refrigeration();
    test_free_cooling();
    test_latched_heater_safety();
    test_manual_compressor_evaporator_limit();
    puts("control application tests passed");
    return 0;
}
