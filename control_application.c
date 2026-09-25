#include "control_application.h"

#include <stdbool.h>
#include <stdint.h>

#include "application_registers.h"

#define RAMP_COUNTS_PER_STEP_X10 600UL
#define EEV_STARTUP_DELAY_S       6U
#define EEV_PROXY_MAX_X10         600
#define EEV_DEADBAND_X10          5
#define PROCESS_OUTPUT_MASK \
    (APP_ACT_AC_COMPRESSOR | APP_ACT_AC_CONDENSER_FAN | \
     APP_ACT_AC_DEFROST | APP_ACT_DC_COMPRESSOR | \
     APP_ACT_HEAT_FANS | APP_ACT_COOL_FANS | \
     APP_ACT_HEATER_PWM_ACTIVE | APP_ACT_EEV_PWM_ACTIVE)

static uint32_t ramp_accumulator;
static int8_t ramp_direction;
static int32_t pid_integral;
static int16_t previous_pid_error;
static bool pid_initialized;

static bool compressor_on;
static uint16_t compressor_on_s;
static uint16_t compressor_off_s;

static bool eev_previous_compressor;
static uint16_t eev_startup_s;
static int16_t eev_filtered_proxy_x10;
static int16_t eev_previous_error_x10;
static bool eev_filter_initialized;

static bool previous_manual_mode;

static int16_t signed_register(uint16_t address)
{
    return (int16_t)ApplicationRegisters_Get(address);
}

static void set_signed_register(uint16_t address, int32_t value)
{
    if (value > INT16_MAX) value = INT16_MAX;
    if (value < INT16_MIN) value = INT16_MIN;
    ApplicationRegisters_SetInternal(address, (uint16_t)(int16_t)value);
}

static uint16_t clamp_u16(uint16_t value, uint16_t minimum, uint16_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void set_actuator(uint16_t mask, bool on)
{
    uint16_t outputs = ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES);
    outputs = on ? (uint16_t)(outputs | mask)
                 : (uint16_t)(outputs & (uint16_t)~mask);
    ApplicationRegisters_SetInternal(APP_REG_ACTUATOR_STATES, outputs);
}

static void clear_process_outputs(void)
{
    uint16_t outputs = ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES);
    outputs &= (uint16_t)~PROCESS_OUTPUT_MASK;
    ApplicationRegisters_SetInternal(APP_REG_ACTUATOR_STATES, outputs);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_PWM, 0U);
    ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, 0U);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_FAN_STATE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_COOL_FAN_STATE, 0U);
}

static void reset_pid(void)
{
    pid_integral = 0;
    previous_pid_error = 0;
    pid_initialized = false;
    ApplicationRegisters_SetInternal(APP_REG_PID_OUTPUT, 0U);
    ApplicationRegisters_SetInternal(APP_REG_PID_ERROR, 0U);
    ApplicationRegisters_SetInternal(APP_REG_PID_INTEGRAL, 0U);
    ApplicationRegisters_SetInternal(APP_REG_PID_DERIVATIVE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_THERMAL_COMMAND, 0U);
}

static void reset_eev_controller(void)
{
    eev_startup_s = 0U;
    eev_filtered_proxy_x10 = 0;
    eev_previous_error_x10 = 0;
    eev_filter_initialized = false;
    eev_previous_compressor = false;
    ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, 0U);
    ApplicationRegisters_SetInternal(APP_REG_EEV_STATE, APP_EEV_STATE_OFF);
    ApplicationRegisters_SetInternal(APP_REG_EEV_STARTUP_ACTIVE, 0U);
    set_actuator(APP_ACT_EEV_PWM_ACTIVE, false);
}

