#include "hardware_outputs.h"

#include <stdbool.h>
#include <stdint.h>

#include "application_registers.h"
#include "mcc_generated_files/system/pins.h"

#define SHIFT_REGISTER_ALLOWED_MASK 0xF7U
#define OUTPUT_MASK \
    (APP_ACT_AC_COMPRESSOR | APP_ACT_AC_CONDENSER_FAN | \
     APP_ACT_AC_DEFROST | APP_ACT_DC_COMPRESSOR | \
     APP_ACT_GREEN_PILOT | APP_ACT_ORANGE_PILOT | APP_ACT_RED_PILOT | \
     APP_ACT_HEAT_FANS | APP_ACT_COOL_FANS | \
     APP_ACT_HEATER_PWM_ACTIVE | APP_ACT_EEV_PWM_ACTIVE)

#define HEATER_PERIOD_MS 1000UL
#define EEV_PERIOD_MS    6000UL

static uint8_t applied_shift_state;

static void shift_register_write(uint8_t state)
{
    for (int8_t bit = 7; bit >= 0; bit--) {
        if ((state & (uint8_t)(1U << bit)) != 0U) {
            IO_PE3_SetHigh();
        } else {
            IO_PE3_SetLow();
        }
        IO_PF0_SetHigh();
        IO_PF0_SetLow();
    }

    IO_PF1_SetHigh();
    IO_PF1_SetLow();
    applied_shift_state = state;
}

static bool process_outputs_permitted(void)
{
    uint16_t status = ApplicationRegisters_Get(APP_REG_SYSTEM_STATUS);
    uint16_t faults = ApplicationRegisters_Get(APP_REG_FAULT_FLAGS);

    return (ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE) == 0U) &&
           ((status & APP_STATUS_SENSOR_CARD_OK) != 0U) &&
           ((faults & APP_FAULT_SENSOR_COMMUNICATION) == 0U);
}

void HardwareOutputs_Initialize(void)
{
    IO_PE3_SetDigitalOutput();
    IO_PF0_SetDigitalOutput();
    IO_PF1_SetDigitalOutput();
    IO_PF4_SetDigitalOutput();
    IO_PE3_SetLow();
    IO_PF0_SetLow();
    IO_PF1_SetLow();
    IO_PF4_SetLow();
    IO_PF4_SetHigh();

    IO_PD0_SetDigitalOutput();
    IO_PD1_SetDigitalOutput();
    IO_PD2_SetDigitalOutput();
    PORTD.DIRSET = PIN3_bm | PIN4_bm;
    IO_PC6_SetDigitalOutput();
    IO_PD0_SetLow();
    IO_PD1_SetLow();
    IO_PD2_SetLow();
    PORTD.OUTCLR = PIN3_bm | PIN4_bm;
    IO_PC6_SetLow();

    applied_shift_state = 0xFFU;
    shift_register_write(0U);
    ApplicationRegisters_SetInternal(APP_REG_ACTUATOR_STATES, 0U);
}

void HardwareOutputs_Process(uint32_t now_ms)
{
    uint16_t requested = ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES);
    bool permitted = process_outputs_permitted();
    uint16_t applied = permitted ? (requested & OUTPUT_MASK)
                                 : (requested & APP_ACT_RED_PILOT);
    uint8_t shift_state = (uint8_t)applied & SHIFT_REGISTER_ALLOWED_MASK;

    if (shift_state != applied_shift_state) {
        shift_register_write(shift_state);
    }

    if ((applied & APP_ACT_HEAT_FANS) != 0U) {
        IO_PD1_SetHigh();
    } else {
        IO_PD1_SetLow();
    }
    if ((applied & APP_ACT_COOL_FANS) != 0U) {
        IO_PD2_SetHigh();
    } else {
        IO_PD2_SetLow();
    }

    uint16_t heater_duty = ApplicationRegisters_Get(APP_REG_HEATER_PWM);
    if (heater_duty > 1000U) heater_duty = 1000U;
    bool heater_on = permitted &&
                     ((applied & APP_ACT_HEATER_PWM_ACTIVE) != 0U) &&
                     ((now_ms % HEATER_PERIOD_MS) < heater_duty);
    if (heater_on) IO_PD0_SetHigh(); else IO_PD0_SetLow();

    uint16_t eev_duty = ApplicationRegisters_Get(APP_REG_EEV_PWM);
    if (eev_duty > 100U) eev_duty = 100U;
    bool eev_on = permitted &&
                  ((applied & APP_ACT_EEV_PWM_ACTIVE) != 0U) &&
                  ((now_ms % EEV_PERIOD_MS) < ((uint32_t)eev_duty * 60UL));
    if (eev_on) IO_PC6_SetHigh(); else IO_PC6_SetLow();

    PORTD.OUTCLR = PIN3_bm | PIN4_bm;

    if (applied != requested) {
        ApplicationRegisters_SetInternal(APP_REG_ACTUATOR_STATES, applied);
    }
}
