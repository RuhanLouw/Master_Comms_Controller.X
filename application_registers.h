#ifndef APPLICATION_REGISTERS_H
#define APPLICATION_REGISTERS_H

#include <stdbool.h>
#include <stdint.h>

#define APP_REGISTER_COUNT              768U
#define APP_SENSOR_BASE                 0x0100U
#define APP_SENSOR_REGISTER_COUNT       33U

/* Sensor-card register mirror. These addresses contain the unmodified values
 * returned by sensor-card unit 2. */
#define APP_SENSOR_FW_VERSION           (APP_SENSOR_BASE + 0x00U)
#define APP_SENSOR_UPTIME_LOW           (APP_SENSOR_BASE + 0x01U)
#define APP_SENSOR_UPTIME_HIGH          (APP_SENSOR_BASE + 0x02U)
#define APP_SENSOR_STATUS               (APP_SENSOR_BASE + 0x03U)
#define APP_SENSOR_ENABLE               (APP_SENSOR_BASE + 0x04U)
#define APP_SENSOR_CHAMBER_INTERNAL     (APP_SENSOR_BASE + 0x05U)
#define APP_SENSOR_HEATING_ELEMENT      (APP_SENSOR_BASE + 0x06U)
#define APP_SENSOR_EVAPORATOR           (APP_SENSOR_BASE + 0x07U)
#define APP_SENSOR_SUPERHEAT            (APP_SENSOR_BASE + 0x08U)
#define APP_SENSOR_SUBCOOLING           (APP_SENSOR_BASE + 0x09U)
#define APP_SENSOR_COMMAND              (APP_SENSOR_BASE + 0x0AU)
#define APP_SENSOR_LOG_INTERVAL         (APP_SENSOR_BASE + 0x0BU)
#define APP_SENSOR_EXTERNAL             (APP_SENSOR_BASE + 0x0CU)
#define APP_SENSOR_NTC1_TEMP            (APP_SENSOR_BASE + 0x0DU)
#define APP_SENSOR_NTC2_TEMP            (APP_SENSOR_BASE + 0x0EU)
#define APP_SENSOR_NTC3_TEMP            (APP_SENSOR_BASE + 0x0FU)
#define APP_SENSOR_NTC4_TEMP            (APP_SENSOR_BASE + 0x10U)
#define APP_SENSOR_NTC5_TEMP            (APP_SENSOR_BASE + 0x11U)
#define APP_SENSOR_NTC6_TEMP            (APP_SENSOR_BASE + 0x12U)
#define APP_SENSOR_NTC7_TEMP            (APP_SENSOR_BASE + 0x13U)
#define APP_SENSOR_NTC8_TEMP            (APP_SENSOR_BASE + 0x14U)
#define APP_SENSOR_NTC_ERROR_BITS       (APP_SENSOR_BASE + 0x15U)
#define APP_SENSOR_KTYPE_TEMP           (APP_SENSOR_BASE + 0x16U)
#define APP_SENSOR_KTYPE_CJ_TEMP        (APP_SENSOR_BASE + 0x17U)
#define APP_SENSOR_KTYPE_ERROR          (APP_SENSOR_BASE + 0x18U)
#define APP_SENSOR_DHT22_TEMP           (APP_SENSOR_BASE + 0x19U)
#define APP_SENSOR_DHT22_HUMIDITY       (APP_SENSOR_BASE + 0x1AU)
#define APP_SENSOR_DHT22_ERROR          (APP_SENSOR_BASE + 0x1BU)
#define APP_SENSOR_DS18B20_1_TEMP       (APP_SENSOR_BASE + 0x1CU)
#define APP_SENSOR_DS18B20_2_TEMP       (APP_SENSOR_BASE + 0x1DU)
#define APP_SENSOR_DS18B20_1_ERROR      (APP_SENSOR_BASE + 0x1EU)
#define APP_SENSOR_DS18B20_2_ERROR      (APP_SENSOR_BASE + 0x1FU)
#define APP_SENSOR_RESERVED             (APP_SENSOR_BASE + 0x20U)