static void update_status(uint16_t state, bool safety_active)
{
    uint16_t status = ApplicationRegisters_Get(APP_REG_SYSTEM_STATUS);
    status &= (uint16_t)~(APP_STATUS_IDLE | APP_STATUS_COOLING |
                          APP_STATUS_HEATING | APP_STATUS_DEFROST |
                          APP_STATUS_FAULT_ACTIVE | APP_STATUS_MANUAL_MODE);

    if (safety_active) status |= APP_STATUS_FAULT_ACTIVE;
    if (state == APP_STATE_MANUAL) status |= APP_STATUS_MANUAL_MODE;
    else if (state == APP_STATE_HEATING) status |= APP_STATUS_HEATING;
    else if ((state == APP_STATE_COOLING) ||
             (state == APP_STATE_FREE_COOLING)) status |= APP_STATUS_COOLING;
    else if (state == APP_STATE_DEFROST) status |= APP_STATUS_DEFROST;
    else status |= APP_STATUS_IDLE;

    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_STATUS, status);
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_STATE, state);
}

static bool update_safety(void)
{
    uint16_t faults = ApplicationRegisters_Get(APP_REG_FAULT_FLAGS);
    uint16_t safety_code = APP_SAFETY_NONE;
    bool trip = false;

    bool communication_fault =
        ((faults & APP_FAULT_SENSOR_COMMUNICATION) != 0U) ||
        ((ApplicationRegisters_Get(APP_REG_SYSTEM_STATUS) &
          APP_STATUS_SENSOR_CARD_OK) == 0U);
    bool chamber_invalid =
        ApplicationRegisters_Get(APP_REG_CHAMBER_TEMPERATURE) ==
        APP_TEMPERATURE_INVALID;
    bool heater_invalid =
        ApplicationRegisters_Get(APP_REG_HEATER_TEMPERATURE) ==
        APP_TEMPERATURE_INVALID;

    bool chamber_limit = false;
    if (!chamber_invalid) {
        int16_t chamber = signed_register(APP_REG_CHAMBER_TEMPERATURE);
        int16_t minimum = signed_register(APP_REG_MIN_TEMPERATURE);
        int16_t maximum = signed_register(APP_REG_MAX_TEMPERATURE);
        chamber_limit = (maximum > minimum) &&
                        ((chamber < minimum) || (chamber > maximum));
    }

    bool heater_limit = false;
    if (!heater_invalid) {
        heater_limit = signed_register(APP_REG_HEATER_TEMPERATURE) >=
                       signed_register(APP_REG_HEATER_MAX_TEMPERATURE);
    }

    bool compressor_active = compressor_on ||
        ((ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES) &
          (APP_ACT_AC_COMPRESSOR | APP_ACT_DC_COMPRESSOR)) != 0U);
    bool evaporator_limit = false;
    uint16_t evaporator_raw =
        ApplicationRegisters_Get(APP_REG_EVAP_BOX_TEMPERATURE);
    if (compressor_active && (evaporator_raw != APP_TEMPERATURE_INVALID)) {
        evaporator_limit =
            (int16_t)evaporator_raw <=
            signed_register(APP_REG_EVAP_MIN_TEMPERATURE);
    }

    if (chamber_limit || evaporator_limit) faults |= APP_FAULT_TEMPERATURE_LIMIT;
    else faults &= (uint16_t)~APP_FAULT_TEMPERATURE_LIMIT;

    if (heater_limit) {
        faults |= APP_FAULT_HEATER;
        ApplicationRegisters_SetInternal(APP_REG_HEATER_FAULT, 1U);
    } else {
        faults &= (uint16_t)~APP_FAULT_HEATER;
        ApplicationRegisters_SetInternal(APP_REG_HEATER_FAULT, 0U);
    }
    ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS, faults);

    if (communication_fault) {
        trip = true;
        safety_code = APP_SAFETY_SENSOR_COMMUNICATION;
    } else if (((faults & (APP_FAULT_NTC | APP_FAULT_KTYPE)) != 0U) ||
               chamber_invalid || heater_invalid) {
        trip = true;
        safety_code = APP_SAFETY_SENSOR;
    } else if (chamber_limit) {
        trip = true;
        safety_code = APP_SAFETY_CHAMBER_LIMIT;
    } else if (evaporator_limit) {
        trip = true;
        safety_code = APP_SAFETY_EVAPORATOR_LIMIT;
    } else if ((faults & APP_FAULT_COMPRESSOR_MASK) != 0U) {
        trip = true;
        safety_code = APP_SAFETY_COMPRESSOR;
    } else if ((faults & APP_FAULT_EEV) != 0U) {
        trip = true;
        safety_code = APP_SAFETY_EEV;
    } else if ((faults & APP_FAULT_HEATER) != 0U) {
        trip = true;
        safety_code = APP_SAFETY_HEATER;
    }

    uint16_t latched = ApplicationRegisters_Get(APP_REG_SAFETY_LATCHED);
    uint16_t active = ApplicationRegisters_Get(APP_REG_SAFETY_ACTIVE);
    if (trip) {
        if (active == 0U) {
            ApplicationRegisters_SetInternal(APP_REG_SAFETY_CODE, safety_code);
        }
        active = 1U;
        latched = 1U;
    } else if (latched == 0U) {
        active = 0U;
        ApplicationRegisters_SetInternal(APP_REG_SAFETY_CODE, APP_SAFETY_NONE);
    } else {
        active = 1U;
    }

    ApplicationRegisters_SetInternal(APP_REG_SAFETY_ACTIVE, active);
    ApplicationRegisters_SetInternal(APP_REG_SAFETY_LATCHED, latched);
    return active != 0U;
}

