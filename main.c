#define F_CPU 16000000UL

#include <avr/interrupt.h>
#include <stdint.h>

#include "application_registers.h"
#include "communications.h"
#include "control_application.h"
#include "definitions.h"
#include "hardware_outputs.h"
#include "mcc_generated_files/system/clock.h"
#include "mcc_generated_files/system/pins.h"
#include "sensor_measurements.h"

int main(void)
{
    CLOCK_Initialize();
    PIN_MANAGER_Initialize();
    Communications_Initialize();
    ControlApplication_Initialize();
    HardwareOutputs_Initialize();
    sei();

    uint32_t last_run_toggle_ms = 0U;

    while (1) {
        Communications_Process();

        uint32_t now = Communications_Millis();
        SensorMeasurements_Process(now);
        HardwareOutputs_Process(now);
        if ((uint32_t)(now - last_run_toggle_ms) >= 1000U) {
            last_run_toggle_ms += 1000U;
            ApplicationRegisters_Tick1s();
            ControlApplication_Step1s();
            LED_RUN_TOGGLE();
        }
    }
}
