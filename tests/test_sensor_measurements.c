#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "application_registers.h"
#include "sensor_measurements.h"

int main(void)
{
    ApplicationRegisters_Initialize();
    SensorMeasurements_Initialize();

    assert((ApplicationRegisters_Get(APP_REG_FAULT_FLAGS) &
            APP_FAULT_SENSOR_COMMUNICATION) != 0U);

    ApplicationRegisters_SetInternal(
        APP_SENSOR_ENABLE,
        APP_ENABLE_NTC1 | APP_ENABLE_NTC2 | APP_ENABLE_NTC3 |
        APP_ENABLE_NTC4 | APP_ENABLE_NTC5 | APP_ENABLE_NTC6 |
        APP_ENABLE_NTC7 | APP_ENABLE_NTC8 | APP_ENABLE_KTYPE |
        APP_ENABLE_DS18B20_1 | APP_ENABLE_DS18B20_2);
    ApplicationRegisters_SetInternal(APP_SENSOR_CHAMBER_INTERNAL, 251U);
    ApplicationRegisters_SetInternal(APP_SENSOR_HEATING_ELEMENT, 503U);
    ApplicationRegisters_SetInternal(APP_SENSOR_EXTERNAL, 250U);
    ApplicationRegisters_SetInternal(APP_SENSOR_EVAPORATOR, 242U);
    ApplicationRegisters_SetInternal(APP_SENSOR_NTC5_TEMP,
                                     (uint16_t)(int16_t)-50);
    ApplicationRegisters_SetInternal(APP_SENSOR_NTC6_TEMP,
                                     (uint16_t)(int16_t)-30);
    ApplicationRegisters_SetInternal(APP_SENSOR_NTC7_TEMP, 360U);
    ApplicationRegisters_SetInternal(APP_SENSOR_NTC8_TEMP, 280U);
    ApplicationRegisters_SetInternal(APP_SENSOR_SUPERHEAT, 20U);
    ApplicationRegisters_SetInternal(APP_SENSOR_SUBCOOLING, 80U);
    ApplicationRegisters_SetInternal(APP_SENSOR_NTC_ERROR_BITS, 0U);
    ApplicationRegisters_SetInternal(APP_SENSOR_KTYPE_ERROR, 0U);
    ApplicationRegisters_SetInternal(APP_SENSOR_DS18B20_1_ERROR, 0U);
    ApplicationRegisters_SetInternal(APP_SENSOR_DS18B20_2_ERROR, 0U);

    SensorMeasurements_OnValidFrame(100U);
    assert(ApplicationRegisters_Get(APP_REG_CHAMBER_TEMPERATURE) == 251U);
    assert(ApplicationRegisters_Get(APP_REG_HEATER_TEMPERATURE) == 503U);
    assert(ApplicationRegisters_Get(APP_REG_EXTERNAL_TEMPERATURE) == 250U);
    assert(ApplicationRegisters_Get(APP_REG_EVAP_BOX_TEMPERATURE) == 242U);
    assert((int16_t)ApplicationRegisters_Get(APP_REG_EVAP_IN_TEMPERATURE) == -50);
    assert((int16_t)ApplicationRegisters_Get(APP_REG_EVAP_OUT_TEMPERATURE) == -30);
    assert(ApplicationRegisters_Get(APP_REG_SUPERHEAT) == 20U);
    assert(ApplicationRegisters_Get(APP_REG_SUBCOOLING) == 80U);
    assert((ApplicationRegisters_Get(APP_REG_FAULT_FLAGS) &
            APP_FAULT_SENSOR_COMMUNICATION) == 0U);

    ApplicationRegisters_SetInternal(APP_SENSOR_NTC_ERROR_BITS, 1U);
    SensorMeasurements_OnValidFrame(200U);
    assert(ApplicationRegisters_Get(APP_REG_CHAMBER_TEMPERATURE) ==
           APP_TEMPERATURE_INVALID);
    assert((ApplicationRegisters_Get(APP_REG_FAULT_FLAGS) & APP_FAULT_NTC) != 0U);

    ApplicationRegisters_SetInternal(APP_SENSOR_NTC_ERROR_BITS, (1U << 4));
    SensorMeasurements_OnValidFrame(300U);
    assert(ApplicationRegisters_Get(APP_REG_SUPERHEAT) ==
           APP_TEMPERATURE_INVALID);
    assert(ApplicationRegisters_Get(APP_REG_SUBCOOLING) == 80U);

    SensorMeasurements_Process(3300U);
    assert(ApplicationRegisters_Get(APP_REG_EXTERNAL_TEMPERATURE) ==
           APP_TEMPERATURE_INVALID);
    assert((ApplicationRegisters_Get(APP_REG_FAULT_FLAGS) &
            APP_FAULT_SENSOR_COMMUNICATION) != 0U);

    puts("sensor measurement tests passed");
    return 0;
}