static void update_trajectory(void)
{
    int16_t target = signed_register(APP_REG_MAIN_SETPOINT);
    int16_t trajectory = signed_register(APP_REG_TRAJECTORY_TEMPERATURE);
    uint16_t rate_x100 = ApplicationRegisters_Get(APP_REG_RAMP_RATE);

    if (rate_x100 == 0U) {
        set_signed_register(APP_REG_TRAJECTORY_TEMPERATURE, target);
        ramp_accumulator = 0U;
        ramp_direction = 0;
        return;
    }

    int32_t difference = (int32_t)target - trajectory;
    if (difference == 0) {
        ramp_accumulator = 0U;
        ramp_direction = 0;
        return;
    }

    int8_t direction = (difference > 0) ? 1 : -1;
    if (direction != ramp_direction) {
        ramp_accumulator = 0U;
        ramp_direction = direction;
    }

    ramp_accumulator += rate_x100;
    uint16_t steps = (uint16_t)(ramp_accumulator / RAMP_COUNTS_PER_STEP_X10);
    ramp_accumulator %= RAMP_COUNTS_PER_STEP_X10;
    if (steps == 0U) return;

    uint32_t distance = (uint32_t)((difference > 0) ? difference : -difference);
    if (distance <= steps) {
        trajectory = target;
        ramp_accumulator = 0U;
        ramp_direction = 0;
    } else {
        trajectory = (int16_t)((int32_t)trajectory +
                               ((int32_t)direction * steps));
    }
    set_signed_register(APP_REG_TRAJECTORY_TEMPERATURE, trajectory);
}

static int16_t update_pid(void)
{
    int16_t measured = signed_register(APP_REG_CHAMBER_TEMPERATURE);
    int16_t target = signed_register(APP_REG_TRAJECTORY_TEMPERATURE);
    int16_t error = (int16_t)clamp_i32((int32_t)target - measured,
                                      INT16_MIN, INT16_MAX);
    int32_t proportional =
        ((int32_t)(int16_t)ApplicationRegisters_Get(APP_REG_PID_KP) * error) /
        100;
    int32_t integral_delta =
        ((int32_t)(int16_t)ApplicationRegisters_Get(APP_REG_PID_KI) * error) /
        100;
    int32_t integral_limit =
        ApplicationRegisters_Get(APP_REG_PID_INTEGRAL_CLAMP);
    pid_integral = clamp_i32(pid_integral + integral_delta,
                             -integral_limit, integral_limit);

    int32_t derivative = 0;
    if (pid_initialized) {
        derivative =
            ((int32_t)(int16_t)ApplicationRegisters_Get(APP_REG_PID_KD) *
             ((int32_t)error - previous_pid_error)) / 100;
    }
    previous_pid_error = error;
    pid_initialized = true;

    int16_t output = (int16_t)clamp_i32(proportional + pid_integral + derivative,
                                       -1000, 1000);
    set_signed_register(APP_REG_PID_ERROR, error);
    set_signed_register(APP_REG_PID_INTEGRAL, pid_integral);
    set_signed_register(APP_REG_PID_DERIVATIVE, derivative);
    set_signed_register(APP_REG_PID_OUTPUT, output);
    set_signed_register(APP_REG_THERMAL_COMMAND, output);
    return output;
}

