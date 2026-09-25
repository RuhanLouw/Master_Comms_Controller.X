/* MODBUS MASTER REGISTERS
 * File:   system_registers.h
 * Author: Ruhan Louw
 *
 * Created on October 22, 2025, 4:15 PM
 */

#ifndef SYSTEM_REGISTERS_H
#define	SYSTEM_REGISTERS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdint.h>

/* ============================================================
 *  MASTER REGISTER MAP
 * ============================================================ */

#define SYS_REG_COUNT           768
extern volatile int16_t system_regs[SYS_REG_COUNT];
#define REG(x)                  system_regs[x]
#define SENSOR_BASE             0x0100  // Slave mirror starts here
#define SREG_REGISTER_COUNT     33

// version 3

/*
 * MASTER / CONTROLLER MODBUS REGISTER MAP
 * ---------------------------------------
 * 0x0000..0x00FF : Master/controller registers
 * 0x0100..0x01FF : Sensor slave mirror (see SENSOR_BASE)
 *
 * All temps in °C×10 unless noted.
 */

/* ============================================================
 * CORE / STATUS (0x0000..0x0008)
 * ============================================================ */

/* Firmware version ×100, e.g. 123 = v1.23
 * old: MREG_FIRMWARE
 * Pi: R
 */

#define MREG_FW_VERSION                 0x0000

/* Uptime in seconds (32-bit, low/high)
 * old: MREG_SYS_UPTIME_L/H
 * Pi: R
 */
#define MREG_SYS_UPTIME_L               0x0001
#define MREG_SYS_UPTIME_H               0x0002

/* System status bitfield (see SYS_STATUS_* below)
 * Pi: R
 */
#define MREG_SYS_STATUS                 0x0003

/* Main chamber setpoint °C×10
 * This is what the user sets from Pi.
 * old: MREG_TARGET_TEMP
 * Pi: R/W
 */
#define MREG_T_SP_MAIN                    0x0004

/* High-level fault flags bitfield (FAULT_* below)
 * Pi: R
 */
#define MREG_FAULT_FLAGS                0x0005

/* 0x0006 was MODE_FLAGS, not used anymore */

/* Actuator state bitfield (actual ON/OFF state)
 * - Firmware owns this word.
 * - Bits 0..7 go to shift register.
 * - Bits 8..11 used as logical flags (fans, PWM-active).
 * old: MREG_MANUAL_ACTUATOR_STATES
 * Pi: R
 */
#define MREG_ACTUATOR_STATES            0x0007
#define MREG_MANUAL_ACTUATOR_STATES     MREG_ACTUATOR_STATES  // alias for old code

/* External DS18B20 temperature °C×10
 * Pi: R
 */
#define MREG_EXTERNAL                   0x0008


/* ============================================================
 * SETPOINTS & TEMPERATURES (0x0009..0x0016)
 * ============================================================ */

/* Defrost setpoint °C×10 (if/when defrost is done)
 * old: MREG_SETPOINT_DEFROST
 * Pi: R/W
 */
#define MREG_SP_DEFROST                 0x0009

/* Temperature hysteresis °C×10 (simple deadband if needed)
 * Pi: R/W
 */
#define MREG_TEMP_HYSTERESIS            0x000A

/* Chamber temperature °C×10 (mirror of sensor card)
 * old: MREG_CHAMBER_INTERNAL
 * Pi: R
 */
#define MREG_T_CHAMBER                  0x000B

/* Heater element surface temp °C×10 (K-type on heater)
 * old: MREG_T_HEATING_ELEMENT
 * Pi: R
 */
#define MREG_T_HEATER_ELEMENT           0x000C

/* Evaporator box air temp °C×10 (DS18B20)
 * old: MREG_EVAPORATOR
 * Pi: R
 */
#define MREG_T_EVAP_BOX                 0x000D

/* Superheat °C×10
 * Pi: R
 */
#define MREG_SUPERHEAT                  0x000E

/* Subcooling °C×10
 * Pi: R
 */
#define MREG_SUBCOOLING                 0x000F

/* Chamber safety limits °C×10
 * Pi: R/W
 */
#define MREG_T_MAX_CHAMBER              0x0010
#define MREG_T_MIN_CHAMBER              0x0011

/* Evaporator inlet / outlet pipe temps °C×10
 * Pi: R
 */
#define MREG_T_EVAP_IN                  0x0012
#define MREG_T_EVAP_OUT                 0x0013

/* Min allowed evaporator temp °C×10 (icing limit)
 * Pi: R/W
 */
#define MREG_T_EVAP_MIN_ALLOWED         0x0014

/* Target superheat °C×10 for EEV
 * Pi: R/W
 */
#define MREG_SH_TARGET                  0x0015

/* Reserved: spare setpoint/config
 * Pi: R/W (if you reuse it)
 */
#define MREG_EEV_KI_X10         0x0016


/* ============================================================
 * MODES / STATES / COMPRESSOR (0x0017..0x001F)
 * ============================================================ */

/* 0 = AUTO (closed-loop)
 * 1 = MANUAL (manual regs used)
 * Pi: R/W
 */
#define MREG_SYSTEM_MODE                0x0017

