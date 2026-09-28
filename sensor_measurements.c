#include "sensor_measurements.h"

#include <stdbool.h>
#include <stdint.h>

#include "application_registers.h"

#define SENSOR_STALE_TIMEOUT_MS 12000UL

static uint32_t last_valid_frame_ms;
static bool received_valid_frame;

static bool value_is_valid(uint16_t value)
{
    return value != APP_TEMPERATURE_INVALID;
}

static bool sensor_enabled(uint16_t enable_mask, uint16_t sensor)
{
    return (enable_mask & sensor) != 0U;
}

static void set_fault(uint16_t mask, bool active)
{
    uint16_t faults = ApplicationRegisters_Get(APP_REG_FAULT_FLAGS);
    if (active) {
        faults |= mask;
    } else {
        faults &= (uint16_t)~mask;
    }
    ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS, faults);
}

static void invalidate_measurements(void)
{
    ApplicationRegisters_SetInternal(APP_REG_CHAMBER_TEMPERATURE,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_TEMPERATURE,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_EXTERNAL_TEMPERATURE,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_BOX_TEMPERATURE,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_IN_TEMPERATURE,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_EVAP_OUT_TEMPERATURE,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_SUPERHEAT,
                                     APP_TEMPERATURE_INVALID);
    ApplicationRegisters_SetInternal(APP_REG_SUBCOOLING,
                                     APP_TEMPERATURE_INVALID);
}

void SensorMeasurements_Initialize(void)
{
    last_valid_frame_ms = 0U;
    received_valid_frame = false;
    invalidate_measurements();
    set_fault(APP_FAULT_SENSOR_COMMUNICATION, true);
}