static void update_compressor(bool request, bool safety_active)
{
    bool compressor_fault =
        (ApplicationRegisters_Get(APP_REG_FAULT_FLAGS) &
         APP_FAULT_COMPRESSOR_MASK) != 0U;
    bool allowed = !safety_active && !compressor_fault;
    uint16_t min_on = ApplicationRegisters_Get(APP_REG_COMP_MIN_ON_TIME);
    uint16_t min_off = ApplicationRegisters_Get(APP_REG_COMP_MIN_OFF_TIME);

    if (compressor_on) {
        if (compressor_on_s < UINT16_MAX) compressor_on_s++;
        compressor_off_s = 0U;
    } else {
        if (compressor_off_s < UINT16_MAX) compressor_off_s++;
        compressor_on_s = 0U;
    }

    if (!allowed) {
        compressor_on = false;
        compressor_on_s = 0U;
        compressor_off_s = 0U;
    } else if (!compressor_on && request && (compressor_off_s >= min_off)) {
        compressor_on = true;
        compressor_on_s = 0U;
        compressor_off_s = 0U;
    } else if (compressor_on && !request && (compressor_on_s >= min_on)) {
        compressor_on = false;
        compressor_on_s = 0U;
        compressor_off_s = 0U;
    }

    ApplicationRegisters_SetInternal(APP_REG_COMPRESSOR_ALLOWED,
                                     allowed ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_COMPRESSOR_STATE,
                                     compressor_on ? 1U : 0U);
    set_actuator(APP_ACT_AC_COMPRESSOR, compressor_on);
    set_actuator(APP_ACT_AC_CONDENSER_FAN, compressor_on);
}