/* High-level system state (enum)
 * Suggested:
 * 0 = MODE_MANUAL
 * 1 = MODE_HEAT
 * 2 = MODE_IDLE
 * 3 = MODE_COOL_FANS_ONLY
 * 4 = MODE_COOL_ACTIVE
 * Pi: R
 */
#define MREG_SYS_STATE                  0x0018

/* Flags from mode_manager
 * 0/1 for each.
 * Pi: R
 */
#define MREG_HEATING_ACTIVE             0x0019
#define MREG_COOLING_ACTIVE             0x001A
#define MREG_FREE_COOL_ACTIVE           0x001B

/* Compressor state and timing
 * These refer to "the active compressor" (AC or DC), depending how you configure it.
 * Pi: R/W for timing, R for states.
 */
#define MREG_COMPRESSOR_STATE           0x001C  // actual ON/OFF
#define MREG_COMPRESSOR_ALLOWED         0x001D  // supervisor says allowed
#define MREG_COMP_MIN_ON_TIME           0x001E  // s, Pi: R/W
#define MREG_COMP_MIN_OFF_TIME          0x001F  // s, Pi: R/W

/* Note: if you later want AC/DC compressor selection or separate states,
 * we can add MREG_COMP_TYPE or similar in the expansion area.
 */


/* ============================================================
 * SAFETY (0x0020..0x0022)
 * ============================================================ */

/* Safety status and latch
 * Pi: R, latch can be cleared by writing 0 to MREG_SAFETY_LATCHED.
 */
#define MREG_SAFETY_ACTIVE              0x0020
#define MREG_SAFETY_CODE                0x0021
#define MREG_SAFETY_LATCHED             0x0022


/* ============================================================
 * EEV (0x0023..0x0029)
 * ============================================================ */

/* EEV PWM command
 * Decide internally if you use 0..100 (%), or 0..1000 (%×10).
 * Pi: R
 */
#define MREG_EEV_PWM                    0x0023

/* EEV internal state and flags
 * Pi: R
 */
#define MREG_EEV_STATE                  0x0024
#define MREG_EEV_STARTUP_ACTIVE         0x0025
#define MREG_EEV_FAULT                  0x0026

/* Manual EEV PWM override (only if SYSTEM_MODE=MANUAL)
 * Pi: R/W
 */
#define MREG_MANUAL_EEV_PWM             0x0027

/* EEV limits for auto
 * Pi: R/W
 */
#define MREG_EEV_MIN_PWM                0x0028
#define MREG_EEV_MAX_PWM                0x0029


/* ============================================================
 * HEATER (0x002A..0x002E)
 * ============================================================ */

/* Heater PWM actually applied
 * Pi: R
 */
#define MREG_HEATER_PWM                 0x002A

/* Heater fan state 0/1
 * Pi: R/W in MANUAL, R in AUTO
 */
#define MREG_HEATER_FAN_STATE           0x002B

/* Heater fault code/bitfield
 * Pi: R
 */
#define MREG_HEATER_FAULT               0x002C

/* Max allowed heater temp °C×10
 * Pi: R/W
 */
#define MREG_HEATER_MAX_TEMP            0x002D

/* Manual heater PWM (only in MANUAL mode)
 * Pi: R/W
 */
#define MREG_MANUAL_HEATER_PWM          0x002E


/* ============================================================
 * FANS & FREE COOL (0x002F..0x0030)
 * ============================================================ */

/* Evaporator fan state 0/1
 * Pi: R/W in MANUAL, R in AUTO
 */
#define MREG_COOL_FAN_STATE             0x002F

/* Marker for how free-cooling is used (optional)
 * Pi: R
 */
#define MREG_FREE_COOL_MARKER           0x0030


/* ============================================================
 * CONTROL INTERNALS & MANUAL FAN FLAGS (0x0031..0x003A)
 * ============================================================ */

/* PID internals
 * Pi: R (for tuning/diagnostics)
 */
#define MREG_PID_OUTPUT                 0x0031
#define MREG_PID_ERROR                  0x0032
#define MREG_PID_INTEGRAL               0x0033
#define MREG_PID_DERIVATIVE             0x0034

/* Trajectory and power demand
 * Pi: R (T_TRAJ), R/W (RAMP_RATE if you want to change it)
 */
#define MREG_T_TRAJ                     0x0035  // internal target °C×10
#define MREG_RAMP_RATE                  0x0036  // °C/min ×100
#define MREG_Q_CMD                      0x0037  // signed, "heat/cool power"

/* Operating mode selector:
 * 0 = AUTO_BIDIRECTIONAL (heat + cool)
 * 1 = HEAT_ONLY
 * 2 = COOL_ONLY
 * (Used by mode_manager)
 * old: MREG_RESERVED_STATES
 * Pi: R/W
 */
#define MREG_OPER_MODE                  0x0038

/* Manual heater fans (recirc even if heater off):
 * 0 = off, 1 = on (only used in MANUAL mode)
 * Pi: R/W
 */
#define MREG_MANUAL_HEAT_FANS           0x0039

/* Manual evaporator fans:
 * 0 = off, 1 = on (only used in MANUAL mode)
 * Pi: R/W
 */
#define MREG_MANUAL_COOL_FANS           0x003A