void SensorMeasurements_OnValidFrame(uint32_t now_ms)
{
    uint16_t enable_mask = ApplicationRegisters_Get(APP_SENSOR_ENABLE);
    uint16_t ntc_errors = ApplicationRegisters_Get(APP_SENSOR_NTC_ERROR_BITS);
    uint16_t ktype_error = ApplicationRegisters_Get(APP_SENSOR_KTYPE_ERROR);
    uint16_t ds1_error = ApplicationRegisters_Get(APP_SENSOR_DS18B20_1_ERROR);
    uint16_t ds2_error = ApplicationRegisters_Get(APP_SENSOR_DS18B20_2_ERROR);
    uint16_t chamber = ApplicationRegisters_Get(APP_SENSOR_CHAMBER_INTERNAL);
    uint16_t heater = ApplicationRegisters_Get(APP_SENSOR_HEATING_ELEMENT);
    uint16_t evap_box = ApplicationRegisters_Get(APP_SENSOR_EVAPORATOR);
    uint16_t external = ApplicationRegisters_Get(APP_SENSOR_EXTERNAL);

    bool chamber_enabled =
        (enable_mask & (APP_ENABLE_NTC1 | APP_ENABLE_NTC2 |
                        APP_ENABLE_NTC3 | APP_ENABLE_NTC4)) ==
        (APP_ENABLE_NTC1 | APP_ENABLE_NTC2 | APP_ENABLE_NTC3 | APP_ENABLE_NTC4);
    bool chamber_valid = chamber_enabled && ((ntc_errors & 0x000FU) == 0U) &&
                         value_is_valid(chamber);
    ApplicationRegisters_SetInternal(
        APP_REG_CHAMBER_TEMPERATURE,
        chamber_valid ? chamber : APP_TEMPERATURE_INVALID);

    bool heater_valid = sensor_enabled(enable_mask, APP_ENABLE_KTYPE) &&
                        (ktype_error == 0U) && value_is_valid(heater);
    ApplicationRegisters_SetInternal(
        APP_REG_HEATER_TEMPERATURE,
        heater_valid ? heater : APP_TEMPERATURE_INVALID);

    bool external_valid = sensor_enabled(enable_mask, APP_ENABLE_DS18B20_2) &&
                          (ds2_error == 0U) && value_is_valid(external);
    ApplicationRegisters_SetInternal(
        APP_REG_EXTERNAL_TEMPERATURE,
        external_valid ? external : APP_TEMPERATURE_INVALID);

    bool evap_box_valid = sensor_enabled(enable_mask, APP_ENABLE_DS18B20_1) &&
                          (ds1_error == 0U) && value_is_valid(evap_box);
    ApplicationRegisters_SetInternal(
        APP_REG_EVAP_BOX_TEMPERATURE,
        evap_box_valid ? evap_box : APP_TEMPERATURE_INVALID);

    uint16_t evap_in = ApplicationRegisters_Get(APP_SENSOR_NTC5_TEMP);
    bool evap_in_valid = sensor_enabled(enable_mask, APP_ENABLE_NTC5) &&
                         ((ntc_errors & (1U << 4)) == 0U) &&
                         value_is_valid(evap_in);
    ApplicationRegisters_SetInternal(
        APP_REG_EVAP_IN_TEMPERATURE,
        evap_in_valid ? evap_in : APP_TEMPERATURE_INVALID);

    uint16_t evap_out = ApplicationRegisters_Get(APP_SENSOR_NTC6_TEMP);
    bool evap_out_valid = sensor_enabled(enable_mask, APP_ENABLE_NTC6) &&
                          ((ntc_errors & (1U << 5)) == 0U) &&
                          value_is_valid(evap_out);
    ApplicationRegisters_SetInternal(
        APP_REG_EVAP_OUT_TEMPERATURE,
        evap_out_valid ? evap_out : APP_TEMPERATURE_INVALID);

    /* These legacy registers contain filtered line-temperature differences,
     * not pressure-derived refrigerant superheat or subcooling. */
    uint16_t evaporator_delta =
        ApplicationRegisters_Get(APP_SENSOR_SUPERHEAT);
    bool evaporator_delta_valid = evap_in_valid && evap_out_valid &&
                                  value_is_valid(evaporator_delta);
    ApplicationRegisters_SetInternal(
        APP_REG_SUPERHEAT,
        evaporator_delta_valid ? evaporator_delta : APP_TEMPERATURE_INVALID);

    uint16_t condenser_delta =
        ApplicationRegisters_Get(APP_SENSOR_SUBCOOLING);
    bool condenser_delta_valid =
        sensor_enabled(enable_mask, APP_ENABLE_NTC7) &&
        sensor_enabled(enable_mask, APP_ENABLE_NTC8) &&
        ((ntc_errors & ((1U << 6) | (1U << 7))) == 0U) &&
        value_is_valid(condenser_delta);
    ApplicationRegisters_SetInternal(
        APP_REG_SUBCOOLING,
        condenser_delta_valid ? condenser_delta : APP_TEMPERATURE_INVALID);

    set_fault(APP_FAULT_NTC, ntc_errors != 0U);
    set_fault(APP_FAULT_KTYPE,
              sensor_enabled(enable_mask, APP_ENABLE_KTYPE) &&
              (ktype_error != 0U));
    set_fault(APP_FAULT_SENSOR_COMMUNICATION, false);

    last_valid_frame_ms = now_ms;
    received_valid_frame = true;
}

void SensorMeasurements_Process(uint32_t now_ms)
{
    if (!received_valid_frame ||
        ((uint32_t)(now_ms - last_valid_frame_ms) >= SENSOR_STALE_TIMEOUT_MS)) {
        invalidate_measurements();
        set_fault(APP_FAULT_SENSOR_COMMUNICATION, true);
        ApplicationRegisters_SetInternal(
            APP_REG_SYSTEM_STATUS,
            (uint16_t)(ApplicationRegisters_Get(APP_REG_SYSTEM_STATUS) &
                       (uint16_t)~APP_STATUS_SENSOR_CARD_OK));
    }
}