static void update_eev(bool safety_active)
{
    if (safety_active || !compressor_on) {
        reset_eev_controller();
        return;
    }

    if (!eev_previous_compressor) {
        eev_startup_s = EEV_STARTUP_DELAY_S;
        eev_previous_compressor = true;
        ApplicationRegisters_SetInternal(APP_REG_EEV_STATE,
                                         APP_EEV_STATE_STARTUP);
        ApplicationRegisters_SetInternal(APP_REG_EEV_STARTUP_ACTIVE, 1U);
        ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, 0U);
        set_actuator(APP_ACT_EEV_PWM_ACTIVE, false);
        return;
    }

    if (eev_startup_s > 0U) {
        eev_startup_s--;
        ApplicationRegisters_SetInternal(APP_REG_EEV_STATE,
                                         APP_EEV_STATE_STARTUP);
        ApplicationRegisters_SetInternal(APP_REG_EEV_STARTUP_ACTIVE, 1U);
        ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, 0U);
        set_actuator(APP_ACT_EEV_PWM_ACTIVE, false);
        return;
    }

    int16_t proxy = signed_register(APP_REG_SUPERHEAT);
    if ((ApplicationRegisters_Get(APP_REG_SUPERHEAT) ==
         APP_TEMPERATURE_INVALID) ||
        (proxy < 0) || (proxy > EEV_PROXY_MAX_X10)) {
        uint16_t faults = ApplicationRegisters_Get(APP_REG_FAULT_FLAGS);
        ApplicationRegisters_SetInternal(APP_REG_EEV_FAULT, 1U);
        ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS,
                                         (uint16_t)(faults | APP_FAULT_EEV));
        reset_eev_controller();
        return;
    }

    uint16_t faults = ApplicationRegisters_Get(APP_REG_FAULT_FLAGS);
    ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS,
                                     (uint16_t)(faults & ~APP_FAULT_EEV));
    ApplicationRegisters_SetInternal(APP_REG_EEV_FAULT, 0U);

    int16_t divisor =
        (int16_t)ApplicationRegisters_Get(APP_REG_SUPERHEAT_FILTER_DIV);
    if (divisor < 1) divisor = 1;
    if (!eev_filter_initialized) {
        eev_filtered_proxy_x10 = proxy;
        eev_filter_initialized = true;
    } else {
        eev_filtered_proxy_x10 =
            (int16_t)(eev_filtered_proxy_x10 +
                      ((proxy - eev_filtered_proxy_x10) / divisor));
    }

    int16_t error = (int16_t)(eev_filtered_proxy_x10 -
                              signed_register(APP_REG_SUPERHEAT_TARGET));
    if ((error >= -EEV_DEADBAND_X10) && (error <= EEV_DEADBAND_X10)) {
        error = 0;
    }

    int32_t change =
        ((int32_t)(error - eev_previous_error_x10) *
         (int16_t)ApplicationRegisters_Get(APP_REG_EEV_KP_X10)) / 100;
    change +=
        ((int32_t)error *
         (int16_t)ApplicationRegisters_Get(APP_REG_EEV_KI_X100)) / 1000;
    eev_previous_error_x10 = error;

    int16_t maximum_step =
        (int16_t)ApplicationRegisters_Get(APP_REG_EEV_MAX_STEP);
    change = clamp_i32(change, -maximum_step, maximum_step);

    uint16_t minimum = ApplicationRegisters_Get(APP_REG_EEV_MIN_PWM);
    uint16_t maximum = ApplicationRegisters_Get(APP_REG_EEV_MAX_PWM);
    if (minimum > maximum) {
        uint16_t temporary = minimum;
        minimum = maximum;
        maximum = temporary;
    }
    minimum = clamp_u16(minimum, 0U, 100U);
    maximum = clamp_u16(maximum, minimum, 100U);

    int32_t current = ApplicationRegisters_Get(APP_REG_EEV_PWM);
    if (current == 0) current = minimum;
    uint16_t duty = (uint16_t)clamp_i32(current + change, minimum, maximum);
    ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, duty);
    ApplicationRegisters_SetInternal(APP_REG_EEV_STATE,
                                     APP_EEV_STATE_CONTROL);
    ApplicationRegisters_SetInternal(APP_REG_EEV_STARTUP_ACTIVE, 0U);
    set_actuator(APP_ACT_EEV_PWM_ACTIVE, duty > 0U);
}