#define MREG_EEV_KP_X10                 0x003B

#define FREECOOL_MIN_DT_X10             0x003C

#define MREG_MANUAL_BYPASS_FLAGS        0x003D

#define MREG_SH_FILT_DIV                0x003E

    #define MREG_EEV_KI_X100                0x003F


/* ============================================================
 * PID & TIMING CONFIG (0x0040..0x0047)
 * ============================================================ */

/* PID tuning
 * Kp/Ki/Kd scaled ×100.
 * Pi: R/W
 */
#define MREG_PID_KP                     0x0040
#define MREG_PID_KI                     0x0041
#define MREG_PID_KD                     0x0042

/* PID sample time in ms (usually 1000)
 * Pi: R/W
 */
#define MREG_PID_SAMPLE_TIME_MS         0x0043

/* Defrost scheduler (if used)
 * Pi: R/W
 */
#define MREG_DEFROST_INTERVAL           0x0044  // minutes
#define MREG_FREE_HEATING_ACTIVE        0x0045  

/* Integral clamp
 * Pi: R/W
 */
#define MREG_PID_I_CLAMP                0x0046

/* Reserved PID/timing
 * Pi: R/W if reused
 */
#define MREG_EEV_MAX_STEP               0x0047

//#define MREG_EEV_KI_X10                 0x0048


/* ============================================================
 * DIAGNOSTICS (0x0050..0x005A)
 * ============================================================ */

#define MREG_DIAG_COMM_ERRORS           0x0050  // Pi: R
#define MREG_DIAG_SENSOR_ERRORS         0x0051  // Pi: R
#define MREG_DIAG_POWER_ON_COUNT        0x0052  // Pi: R
#define MREG_DIAG_LAST_FAULT_CODE       0x0053  // Pi: R
#define MREG_DIAG_LAST_FAULT_TIME_L     0x0054  // Pi: R
#define MREG_DIAG_LAST_FAULT_TIME_H     0x0055  // Pi: R
#define MREG_DIAG_LAST_COMM_TIME_L      0x0056  // Pi: R
#define MREG_DIAG_LAST_COMM_TIME_H      0x0057  // Pi: R
#define MREG_DIAG_CPU_USAGE             0x0058  // %×10, Pi: R
#define MREG_DIAG_TEMP_INTERNAL         0x0059  // MCU temp °C×10, Pi: R
#define MREG_RESERVED_DIAG              0x005A  // Pi: R/W if reused


/* ============================================================
 * EXPANSION / COMPRESSOR-SPECIFIC (0x0080..)
 * ============================================================ */

/* You can use these for DC compressor error decoding, config, etc.
 * Just a suggestion; adjust addresses as needed.
 */

// Start of expansion area
#define MREG_EXPANSION_START            0x0080

/* Sensor-link diagnostics. These count activity at the master USART3 input. */
#define MREG_SENSOR_RX_BYTE_COUNT       0x0080  // Total received bytes, wraps at 65535
#define MREG_SENSOR_RX_LAST_STATUS      0x0081  // USART RXDATAH status for latest byte
#define MREG_SENSOR_RX_LAST_BYTE        0x0082  // Latest received byte
#define MREG_SENSOR_RX_FRAME_LENGTH     0x0083  // Length of latest completed frame
#define MREG_SENSOR_RX_REJECT_REASON    0x0084  // 0 valid/none, 1 short/address, 2 exception, 3 function, 4 byte count, 5 length, 6 CRC

/* Example:
 * DC compressor error flags decoded from LED (via ISR)
 * Pi: R
 */
// #define MREG_DC_COMP_ERROR_FLAGS        0x0080

/* Example:
 * Compressor type: 0 = DC, 1 = AC
 * Pi: R/W
 */
// #define MREG_COMP_TYPE                  0x0081


/* ============================================================
 * SENSOR SLAVE MIRROR (0x0100..0x01FF)
 * ============================================================ */

/* All of these stay as-is, they just mirror the sensor card.
 * Pi should normally read from here rather than probing the sensor card directly.
 */

#define MREG_SREG_FIRMWARE          (SENSOR_BASE + 0x0000)
#define MREG_SREG_UPTIME_L          (SENSOR_BASE + 0x0001)
#define MREG_SREG_UPTIME_H          (SENSOR_BASE + 0x0002)
#define MREG_SREG_STATUS            (SENSOR_BASE + 0x0003)

#define MREG_SREG_SENSOR_ENABLE     (SENSOR_BASE + 0x0004)
#define MREG_SREG_CHAMBER_INTERNAL  (SENSOR_BASE + 0x0005)
#define MREG_SREG_HEATING_ELEMENT   (SENSOR_BASE + 0x0006)
#define MREG_SREG_EVAPORATOR        (SENSOR_BASE + 0x0007)
#define MREG_SREG_SUPERHEAT         (SENSOR_BASE + 0x0008)
#define MREG_SREG_SUBCOOLING        (SENSOR_BASE + 0x0009)

#define MREG_SREG_COMMAND           (SENSOR_BASE + 0x000A)
#define MREG_SREG_LOG_INTERVAL      (SENSOR_BASE + 0x000B)
#define MREG_SREG_EXTERNAL          (SENSOR_BASE + 0x000C)