#define APP_REG_FW_VERSION              0x0000U
#define APP_REG_UPTIME_LOW              0x0001U
#define APP_REG_UPTIME_HIGH             0x0002U
#define APP_REG_SYSTEM_STATUS           0x0003U
#define APP_REG_MAIN_SETPOINT           0x0004U
#define APP_REG_FAULT_FLAGS             0x0005U
#define APP_REG_ACTUATOR_STATES         0x0007U
#define APP_REG_EXTERNAL_TEMPERATURE    0x0008U
#define APP_REG_DEFROST_SETPOINT        0x0009U
#define APP_REG_TEMP_HYSTERESIS         0x000AU
#define APP_REG_CHAMBER_TEMPERATURE     0x000BU
#define APP_REG_HEATER_TEMPERATURE      0x000CU
#define APP_REG_EVAP_BOX_TEMPERATURE    0x000DU
#define APP_REG_SUPERHEAT               0x000EU /* NTC6 - NTC5, K x10. */
#define APP_REG_SUBCOOLING              0x000FU /* NTC7 - NTC8, K x10. */
#define APP_REG_MAX_TEMPERATURE         0x0010U
#define APP_REG_MIN_TEMPERATURE         0x0011U
#define APP_REG_EVAP_MIN_TEMPERATURE    0x0014U
#define APP_REG_EVAP_IN_TEMPERATURE     0x0012U
#define APP_REG_EVAP_OUT_TEMPERATURE    0x0013U
#define APP_REG_SUPERHEAT_TARGET        0x0015U
#define APP_REG_EEV_KI_X10              0x0016U
#define APP_REG_SYSTEM_MODE             0x0017U
#define APP_REG_SYSTEM_STATE            0x0018U
#define APP_REG_HEATING_ACTIVE          0x0019U
#define APP_REG_COOLING_ACTIVE          0x001AU
#define APP_REG_FREE_COOL_ACTIVE        0x001BU
#define APP_REG_COMPRESSOR_STATE        0x001CU
#define APP_REG_COMPRESSOR_ALLOWED      0x001DU
#define APP_REG_COMP_MIN_ON_TIME        0x001EU
#define APP_REG_COMP_MIN_OFF_TIME       0x001FU
#define APP_REG_SAFETY_ACTIVE           0x0020U
#define APP_REG_SAFETY_CODE             0x0021U
#define APP_REG_SAFETY_LATCHED          0x0022U
#define APP_REG_EEV_PWM                 0x0023U
#define APP_REG_EEV_STATE               0x0024U
#define APP_REG_EEV_STARTUP_ACTIVE      0x0025U
#define APP_REG_EEV_FAULT               0x0026U
#define APP_REG_MANUAL_EEV_PWM          0x0027U
#define APP_REG_EEV_MIN_PWM             0x0028U
#define APP_REG_EEV_MAX_PWM             0x0029U
#define APP_REG_HEATER_PWM              0x002AU
#define APP_REG_HEATER_FAN_STATE        0x002BU
#define APP_REG_HEATER_FAULT            0x002CU
#define APP_REG_HEATER_MAX_TEMPERATURE  0x002DU
#define APP_REG_MANUAL_HEATER_PWM       0x002EU
#define APP_REG_COOL_FAN_STATE          0x002FU
#define APP_REG_FREE_COOL_MARKER        0x0030U
#define APP_REG_PID_OUTPUT              0x0031U
#define APP_REG_PID_ERROR               0x0032U
#define APP_REG_PID_INTEGRAL            0x0033U
#define APP_REG_PID_DERIVATIVE          0x0034U
#define APP_REG_TRAJECTORY_TEMPERATURE  0x0035U
#define APP_REG_RAMP_RATE               0x0036U
#define APP_REG_THERMAL_COMMAND         0x0037U
#define APP_REG_OPERATING_MODE          0x0038U
#define APP_REG_MANUAL_HEAT_FANS        0x0039U
#define APP_REG_MANUAL_COOL_FANS        0x003AU
#define APP_REG_EEV_KP_X10              0x003BU
#define APP_REG_FREECOOL_MIN_DT_X10     0x003CU
#define APP_REG_MANUAL_BYPASS_FLAGS     0x003DU
#define APP_REG_SUPERHEAT_FILTER_DIV    0x003EU
#define APP_REG_EEV_KI_X100             0x003FU
#define APP_REG_PID_KP                  0x0040U
#define APP_REG_PID_KI                  0x0041U
#define APP_REG_PID_KD                  0x0042U
#define APP_REG_PID_SAMPLE_TIME_MS      0x0043U
#define APP_REG_DEFROST_INTERVAL        0x0044U
#define APP_REG_FREE_HEATING_ACTIVE     0x0045U
#define APP_REG_PID_INTEGRAL_CLAMP      0x0046U
#define APP_REG_EEV_MAX_STEP            0x0047U

#define APP_REG_SENSOR_RX_BYTE_COUNT    0x0080U
#define APP_REG_SENSOR_RX_LAST_STATUS   0x0081U
#define APP_REG_SENSOR_RX_LAST_BYTE     0x0082U
#define APP_REG_SENSOR_RX_FRAME_LENGTH  0x0083U
#define APP_REG_SENSOR_REJECT_REASON    0x0084U
#define APP_REG_SENSOR_POLL_COUNT       0x0085U