static void run_manual(bool safety_active)
{
    if (!previous_manual_mode) {
        clear_process_outputs();
        compressor_on = false;
        compressor_on_s = 0U;
        compressor_off_s = 0U;
        reset_pid();
        reset_eev_controller();
    }
    previous_manual_mode = true;

    int16_t eev_proxy = signed_register(APP_REG_SUPERHEAT);
    if ((ApplicationRegisters_Get(APP_REG_SUPERHEAT) !=
         APP_TEMPERATURE_INVALID) &&
        (eev_proxy >= 0) && (eev_proxy <= EEV_PROXY_MAX_X10)) {
        uint16_t faults = ApplicationRegisters_Get(APP_REG_FAULT_FLAGS);
        ApplicationRegisters_SetInternal(APP_REG_EEV_FAULT, 0U);
        ApplicationRegisters_SetInternal(APP_REG_FAULT_FLAGS,
                                         (uint16_t)(faults & ~APP_FAULT_EEV));
    }

    if (safety_active) {
        clear_process_outputs();
        set_actuator(APP_ACT_GREEN_PILOT, false);
        set_actuator(APP_ACT_ORANGE_PILOT, false);
        set_actuator(APP_ACT_RED_PILOT, true);
    } else {
        uint16_t outputs = ApplicationRegisters_Get(APP_REG_ACTUATOR_STATES);
        bool heater_enabled = (outputs & APP_ACT_HEATER_PWM_ACTIVE) != 0U;
        bool eev_enabled = (outputs & APP_ACT_EEV_PWM_ACTIVE) != 0U;
        uint16_t heater = heater_enabled
                              ? ApplicationRegisters_Get(
                                    APP_REG_MANUAL_HEATER_PWM)
                              : 0U;
        uint16_t eev = eev_enabled
                           ? ApplicationRegisters_Get(APP_REG_MANUAL_EEV_PWM)
                           : 0U;
        uint16_t eev_min = ApplicationRegisters_Get(APP_REG_EEV_MIN_PWM);
        uint16_t eev_max = ApplicationRegisters_Get(APP_REG_EEV_MAX_PWM);
        if (eev_min > eev_max) {
            uint16_t temporary = eev_min;
            eev_min = eev_max;
            eev_max = temporary;
        }
        if (eev > 0U) eev = clamp_u16(eev, eev_min, eev_max);

        ApplicationRegisters_SetInternal(APP_REG_HEATER_PWM, heater);
        ApplicationRegisters_SetInternal(APP_REG_EEV_PWM, eev);
        ApplicationRegisters_SetInternal(APP_REG_EEV_STATE,
                                         APP_EEV_STATE_MANUAL);
        ApplicationRegisters_SetInternal(APP_REG_EEV_STARTUP_ACTIVE, 0U);
        ApplicationRegisters_SetInternal(
            APP_REG_COMPRESSOR_STATE,
            ((outputs & (APP_ACT_AC_COMPRESSOR | APP_ACT_DC_COMPRESSOR)) != 0U)
                ? 1U : 0U);
        ApplicationRegisters_SetInternal(APP_REG_COMPRESSOR_ALLOWED, 1U);
        ApplicationRegisters_SetInternal(
            APP_REG_HEATER_FAN_STATE,
            (outputs & APP_ACT_HEAT_FANS) != 0U ? 1U : 0U);
        ApplicationRegisters_SetInternal(
            APP_REG_COOL_FAN_STATE,
            (outputs & APP_ACT_COOL_FANS) != 0U ? 1U : 0U);
    }

    ApplicationRegisters_SetInternal(APP_REG_HEATING_ACTIVE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_COOLING_ACTIVE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_FREE_COOL_ACTIVE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_FREE_HEATING_ACTIVE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_FREE_COOL_MARKER, 0U);
    update_status(APP_STATE_MANUAL, safety_active);
}