#define MREG_SREG_NTC1_TEMP         (SENSOR_BASE + 0x000D)
#define MREG_SREG_NTC2_TEMP         (SENSOR_BASE + 0x000E)
#define MREG_SREG_NTC3_TEMP         (SENSOR_BASE + 0x000F)
#define MREG_SREG_NTC4_TEMP         (SENSOR_BASE + 0x0010)
#define MREG_SREG_NTC5_TEMP         (SENSOR_BASE + 0x0011)
#define MREG_SREG_NTC6_TEMP         (SENSOR_BASE + 0x0012)
#define MREG_SREG_NTC7_TEMP         (SENSOR_BASE + 0x0013)
#define MREG_SREG_NTC8_TEMP         (SENSOR_BASE + 0x0014)
#define MREG_SREG_NTC_ERROR_BITS    (SENSOR_BASE + 0x0015)
#define MREG_SREG_KTYPE_TEMP        (SENSOR_BASE + 0x0016)
#define MREG_SREG_KTYPE_CJ_TEMP     (SENSOR_BASE + 0x0017)
#define MREG_SREG_KTYPE_ERROR       (SENSOR_BASE + 0x0018)
#define MREG_SREG_DHT22_TEMP        (SENSOR_BASE + 0x0019)
#define MREG_SREG_DHT22_HUMIDITY    (SENSOR_BASE + 0x001A)
#define MREG_SREG_DHT22_ERROR       (SENSOR_BASE + 0x001B)
#define MREG_SREG_DS18B20_1_TEMP    (SENSOR_BASE + 0x001C)
#define MREG_SREG_DS18B20_2_TEMP    (SENSOR_BASE + 0x001D)
#define MREG_SREG_DS18B20_ERROR     (SENSOR_BASE + 0x001E)
#define MREG_SREG_DS18B20_ERROR2    (SENSOR_BASE + 0x001F)

#define MREG_SREG_RESERVED_START    (SENSOR_BASE + 0x0020)


/* ============================================================
 * BITFIELD DEFINITIONS
 * ============================================================ */

/* MREG_SYS_STATUS (0x0003) */
#define SYS_STATUS_IDLE         (1 << 0)
#define SYS_STATUS_COOLING      (1 << 1)
#define SYS_STATUS_HEATING      (1 << 2)
#define SYS_STATUS_DEFROST      (1 << 3)
#define SYS_STATUS_FAULT_ACTIVE (1 << 4)
#define SYS_STATUS_REMOTE_MODE  (1 << 5)
#define SYS_STATUS_MANUAL_MODE  (1 << 6)
#define SYS_STATUS_SENS_CARD_OK (1 << 7)
#define SYS_STATUS_CONTRL_CARD_OK (1 << 8)

/* MREG_FAULT_FLAGS (0x0005) */
#define FAULT_NTC_ERROR         (1 << 0)
#define FAULT_KTYPE_ERROR       (1 << 1)
#define FAULT_DHT22_ERROR       (1 << 2)
#define FAULT_EEPROM_ERROR      (1 << 3)
#define FAULT_COMM_SENSOR_CARD  (1 << 4)
#define FAULT_OVER_UNDER_TEMP   (1 << 5)
#define FAULT_COIL              (1 << 6)
#define FAULT_EEV_ERROR         (1 << 7)
#define FAULT_AC_HEATER_ERROR   (1 << 8)
#define FAULT_COMP_BATTERY_CUTOUT (1 << 9)
#define FAULT_COMP_FAN_OVER_CURRENT (1 << 10)
#define FAULT_COMP_START_ERROR  (1 << 11)
#define FAULT_COMP_SPEED_ERROR  (1 << 12)
#define FAULT_COMP_THERM_CUTOUT (1 << 13)
#define FAULT_COMP_CONTROL_FAILURE (1 << 14)

/* MREG_ACTUATOR_STATES (0x0007) */
#define ACT_AC_COMPRESSOR       (1 << 0)
#define ACT_AC_CONDENSER_FAN    (1 << 1)
#define ACT_AC_DEFROST          (1 << 2)
#define ACT_RESERVED_3          (1 << 3)
#define ACT_DC_COMPRESSOR       (1 << 4)
#define ACT_DC_GREEN_PILOT      (1 << 5)
#define ACT_DC_ORANGE_PILOT     (1 << 6)
#define ACT_DC_RED_PILOT        (1 << 7)
#define ACT_HEAT_FANS           (1 << 8)
#define ACT_COOL_FANS           (1 << 9)
#define ACT_PWM_HEAT_ELEMENT    (1 << 10)
#define ACT_PWM_COOL_EEV        (1 << 11)

/* Sensor card status bits, errors, etc. */
#define STATUS_POLL_ACTIVE      (1 << 0)
#define STATUS_SYSTEM_READY     (1 << 1)
#define STATUS_ERROR_PRESENT    (1 << 2)
#define STATUS_LOGGING_ACTIVE   (1 << 3)
#define STATUS_RS485_OK         (1 << 4)