#define APP_STATUS_IDLE                 (1U << 0)
#define APP_STATUS_COOLING              (1U << 1)
#define APP_STATUS_HEATING              (1U << 2)
#define APP_STATUS_DEFROST              (1U << 3)
#define APP_STATUS_FAULT_ACTIVE         (1U << 4)
#define APP_STATUS_REMOTE_MODE          (1U << 5)
#define APP_STATUS_MANUAL_MODE          (1U << 6)
#define APP_STATUS_SENSOR_CARD_OK       (1U << 7)
#define APP_STATUS_CONTROLLER_OK        (1U << 8)

#define APP_FAULT_NTC                   (1U << 0)
#define APP_FAULT_KTYPE                 (1U << 1)
#define APP_FAULT_SENSOR_COMMUNICATION  (1U << 4)
#define APP_FAULT_TEMPERATURE_LIMIT     (1U << 5)
#define APP_FAULT_EEV                   (1U << 7)
#define APP_FAULT_HEATER                (1U << 8)
#define APP_FAULT_COMPRESSOR_MASK       (0x3FU << 9)

#define APP_STATE_IDLE                  0U
#define APP_STATE_HEATING               1U
#define APP_STATE_COOLING               2U
#define APP_STATE_FREE_COOLING          3U
#define APP_STATE_DEFROST               4U
#define APP_STATE_MANUAL                5U

#define APP_EEV_STATE_OFF               0U
#define APP_EEV_STATE_STARTUP           1U
#define APP_EEV_STATE_CONTROL           2U
#define APP_EEV_STATE_MANUAL            3U

#define APP_SAFETY_NONE                 0U
#define APP_SAFETY_SENSOR               1U
#define APP_SAFETY_SENSOR_COMMUNICATION 2U
#define APP_SAFETY_CHAMBER_LIMIT        3U
#define APP_SAFETY_COMPRESSOR           4U
#define APP_SAFETY_EEV                  5U
#define APP_SAFETY_HEATER               6U
#define APP_SAFETY_EVAPORATOR_LIMIT     7U

#define APP_ACT_AC_COMPRESSOR           (1U << 0)
#define APP_ACT_AC_CONDENSER_FAN        (1U << 1)
#define APP_ACT_AC_DEFROST              (1U << 2)
#define APP_ACT_DC_COMPRESSOR           (1U << 4)
#define APP_ACT_GREEN_PILOT             (1U << 5)
#define APP_ACT_ORANGE_PILOT            (1U << 6)
#define APP_ACT_RED_PILOT               (1U << 7)
#define APP_ACT_HEAT_FANS               (1U << 8)
#define APP_ACT_COOL_FANS               (1U << 9)
#define APP_ACT_HEATER_PWM_ACTIVE       (1U << 10)
#define APP_ACT_EEV_PWM_ACTIVE          (1U << 11)

#define APP_SENSOR_STATUS_READY         (1U << 1)
#define APP_ENABLE_NTC1                 (1U << 0)
#define APP_ENABLE_NTC2                 (1U << 1)
#define APP_ENABLE_NTC3                 (1U << 2)
#define APP_ENABLE_NTC4                 (1U << 3)
#define APP_ENABLE_NTC5                 (1U << 4)
#define APP_ENABLE_NTC6                 (1U << 5)
#define APP_ENABLE_NTC7                 (1U << 6)
#define APP_ENABLE_NTC8                 (1U << 7)
#define APP_ENABLE_KTYPE                (1U << 8)
#define APP_ENABLE_DS18B20_1            (1U << 10)
#define APP_ENABLE_DS18B20_2            (1U << 11)

#define APP_TEMPERATURE_INVALID         ((uint16_t)0x8000U)

typedef enum {
    APP_WRITE_OK = 0,
    APP_WRITE_ILLEGAL_ADDRESS = 2,
    APP_WRITE_ILLEGAL_VALUE = 3
} app_write_result_t;

void ApplicationRegisters_Initialize(void);
uint16_t ApplicationRegisters_Get(uint16_t address);
void ApplicationRegisters_SetInternal(uint16_t address, uint16_t value);
app_write_result_t ApplicationRegisters_Write(uint16_t start,
                                                const uint16_t *values,
                                                uint16_t count);
app_write_result_t ApplicationRegisters_WriteCoil(uint16_t coil, bool on);
void ApplicationRegisters_Tick1s(void);

#endif