static void run_automatic(bool safety_active)
{
    previous_manual_mode = false;
    set_actuator(APP_ACT_AC_DEFROST, false);
    set_actuator(APP_ACT_DC_COMPRESSOR, false);

    if (safety_active) {
        update_compressor(false, true);
        reset_pid();
        reset_eev_controller();
        clear_process_outputs();
        set_actuator(APP_ACT_GREEN_PILOT, false);
        set_actuator(APP_ACT_ORANGE_PILOT, false);
        set_actuator(APP_ACT_RED_PILOT, true);
        ApplicationRegisters_SetInternal(APP_REG_HEATING_ACTIVE, 0U);
        ApplicationRegisters_SetInternal(APP_REG_COOLING_ACTIVE, 0U);
        ApplicationRegisters_SetInternal(APP_REG_FREE_COOL_ACTIVE, 0U);
        ApplicationRegisters_SetInternal(APP_REG_FREE_HEATING_ACTIVE, 0U);
        update_status(APP_STATE_IDLE, true);
        return;
    }

    update_trajectory();
    int16_t demand = update_pid();
    int16_t chamber = signed_register(APP_REG_CHAMBER_TEMPERATURE);
    int16_t target = signed_register(APP_REG_TRAJECTORY_TEMPERATURE);
    int16_t error = (int16_t)((int32_t)target - chamber);
    int16_t hysteresis =
        (int16_t)ApplicationRegisters_Get(APP_REG_TEMP_HYSTERESIS);
    uint16_t operating_mode =
        ApplicationRegisters_Get(APP_REG_OPERATING_MODE);

    bool heat_request = (error > hysteresis) && (operating_mode != 2U);
    bool cool_request = (error < -hysteresis) && (operating_mode != 1U);
    bool free_cool = false;
    uint16_t evaporator_raw =
        ApplicationRegisters_Get(APP_REG_EVAP_BOX_TEMPERATURE);
    if (cool_request && (evaporator_raw != APP_TEMPERATURE_INVALID)) {
        int16_t minimum_delta =
            (int16_t)ApplicationRegisters_Get(APP_REG_FREECOOL_MIN_DT_X10);
        free_cool = ((int32_t)(int16_t)evaporator_raw + minimum_delta) <=
                    chamber;
    }

    bool compressor_request = cool_request && !free_cool;
    update_compressor(compressor_request, false);

    if (compressor_on) heat_request = false;
    uint16_t heater_duty = (heat_request && (demand > 0))
                               ? (uint16_t)demand
                               : 0U;
    bool cool_fans = cool_request || compressor_on;
    bool heat_fans = heater_duty > 0U;

    ApplicationRegisters_SetInternal(APP_REG_HEATER_PWM, heater_duty);
    ApplicationRegisters_SetInternal(APP_REG_HEATING_ACTIVE,
                                     heat_request ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_COOLING_ACTIVE,
                                     compressor_request ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_FREE_COOL_ACTIVE,
                                     free_cool ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_FREE_HEATING_ACTIVE,
                                     heat_fans ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_FREE_COOL_MARKER,
                                     free_cool ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_HEATER_FAN_STATE,
                                     heat_fans ? 1U : 0U);
    ApplicationRegisters_SetInternal(APP_REG_COOL_FAN_STATE,
                                     cool_fans ? 1U : 0U);
    set_actuator(APP_ACT_HEATER_PWM_ACTIVE, heater_duty > 0U);
    set_actuator(APP_ACT_HEAT_FANS, heat_fans);
    set_actuator(APP_ACT_COOL_FANS, cool_fans);
    update_eev(false);

    uint16_t state = APP_STATE_IDLE;
    if (heat_request) state = APP_STATE_HEATING;
    else if (compressor_on || compressor_request) state = APP_STATE_COOLING;
    else if (free_cool) state = APP_STATE_FREE_COOLING;

    set_actuator(APP_ACT_GREEN_PILOT, true);
    set_actuator(APP_ACT_RED_PILOT, false);
    set_actuator(APP_ACT_ORANGE_PILOT, state != APP_STATE_IDLE);
    update_status(state, false);
}

void ControlApplication_Initialize(void)
{
    ramp_accumulator = 0U;
    ramp_direction = 0;
    compressor_on = false;
    compressor_on_s = 0U;
    compressor_off_s = 0U;
    previous_manual_mode = true;
    reset_pid();
    reset_eev_controller();

    int16_t initial_target = signed_register(APP_REG_MAIN_SETPOINT);
    uint16_t chamber = ApplicationRegisters_Get(APP_REG_CHAMBER_TEMPERATURE);
    if (chamber != APP_TEMPERATURE_INVALID) initial_target = (int16_t)chamber;
    set_signed_register(APP_REG_TRAJECTORY_TEMPERATURE, initial_target);
    ApplicationRegisters_SetInternal(APP_REG_SYSTEM_STATE, APP_STATE_MANUAL);
    ApplicationRegisters_SetInternal(APP_REG_SAFETY_ACTIVE, 0U);
    ApplicationRegisters_SetInternal(APP_REG_SAFETY_CODE, APP_SAFETY_NONE);
    ApplicationRegisters_SetInternal(APP_REG_SAFETY_LATCHED, 0U);
}

void ControlApplication_Step1s(void)
{
    bool safety_active = update_safety();
    if (ApplicationRegisters_Get(APP_REG_SYSTEM_MODE) != 0U) {
        run_manual(safety_active);
    } else {
        run_automatic(safety_active);
    }
}
