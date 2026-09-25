#include "pwm_sw.h"
#include "../../mcc_generated_files/timer/tca0.h"

#define DEFAULT_LOW_PERIOD_MS 0.048f // ~20.83 kHz
#define DEFAULT_HIGH_PERIOD_MS 4.096f // ~244.14 Hz
#define F_CLK 16000000UL // 16 MHz
#define PRESCALER 1024 // Fixed prescaler
#define PRESCALER_BITS TCA_SPLIT_CLKSEL_DIV1024_gc

bool PWM_TCA0_Initialize(void) {
    // Set pins as output (PD1-PD4)
    PORTD.DIR |= DC_HEAT_FAN_PWM_bm | DC_EVAP_FAN_PWM_bm |
                 DC_Heater_Resistor_1_PWM_bm | DC_Heater_Resistor_2_PWM_bm;

    // Enable Split mode
    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;

    // Set default periods
    PWM_TCA0_SetLowPeriod(DEFAULT_LOW_PERIOD_MS);
    PWM_TCA0_SetHighPeriod(DEFAULT_HIGH_PERIOD_MS);

    // Enable PWM channels
    TCA0.SPLIT.CTRLB = TCA_SPLIT_LCMP1EN_bm | TCA_SPLIT_LCMP2EN_bm |
                       TCA_SPLIT_HCMP0EN_bm | TCA_SPLIT_HCMP1EN_bm;

    // Set initial duty cycles to 0%
    TCA0.SPLIT.LCMP1 = 0; // WO1 (PD1)
    TCA0.SPLIT.LCMP2 = 0; // WO2 (PD2)
    TCA0.SPLIT.HCMP0 = 0; // WO3 (PD3)
    TCA0.SPLIT.HCMP1 = 0; // WO4 (PD4)

    // Set fixed prescaler
    TCA0.SPLIT.CTRLA = PRESCALER_BITS;

    return true;
}

void PWM_TCA0_Enable(void) {
    TCA0.SPLIT.CTRLA |= TCA_SPLIT_ENABLE_bm;
}

void PWM_TCA0_Disable(void) {
    TCA0.SPLIT.CTRLA &= ~TCA_SPLIT_ENABLE_bm;
}

bool PWM_TCA0_SetLowPeriod(float period_ms) {
    // Calculate frequency
    float freq_hz = 1000.0f / period_ms;
    uint32_t per = (uint32_t)(F_CLK / (PRESCALER * freq_hz)) - 1;
    if (per > 255) {
        return false; // Out of 8-bit range
    }

    // Update period
    TCA0.SPLIT.LPER = (uint8_t)per;
    return true;
}

bool PWM_TCA0_SetHighPeriod(float period_ms) {
    // Calculate frequency
    float freq_hz = 1000.0f / period_ms;
    uint32_t per = (uint32_t)(F_CLK / (PRESCALER * freq_hz)) - 1;
    if (per > 255) {
        return false; // Out of 8-bit range 
    }

    // Update period
    TCA0.SPLIT.HPER = (uint8_t)per;
    return true;
}

bool PWM_TCA0_SetDutyCycle_DC_Heat_Fan(uint8_t duty_cycle) {
    if (duty_cycle > 100) {
        return false;
    }
    uint16_t cmp_value = ((uint16_t)duty_cycle * (TCA0.SPLIT.LPER + 1)) / 100;
    TCA0.SPLIT.LCMP1 = (uint8_t)cmp_value; // WO1 (PD1)
    return true;
}

bool PWM_TCA0_SetDutyCycle_DC_Evap_Fan(uint8_t duty_cycle) {
    if (duty_cycle > 100) {
        return false;
    }
    uint16_t cmp_value = ((uint16_t)duty_cycle * (TCA0.SPLIT.LPER + 1)) / 100;
    TCA0.SPLIT.LCMP2 = (uint8_t)cmp_value; // WO2 (PD2)
    return true;
}

bool PWM_TCA0_SetDutyCycle_DC_Heater_1(uint8_t duty_cycle) {
    if (duty_cycle > 100) {
        return false;
    }
    uint16_t cmp_value = ((uint16_t)duty_cycle * (TCA0.SPLIT.HPER + 1)) / 100;
    TCA0.SPLIT.HCMP0 = (uint8_t)cmp_value; // WO3 (PD3)
    return true;
}

bool PWM_TCA0_SetDutyCycle_DC_Heater_2(uint8_t duty_cycle) {
    if (duty_cycle > 100) {
        return false;
    }
    uint16_t cmp_value = ((uint16_t)duty_cycle * (TCA0.SPLIT.HPER + 1)) / 100;
    TCA0.SPLIT.HCMP1 = (uint8_t)cmp_value; // WO4 (PD4)
    return true;
}