#define ENABLE_NTC1             (1 << 0)
#define ENABLE_NTC2             (1 << 1)
#define ENABLE_NTC3             (1 << 2)
#define ENABLE_NTC4             (1 << 3)
#define ENABLE_NTC5             (1 << 4)
#define ENABLE_NTC6             (1 << 5)
#define ENABLE_NTC7             (1 << 6)
#define ENABLE_NTC8             (1 << 7)
#define ENABLE_KTYPE            (1 << 8)
#define ENABLE_DHT22            (1 << 9)
#define ENABLE_DS18B20_1        (1 << 10)
#define ENABLE_DS18B20_2        (1 << 11)

#define NTC_ERR_1               (1 << 0)
#define NTC_ERR_2               (1 << 1)
#define NTC_ERR_3               (1 << 2)
#define NTC_ERR_4               (1 << 3)
#define NTC_ERR_5               (1 << 4)
#define NTC_ERR_6               (1 << 5)
#define NTC_ERR_7               (1 << 6)
#define NTC_ERR_8               (1 << 7)

#define KTYPE_ERR_OPEN          (1 << 0)
#define KTYPE_ERR_SHORT_GND     (1 << 1)
#define KTYPE_ERR_SHORT_VCC     (1 << 2)
#define KTYPE_ERR_SPI_FAIL      (1 << 3)
#define KTYPE_ERR_TEMP_OOR      (1 << 4)

#define DHT22_ERR_CHECKSUM      (1 << 0)
#define DHT22_ERR_TIMEOUT       (1 << 1)
#define DHT22_ERR_NO_RESPONSE   (1 << 2)
#define DHT22_ERR_DATA_INVALID  (1 << 3)

#define DS18B20_1_ERR_NO_RESP   (1 << 0)
#define DS18B20_1_ERR_CRC_FAIL  (1 << 1)
#define DS18B20_1_ERR_CONV_TO   (1 << 2)
#define DS18B20_1_ERR_INVALID   (1 << 3)

#define DS18B20_2_ERR_NO_RESP   (1 << 0)
#define DS18B20_2_ERR_CRC_FAIL  (1 << 1)
#define DS18B20_2_ERR_CONV_TO   (1 << 2)
#define DS18B20_2_ERR_INVALID   (1 << 3)

/* Sensor card commands */
#define CMD_SYS_RESET           (1 << 0)
#define CMD_FORCE_REFRESH       (1 << 1)


#ifdef	__cplusplus
}
#endif
#endif	/* SYSTEM_REGISTERS_H */


