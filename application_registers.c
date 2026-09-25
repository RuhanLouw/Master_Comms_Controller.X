#include "application_registers.h"

#include <limits.h>
#include <string.h>

#define MASTER_FIRMWARE_VERSION 206U

static uint16_t registers[APP_REGISTER_COUNT];
static uint32_t uptime_seconds;

static app_write_result_t validate_write(uint16_t address, uint16_t value)
{
    switch (address) {
        case APP_REG_DEFROST_SETPOINT:
        case APP_REG_MAX_TEMPERATURE:
        case APP_REG_MIN_TEMPERATURE:
        case APP_REG_EVAP_MIN_TEMPERATURE:
        case APP_REG_EEV_KI_X10:
        case APP_REG_COMP_MIN_ON_TIME:
        case APP_REG_COMP_MIN_OFF_TIME:
        case APP_REG_HEATER_MAX_TEMPERATURE:
        case APP_REG_PID_SAMPLE_TIME_MS:
        case APP_REG_DEFROST_INTERVAL:
            return APP_WRITE_OK;

        case APP_REG_MAIN_SETPOINT:
        {
            int16_t setpoint = (int16_t)value;
            int16_t minimum = (int16_t)registers[APP_REG_MIN_TEMPERATURE];
            int16_t maximum = (int16_t)registers[APP_REG_MAX_TEMPERATURE];
            return ((maximum > minimum) &&
                    ((setpoint < minimum) || (setpoint > maximum)))
                       ? APP_WRITE_ILLEGAL_VALUE
                       : APP_WRITE_OK;
        }

        case APP_REG_SUPERHEAT_TARGET:
            return (value <= 600U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_TEMP_HYSTERESIS:
        case APP_REG_EEV_KP_X10:
        case APP_REG_EEV_KI_X100:
        case APP_REG_PID_KP:
        case APP_REG_PID_KI:
        case APP_REG_PID_KD:
        case APP_REG_RAMP_RATE:
        case APP_REG_FREECOOL_MIN_DT_X10:
            return (value <= INT16_MAX) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_PID_INTEGRAL_CLAMP:
            return (value <= 1000U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_SYSTEM_MODE:
            return (value <= 1U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_SAFETY_LATCHED:
            return (value == 0U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_MANUAL_EEV_PWM:
        case APP_REG_EEV_MIN_PWM:
        case APP_REG_EEV_MAX_PWM:
            return (value <= 100U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_MANUAL_HEATER_PWM:
            return (value <= 1000U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_HEATER_FAN_STATE:
        case APP_REG_COOL_FAN_STATE:
        case APP_REG_MANUAL_HEAT_FANS:
        case APP_REG_MANUAL_COOL_FANS:
        case APP_REG_MANUAL_BYPASS_FLAGS:
            return (value <= 1U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_OPERATING_MODE:
            return (value <= 2U) ? APP_WRITE_OK : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_SUPERHEAT_FILTER_DIV:
            return ((value >= 1U) && (value <= 50U))
                       ? APP_WRITE_OK
                       : APP_WRITE_ILLEGAL_VALUE;

        case APP_REG_EEV_MAX_STEP:
            return ((value >= 1U) && (value <= 100U))
                       ? APP_WRITE_OK
                       : APP_WRITE_ILLEGAL_VALUE;

        default:
            return APP_WRITE_ILLEGAL_ADDRESS;
    }
}

void ApplicationRegisters_Initialize(void)
{
    memset(registers, 0, sizeof(registers));
    uptime_seconds = 0U;

    registers[APP_REG_FW_VERSION] = MASTER_FIRMWARE_VERSION;
    registers[APP_REG_SYSTEM_STATUS] = APP_STATUS_IDLE |
                                        APP_STATUS_MANUAL_MODE |
                                        APP_STATUS_CONTROLLER_OK;
    registers[APP_REG_MAIN_SETPOINT] = 250U;
    registers[APP_REG_EXTERNAL_TEMPERATURE] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_TEMP_HYSTERESIS] = 5U;
    registers[APP_REG_CHAMBER_TEMPERATURE] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_HEATER_TEMPERATURE] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_EVAP_BOX_TEMPERATURE] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_SUPERHEAT] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_SUBCOOLING] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_EVAP_IN_TEMPERATURE] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_EVAP_OUT_TEMPERATURE] = APP_TEMPERATURE_INVALID;
    registers[APP_REG_MAX_TEMPERATURE] = 600U;
    registers[APP_REG_MIN_TEMPERATURE] = (uint16_t)(int16_t)-300;
    registers[APP_REG_EVAP_MIN_TEMPERATURE] = (uint16_t)(int16_t)-300;
    registers[APP_REG_SUPERHEAT_TARGET] = 100U;
    registers[APP_REG_SYSTEM_MODE] = 1U;
    registers[APP_REG_SYSTEM_STATE] = APP_STATE_MANUAL;
    registers[APP_REG_COMP_MIN_ON_TIME] = 900U;
    registers[APP_REG_COMP_MIN_OFF_TIME] = 600U;
    registers[APP_REG_EEV_MIN_PWM] = 10U;
    registers[APP_REG_EEV_MAX_PWM] = 30U;
    registers[APP_REG_HEATER_MAX_TEMPERATURE] = 1300U;
    registers[APP_REG_RAMP_RATE] = 1500U;
    registers[APP_REG_FREECOOL_MIN_DT_X10] = 10U;
    registers[APP_REG_SUPERHEAT_FILTER_DIV] = 2U;
    registers[APP_REG_EEV_KP_X10] = 20U;
    registers[APP_REG_EEV_KI_X100] = 50U;
    registers[APP_REG_PID_KP] = 300U;
    registers[APP_REG_PID_KI] = 75U;
    registers[APP_REG_PID_KD] = 0U;
    registers[APP_REG_PID_SAMPLE_TIME_MS] = 1000U;
    registers[APP_REG_PID_INTEGRAL_CLAMP] = 400U;
    registers[APP_REG_EEV_MAX_STEP] = 10U;
}

uint16_t ApplicationRegisters_Get(uint16_t address)
{
    return (address < APP_REGISTER_COUNT) ? registers[address] : 0U;
}

void ApplicationRegisters_SetInternal(uint16_t address, uint16_t value)
{
    if (address < APP_REGISTER_COUNT) {
        registers[address] = value;
    }
}

app_write_result_t ApplicationRegisters_Write(uint16_t start,
                                               const uint16_t *values,
                                               uint16_t count)
{
    if ((values == NULL) || (count == 0U) || (start >= APP_REGISTER_COUNT) ||
        (count > (APP_REGISTER_COUNT - start))) {
        return APP_WRITE_ILLEGAL_ADDRESS;
    }

    for (uint16_t i = 0U; i < count; i++) {
        app_write_result_t result = validate_write((uint16_t)(start + i), values[i]);
        if (result != APP_WRITE_OK) {
            return result;
        }
    }

    for (uint16_t i = 0U; i < count; i++) {
        registers[start + i] = values[i];
    }
    return APP_WRITE_OK;
}

app_write_result_t ApplicationRegisters_WriteCoil(uint16_t coil, bool on)
{
    if ((coil < 112U) || (coil > 123U)) {
        return APP_WRITE_ILLEGAL_ADDRESS;
    }
    if (registers[APP_REG_SYSTEM_MODE] == 0U) {
        return APP_WRITE_ILLEGAL_VALUE;
    }

    uint16_t mask = (uint16_t)(1U << (coil - 112U));
    if (on) {
        registers[APP_REG_ACTUATOR_STATES] |= mask;
    } else {
        registers[APP_REG_ACTUATOR_STATES] &= (uint16_t)~mask;
    }
    return APP_WRITE_OK;
}

void ApplicationRegisters_Tick1s(void)
{
    uptime_seconds++;
    registers[APP_REG_UPTIME_LOW] = (uint16_t)uptime_seconds;
    registers[APP_REG_UPTIME_HIGH] = (uint16_t)(uptime_seconds >> 16);
}