// version 2
/* --- Core / Status (0x0000..0x0008) --- */
//#define MREG_FIRMWARE                   0x0000  // Master firmware ×100
//#define MREG_SYS_UPTIME_L               0x0001  // Master uptime sec (low)
//#define MREG_SYS_UPTIME_H               0x0002  // Master uptime sec (high)
//#define MREG_SYS_STATUS                 0x0003  // System state bitfield
//#define MREG_TARGET_TEMP                0x0004  // Current setpoint °C×10
//#define MREG_FAULT_FLAGS                0x0005  // High-level faults bitfield
////#define MREG_MODE_FLAGS                          // Operating mode flags (reserved @0x0006)
//#define MREG_ACTUATOR_STATES            0x0007  // Actuator bits (manual override / status)
//#define MREG_RESERVED_CORE              0x0008  // Reserved
//
///* Alias so your bitfield section name matches: */
////#define MREG_ACTUATOR_STATES            MREG_MANUAL_ACTUATOR_STATES
//
///* --- Setpoints & Derived Temps (0x0009..0x0016) --- */
////#define MREG_SETPOINT_COOL                      // °C×10 (reserved)
////#define MREG_SETPOINT_HEAT                      // °C×10 (reserved)
//#define MREG_SETPOINT_DEFROST           0x0009  // °C×10
//#define MREG_TEMP_HYSTERESIS            0x000A  // °C×10
//#define MREG_CHAMBER_INTERNAL           0x000B  // Mirrored/derived °C×10
//#define MREG_T_HEATING_ELEMENT          0x000C  // °C×10
//#define MREG_EVAPORATOR                 0x000D  // °C×10
//#define MREG_SUPERHEAT                  0x000E  // °C×10
//#define MREG_SUBCOOLING                 0x000F  // °C×10
////#define MREG_CHAMBER_AVG_TEMP                   // Computed avg °C×10 (reserved)
//
//#define MREG_T_MAX_CHAMBER              0x0010  // °C×10
//#define MREG_T_MIN_CHAMBER              0x0011  // °C×10
//
//#define MREG_T_EVAP_IN                  0x0012  // °C×10
//#define MREG_T_EVAP_OUT                 0x0013  // °C×10
//#define MREG_T_EVAP_MIN_ALLOWED         0x0014  // °C×10
//#define MREG_SH_TARGET                  0x0015  // °C×10
//
//#define MREG_RESERVED_SETPOINTS         0x0016  // Reserved
//
///* --- System Modes / Compressor (0x0017..0x001F) --- */
//#define MREG_SYSTEM_MODE                0x0017  // 1 Manual, 0 Auto
//#define MREG_SYS_STATE                  0x0018  // MODE_MANUAL, MODE_HEAT, MODE_IDLE, ...
//
//#define MREG_HEATING_ACTIVE             0x0019  // 0/1
//#define MREG_COOLING_ACTIVE             0x001A  // 0/1
//#define MREG_FREE_COOL_ACTIVE           0x001B  // 0/1
//
//#define MREG_COMPRESSOR_STATE           0x001C  // 0/1 (or enum)
//#define MREG_COMPRESSOR_ALLOWED         0x001D  // 0/1
//#define MREG_COMP_MIN_ON_TIME           0x001E  // seconds
//#define MREG_COMP_MIN_OFF_TIME          0x001F  // seconds
//
///* --- Safety (0x0020..0x0022) --- */
//#define MREG_SAFETY_ACTIVE              0x0020  // 0/1
//#define MREG_SAFETY_CODE                0x0021  // SAFE_*
//#define MREG_SAFETY_LATCHED             0x0022  // 0/1
//
///* --- EEV (0x0023..0x0029) --- */
//#define MREG_EEV_PWM                    0x0023  // 0..1000 (or %×10)
//#define MREG_EEV_STATE                  0x0024  // enum/state
//#define MREG_EEV_STARTUP_ACTIVE         0x0025  // 0/1
//#define MREG_EEV_FAULT                  0x0026  // bitfield / code
//#define MREG_MANUAL_EEV_PWM             0x0027  // manual override
//#define MREG_EEV_MIN_PWM                0x0028
//#define MREG_EEV_MAX_PWM                0x0029
//
///* --- Heater (0x002A..0x002E) --- */
//#define MREG_HEATER_PWM                 0x002A  // 0..1000 (or %×10)
//#define MREG_HEATER_FAN_STATE           0x002B  // 0/1
//#define MREG_HEATER_FAULT               0x002C  // bitfield / code
//#define MREG_HEATER_MAX_TEMP            0x002D  // °C×10
//#define MREG_MANUAL_HEATER_PWM          0x002E  // manual override
//
///* --- Fans / Markers (0x002F..0x0030) --- */
//#define MREG_COOL_FAN_STATE             0x002F  // 0/1
//#define MREG_FREE_COOL_MARKER           0x0030  // 0/1 (or enum)
//
///* --- Control States (0x0031..0x0038) --- */ // not used anymore, you have had better registers i already implimented
////#define MREG_PWM_HEAT_FAN                       // %×10 (reserved)
////#define MREG_PWM_COOL_FAN                       // %×10 (reserved)
////#define MREG_PWM_HEATBED1                       // %×10 (reserved)
////#define MREG_PWM_HEATBED2                       // %×10 (reserved)
////#define MREG_PWM_AC_EEV                         // %×10 (reserved)
////#define MREG_PWM_HEATER_ELEMENT                 // %×10 (reserved)
//
//#define MREG_PID_OUTPUT                 0x0031  // Current PID value
//#define MREG_PID_ERROR                  0x0032  // Current error
//#define MREG_PID_INTEGRAL               0x0033  // Integral term
//#define MREG_PID_DERIVATIVE             0x0034  // Derivative term
//
//#define MREG_T_TRAJ                     0x0035
//#define MREG_RAMP_RATE                  0x0036
//#define MREG_Q_CMD                      0x0037
//
//#define MREG_RESERVED_STATES            0x0038  // Reserved
//
///* --- PID & Timing (0x0040..0x0047) --- */
//#define MREG_PID_KP                     0x0040  // Scaled ×100
//#define MREG_PID_KI                     0x0041
//#define MREG_PID_KD                     0x0042
//#define MREG_PID_SAMPLE_TIME_MS         0x0043  // ms
//#define MREG_DEFROST_INTERVAL           0x0044  // min
//#define MREG_DEFROST_DURATION           0x0045  // min
//#define MREG_PID_I_CLAMP                0x0046
//#define MREG_RESERVED_PID               0x0047  // Reserved
//
///* --- Diagnostics (0x0050..0x005A) --- */
//#define MREG_DIAG_COMM_ERRORS           0x0050
//#define MREG_DIAG_SENSOR_ERRORS         0x0051
//#define MREG_DIAG_POWER_ON_COUNT        0x0052
//#define MREG_DIAG_LAST_FAULT_CODE       0x0053
//#define MREG_DIAG_LAST_FAULT_TIME_L     0x0054
//#define MREG_DIAG_LAST_FAULT_TIME_H     0x0055
//#define MREG_DIAG_LAST_COMM_TIME_L      0x0056
//#define MREG_DIAG_LAST_COMM_TIME_H      0x0057
//#define MREG_DIAG_CPU_USAGE             0x0058  // %×10
//#define MREG_DIAG_TEMP_INTERNAL         0x0059  // MCU °C×10
//#define MREG_RESERVED_DIAG              0x005A  // Reserved
//
///* ============================================================
// * EXPANSION (keep below SENSOR_BASE=0x0100)
// * ============================================================ */
////#define MREG_EXPANSION_START            0x0080
//
///* ============================================================
// * SENSOR SLAVE MIRROR (0x0100?0x01FF): Exact copy
// * ============================================================ */
//#define MREG_SREG_FIRMWARE          (SENSOR_BASE + 0x0000)
//#define MREG_SREG_UPTIME_L          (SENSOR_BASE + 0x0001)
//#define MREG_SREG_UPTIME_H          (SENSOR_BASE + 0x0002)
//#define MREG_SREG_STATUS            (SENSOR_BASE + 0x0003)
//
//#define MREG_SREG_SENSOR_ENABLE     (SENSOR_BASE + 0x0004)
//#define MREG_SREG_CHAMBER_INTERNAL  (SENSOR_BASE + 0x0005) // main chamber temp
//#define MREG_SREG_HEATING_ELEMENT   (SENSOR_BASE + 0x0006)
//#define MREG_SREG_EVAPORATOR        (SENSOR_BASE + 0x0007)
//#define MREG_SREG_SUPERHEAT         (SENSOR_BASE + 0x0008)
//#define MREG_SREG_SUBCOOLING        (SENSOR_BASE + 0x0009)
//
//#define MREG_SREG_COMMAND           (SENSOR_BASE + 0x000A)
//#define MREG_SREG_LOG_INTERVAL      (SENSOR_BASE + 0x000B)
//#define MREG_SREG_SYSTEM_RESERVED   (SENSOR_BASE + 0x000C)
//
//#define MREG_SREG_NTC1_TEMP         (SENSOR_BASE + 0x000D)
//#define MREG_SREG_NTC2_TEMP         (SENSOR_BASE + 0x000E)
//#define MREG_SREG_NTC3_TEMP         (SENSOR_BASE + 0x000F)
//#define MREG_SREG_NTC4_TEMP         (SENSOR_BASE + 0x0010) // 1-4 averaged for chamber internal
//#define MREG_SREG_NTC5_TEMP         (SENSOR_BASE + 0x0011) // evap in
//#define MREG_SREG_NTC6_TEMP         (SENSOR_BASE + 0x0012) // evap out // for the superhead
//#define MREG_SREG_NTC7_TEMP         (SENSOR_BASE + 0x0013) // condensor in
//#define MREG_SREG_NTC8_TEMP         (SENSOR_BASE + 0x0014) // condensor out // for subcooling
//#define MREG_SREG_NTC_ERROR_BITS    (SENSOR_BASE + 0x0015)
//#define MREG_SREG_KTYPE_TEMP        (SENSOR_BASE + 0x0016) // heating element
//#define MREG_SREG_KTYPE_CJ_TEMP     (SENSOR_BASE + 0x0017) // pcb temp
//#define MREG_SREG_KTYPE_ERROR       (SENSOR_BASE + 0x0018)
//#define MREG_SREG_DHT22_TEMP        (SENSOR_BASE + 0x0019)
//#define MREG_SREG_DHT22_HUMIDITY    (SENSOR_BASE + 0x001A)
//#define MREG_SREG_DHT22_ERROR       (SENSOR_BASE + 0x001B)
//#define MREG_SREG_DS18B20_1_TEMP    (SENSOR_BASE + 0x001C) // also chamber internal, or I can place it outside for an evironment temp (as a reference)
//#define MREG_SREG_DS18B20_2_TEMP    (SENSOR_BASE + 0x001D) // evaporator
//#define MREG_SREG_DS18B20_ERROR     (SENSOR_BASE + 0x001E)
//#define MREG_SREG_DS18B20_ERROR2    (SENSOR_BASE + 0x001F)
//
//#define MREG_SREG_RESERVED_START    (SENSOR_BASE + 0x0020)
//
///* ============================================================
// * BITFIELD DEFINITIONS
// * ============================================================ */
//
///* --- MREG_SYS_STATUS (0x0003) --- */
//#define SYS_STATUS_IDLE         (1 << 0)
//#define SYS_STATUS_COOLING      (1 << 1)
//#define SYS_STATUS_HEATING      (1 << 2)
//#define SYS_STATUS_DEFROST      (1 << 3)
//#define SYS_STATUS_FAULT_ACTIVE (1 << 4)
//#define SYS_STATUS_REMOTE_MODE  (1 << 5)
//#define SYS_STATUS_MANUAL_MODE  (1 << 6)
//#define SYS_STATUS_SENS_CARD_OK (1 << 7)
//#define SYS_STATUS_CONTRL_CARD_OK (1 << 8)
//
///* --- MREG_FAULT_FLAGS (0x0005) --- */
//#define FAULT_NTC_ERROR         (1 << 0)
//#define FAULT_KTYPE_ERROR       (1 << 1)
//#define FAULT_DHT22_ERROR       (1 << 2)
//#define FAULT_EEPROM_ERROR      (1 << 3)
//#define FAULT_COMM_SENSOR_CARD  (1 << 4)
//#define FAULT_OVER_UNDER_TEMP   (1 << 5)
//#define FAULT_COIL              (1 << 6)
//#define FAULT_EEV_ERROR         (1 << 7)
//#define FAULT_AC_HEATER_ERROR   (1 << 8)
//#define FAULT_COMP_BATTERY_CUTOUT (1 << 9)
//#define FAULT_COMP_FAN_OVER_CURRENT (1 << 10)
//#define FAULT_COMP_START_ERROR  (1 << 11)
//#define FAULT_COMP_SPEED_ERROR  (1 << 12)
//#define FAULT_COMP_THERM_CUTOUT (1 << 13)
//#define FAULT_COMP_CONTROL_FAILURE (1 << 14)
//
///* --- MREG_ACTUATOR_STATES (0x0007) --- */
//#define ACT_AC_COMPRESSOR       (1 << 0) //112
//#define ACT_AC_CONDENSER_FAN    (1 << 1) //113
//#define ACT_AC_DEFROST          (1 << 2) //114
//#define ACT_RESERVED_3          (1 << 3) //115
//#define ACT_DC_COMPRESSOR       (1 << 4) //116
//#define ACT_DC_GREEN_PILOT      (1 << 5) //117
//#define ACT_DC_ORANGE_PILOT     (1 << 6) //118
//#define ACT_DC_RED_PILOT        (1 << 7) //119
//#define ACT_HEAT_FANS       (1 << 8) //120
//#define ACT_COOL_FANS       (1 << 9) //121
//#define ACT_PWM_HEAT_ELEMENT    (1 << 10)//122
//#define ACT_PWM_COOL_EEV        (1 << 11)//123
//
///* --- MREG_MODE_FLAGS (0x0006) --- */ // not used anymore
////#define MODE_HEATING            (1 << 0) //96
////#define MODE_COOLING            (1 << 1) //97
////#define MODE_DEFROST            (1 << 2)//98
////#define MODE_AUTOMATIC          (1 << 3)//99
////#define MODE_MANUAL             (1 << 4)//100
////#define MODE_STANDBY            (1 << 5)//101
////#define MODE_REMOTE_CONTROL     (1 << 6)//102
//
////--------- SERSOR SLAVE REGISTERS ---------------------------------
//
///* --- MREG_SREG_STATUS (SENSOR_BASE + 0x0003) --- */
//#define STATUS_POLL_ACTIVE      (1 << 0)
//#define STATUS_SYSTEM_READY     (1 << 1)
//#define STATUS_ERROR_PRESENT    (1 << 2)
//#define STATUS_LOGGING_ACTIVE   (1 << 3)
//#define STATUS_RS485_OK         (1 << 4)
//
///* --- MREG_SREG_SENSOR_ENABLE (SENSOR_BASE + 0x0004) --- */
//#define ENABLE_NTC1             (1 << 0)
//#define ENABLE_NTC2             (1 << 1)
//#define ENABLE_NTC3             (1 << 2)
//#define ENABLE_NTC4             (1 << 3)
//#define ENABLE_NTC5             (1 << 4)
//#define ENABLE_NTC6             (1 << 5)
//#define ENABLE_NTC7             (1 << 6)
//#define ENABLE_NTC8             (1 << 7)
//#define ENABLE_KTYPE            (1 << 8)
//#define ENABLE_DHT22            (1 << 9)
//#define ENABLE_DS18B20_1        (1 << 10)
//#define ENABLE_DS18B20_2        (1 << 11)
//
///* --- MREG_SREG_NTC_ERROR_BITS (SENSOR_BASE + 0x0015) --- */
//#define NTC_ERR_1               (1 << 0)
//#define NTC_ERR_2               (1 << 1)
//#define NTC_ERR_3               (1 << 2)
//#define NTC_ERR_4               (1 << 3)
//#define NTC_ERR_5               (1 << 4)
//#define NTC_ERR_6               (1 << 5)
//#define NTC_ERR_7               (1 << 6)
//#define NTC_ERR_8               (1 << 7)
//
///* --- MREG_SREG_KTYPE_ERROR (SENSOR_BASE + 0x0018) --- */
//#define KTYPE_ERR_OPEN          (1 << 0)
//#define KTYPE_ERR_SHORT_GND     (1 << 1)
//#define KTYPE_ERR_SHORT_VCC     (1 << 2)
//#define KTYPE_ERR_SPI_FAIL      (1 << 3)
//#define KTYPE_ERR_TEMP_OOR      (1 << 4)
//
///* --- MREG_SREG_DHT22_ERROR (SENSOR_BASE + 0x001B) --- */
//#define DHT22_ERR_CHECKSUM      (1 << 0)
//#define DHT22_ERR_TIMEOUT       (1 << 1)
//#define DHT22_ERR_NO_RESPONSE   (1 << 2)
//#define DHT22_ERR_DATA_INVALID  (1 << 3)
//
///* --- MREG_SREG_DS18B20_ERROR (SENSOR_BASE + 0x001E) --- */
//#define DS18B20_1_ERR_NO_RESP   (1 << 0)
//#define DS18B20_1_ERR_CRC_FAIL  (1 << 1)
//#define DS18B20_1_ERR_CONV_TO   (1 << 2)
//#define DS18B20_1_ERR_INVALID   (1 << 3)
//
///* --- MREG_SREG_DS18B20_ERROR (SENSOR_BASE + 0x001F) --- */
//#define DS18B20_2_ERR_NO_RESP   (1 << 0)
//#define DS18B20_2_ERR_CRC_FAIL  (1 << 1)
//#define DS18B20_2_ERR_CONV_TO   (1 << 2)
//#define DS18B20_2_ERR_INVALID   (1 << 3)
//
///* --- MREG_SREG_COMMAND (SENSOR_BASE + 0x000A) --- */
//#define CMD_SYS_RESET           (1 << 0)
//#define CMD_FORCE_REFRESH       (1 << 1)
///////////////////////////////////////////////
