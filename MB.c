
/*
 * Modbus Implementation
 * Ruhan Louw
 *
 * PI and SENSOR cards
 */

#include <string.h>
#include "system_registers.h"
#include "MB.h"
#include "Functions_Lib/RTC/debug_uart.h"
#include <util/atomic.h>
#include <util/crc16.h>
#include <util/delay.h>

/*========================================
 *  VARS
 =========================================*/

/* Frame state flags */
static volatile bool _FULL_FRAME_RECEIVED[2] = { false, false };
static volatile bool _RX_RUNNING[2]          = { false, false };

/*
 * Double buffers:
 *  _FRAME_BUF[port][0] = RX buffer
 *  _FRAME_BUF[port][1] = PROC (processing) buffer
 */
static uint8_t  _FRAME_BUF[2][2][MAX_FRAME_LENGTH] = {0};

/* Pointers to active RX and PROC buffers per port */
static uint8_t* _FRAME_RX[2]   = { 0 };
static uint8_t* _FRAME_PROC[2] = { 0 };

/* RX index into current RX buffer, and length of PROC buffer */
static volatile uint8_t _FRAME_RX_INDEX[2] = { 0 };
static volatile uint8_t _FRAME_PROC_LEN[2] = { 0 };

volatile bool _POLL_SENSOR = false;

/* Sensor's Master state */
static volatile bool   sensor_request_pending   = false;
static volatile bool   sensor_response_received = false;
static volatile bool   sensor_comm_failure_pending = false;
static volatile bool   sensor_tx_active         = false;
static volatile uint8_t sensor_fc               = 0;
static volatile uint8_t sensor_timeout_count    = 0;
static uint8_t sensor_tx_buffer[MAX_FRAME_LENGTH];
static uint16_t sensor_tx_length = 0U;
static uint16_t sensor_tx_index  = 0U;
static uint8_t slave_response_port = PI;

/* An RTU response is expected well within the next one-second system tick. */
#define SENSOR_TIMEOUT_SECONDS  1U

#if SENSOR_LINK_TTL_DEBUG_TEST
#define SENSOR_USART USART1
#else
#define SENSOR_USART USART3
#endif

/*========================================
 *  STATIC FUNCTIONS
 =========================================*/

/*-------------- RS485 Direction ---------------------------*/
static inline void PI_TX_EN(bool enable)
{
    if (enable) {
        DE_PI_PORT.OUTSET = (1 << DE_PI_PIN);
        _delay_us(1);
    } else {
        _delay_us(1);
        DE_PI_PORT.OUTCLR = (1 << DE_PI_PIN);
    }
}

static inline void SENSOR_TX_EN(bool enable)
{
#if SENSOR_LINK_TTL_DEBUG_TEST
    (void)enable;
    DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN);
#else
    if (enable) {
        DE_SENSOR_PORT.OUTSET = (1 << DE_SENSOR_PIN);
        _delay_us(1);
    } else {
        _delay_us(1);
        DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN);
    }
#endif
}

/*-------------------- CRC16 ------------------------------*/
static uint16_t CRC16(const uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; ++i) {
        crc = _crc16_update(crc, buf[i]);
    }
    return crc;
}

/*========================================
 *  RX / TX INTERRUPT CALLBACK FUNCTIONS
 =========================================*/

/* ======== UART Interrupts =========*/
void PI_RX_RECEIVE(void)
{
    uint8_t idx = _FRAME_RX_INDEX[PI];

    if (idx >= MAX_FRAME_LENGTH) {
        /* Overflow for this frame: read and drop byte */
        (void)UART0_Read();
        return;
    }

    LED_RX_PI_ON();

    _FRAME_RX[PI][idx] = UART0_Read();
    _FRAME_RX_INDEX[PI] = idx + 1;

    if (!_RX_RUNNING[PI]) {
        _RX_RUNNING[PI] = true;
        PI_RX_TIMEOUT_Start();
    }
    PI_RX_TIMEOUT_CounterSet(0);
}

void PI_TX_COMPLETE(void)
{
    LED_TX_PI_OFF();
    PI_TX_EN(false);
}

static void sensor_rx_byte(uint8_t received_byte)
{
    uint8_t idx = _FRAME_RX_INDEX[SENSOR];

    if (idx >= MAX_FRAME_LENGTH) {
        return;
    }

    LED_RX_SENSOR_ON();

    _FRAME_RX[SENSOR][idx] = received_byte;
    _FRAME_RX_INDEX[SENSOR] = idx + 1;

    if (!_RX_RUNNING[SENSOR]) {
        _RX_RUNNING[SENSOR] = true;
        SENSOR_RX_TIMEOUT_Start();
    }
    SENSOR_RX_TIMEOUT_CounterSet(0);
}

void SENSOR_RX_RECEIVE(void)
{
#if SENSOR_LINK_TTL_DEBUG_TEST
    uint8_t status = SENSOR_USART.RXDATAH;
    uint8_t received_byte = SENSOR_USART.RXDATAL;
#else
    uint8_t status = (uint8_t)UART3_ErrorGet();
    uint8_t received_byte = UART3_Read();
#endif

    REG(MREG_SENSOR_RX_BYTE_COUNT)++;
    REG(MREG_SENSOR_RX_LAST_STATUS) = status;
    REG(MREG_SENSOR_RX_LAST_BYTE) = received_byte;
    LED_RX_SENSOR_ON();
    if ((status & (USART_FERR_bm | USART_PERR_bm | USART_BUFOVF_bm)) != 0U) {
        sensor_comm_failure_pending = true;
    }

    sensor_rx_byte(received_byte);
}

void SENSOR_TX_COMPLETE(void)
{
    sensor_tx_active = false;
    SENSOR_TX_EN(false);
    LED_TX_SENSOR_OFF();
}

static void sensor_tx_service(void)
{
    if (!sensor_tx_active) {
        return;
    }

#if SENSOR_LINK_TTL_DEBUG_TEST
    if (sensor_tx_index < sensor_tx_length) {
        if ((SENSOR_USART.STATUS & USART_DREIF_bm) != 0U) {
            SENSOR_USART.TXDATAL = sensor_tx_buffer[sensor_tx_index++];
        }
        return;
    }

    if ((SENSOR_USART.STATUS & USART_TXCIF_bm) != 0U) {
        SENSOR_USART.STATUS = USART_TXCIF_bm;
        SENSOR_TX_COMPLETE();
    }
#else
    bool transmit_ok = true;

    /* Send the complete RTU frame in one call so the main loop cannot add gaps. */
    while (sensor_tx_index < sensor_tx_length) {
        uint16_t dre_guard = 3000U;
        while (((SENSOR_USART.STATUS & USART_DREIF_bm) == 0U) &&
               (dre_guard > 0U)) {
            dre_guard--;
            _delay_us(1);
        }
        if (dre_guard == 0U) {
            transmit_ok = false;
            break;
        }
        SENSOR_USART.TXDATAL = sensor_tx_buffer[sensor_tx_index++];
    }

    /* Wait for the final stop bit, then return the transceiver to receive. */
    uint16_t completion_guard = 3000U;
    while (((SENSOR_USART.STATUS & USART_TXCIF_bm) == 0U) &&
           (completion_guard > 0U)) {
        completion_guard--;
        _delay_us(1);
    }

    if ((SENSOR_USART.STATUS & USART_TXCIF_bm) != 0U) {
        SENSOR_USART.STATUS = USART_TXCIF_bm;
    } else {
        transmit_ok = false;
    }
    if (!transmit_ok) {
        sensor_comm_failure_pending = true;
    }
    SENSOR_TX_COMPLETE();
    SENSOR_USART.CTRLB |= USART_RXEN_bm;
    SENSOR_USART.CTRLA |= USART_RXCIE_bm;
#endif
}

/* ======== Timer Interrupts =========*/

/* PI frame complete (TCB0 callback) */
void PI_FRAME_COMPLETE(void)
{
    /* If previous frame not yet processed, drop this one to avoid overwriting */
    if (_FULL_FRAME_RECEIVED[PI]) {
        _FRAME_RX_INDEX[PI] = 0;
        _RX_RUNNING[PI]     = false;
        LED_RX_PI_OFF();
        PI_RX_TIMEOUT_Stop();
        return;
    }

    /* Capture length of just-finished frame */
    _FRAME_PROC_LEN[PI] = _FRAME_RX_INDEX[PI];

    /* Swap RX and PROC buffers */
    uint8_t* tmp   = _FRAME_PROC[PI];
    _FRAME_PROC[PI] = _FRAME_RX[PI];
    _FRAME_RX[PI]   = tmp;

    /* Reset RX index for next frame */
    _FRAME_RX_INDEX[PI] = 0;

    _FULL_FRAME_RECEIVED[PI] = true;
    _RX_RUNNING[PI]          = false;

    LED_RX_PI_OFF();
    PI_RX_TIMEOUT_Stop();
}

/* SENSOR frame complete (TCB1 callback) */
void SENSOR_FRAME_COMPLETE(void)
{
    if (_FULL_FRAME_RECEIVED[SENSOR]) {
        /* Still busy with the previous frame: abort this transaction. */
        _FRAME_RX_INDEX[SENSOR] = 0;
        _RX_RUNNING[SENSOR]     = false;
        sensor_request_pending   = false;
        sensor_response_received = false;
        sensor_comm_failure_pending = true;
        LED_TX_SENSOR_OFF();
        LED_RX_SENSOR_OFF();
        SENSOR_RX_TIMEOUT_Stop();
        return;
    }

    _FRAME_PROC_LEN[SENSOR] = _FRAME_RX_INDEX[SENSOR];
    REG(MREG_SENSOR_RX_FRAME_LENGTH) = _FRAME_RX_INDEX[SENSOR];

    uint8_t* tmp      = _FRAME_PROC[SENSOR];
    _FRAME_PROC[SENSOR] = _FRAME_RX[SENSOR];
    _FRAME_RX[SENSOR]   = tmp;

    _FRAME_RX_INDEX[SENSOR] = 0;

    _FULL_FRAME_RECEIVED[SENSOR] = true;
#if SENSOR_PORT_PI_SLAVE_TEST
    sensor_response_received     = false;
#else
    sensor_response_received     = true;
    sensor_request_pending       = false;
#endif
    _RX_RUNNING[SENSOR]          = false;

    LED_TX_SENSOR_OFF();
    LED_RX_SENSOR_OFF();
    SENSOR_RX_TIMEOUT_Stop();
    // printf("%d\n", (int)_FRAME_PROC_LEN[SENSOR]);
}

/* Slave response helper shared by the normal and diagnostic RS485 ports. */
static void SLAVE_TX_SEND(const uint8_t* data, uint16_t len)
{
    if (len > MAX_FRAME_LENGTH) return;

    if (slave_response_port == SENSOR) {
        LED_TX_SENSOR_ON();
        SENSOR_TX_EN(true);
        for (uint16_t i = 0; i < len; i++) {
            UART3_Write(data[i]);
        }
    } else {
        LED_TX_PI_ON();
        PI_TX_EN(true);
        for (uint16_t i = 0; i < len; i++) {
            UART0_Write(data[i]);
        }
    }

//    printf("pi send: ");
//    for (uint16_t i = 0; i < len; i++) {
//        printf("%02X ", data[i]);
//    }
//    printf("\n");
}

static void increment_comm_error_count(void)
{
    uint16_t count = (uint16_t)REG(MREG_DIAG_COMM_ERRORS);
    if (count < UINT16_MAX) {
        REG(MREG_DIAG_COMM_ERRORS) = (uint16_t)(count + 1U);
    }
}

#if !SENSOR_PORT_PI_SLAVE_TEST
static void sensor_comm_failed(void)
{
    REG(MREG_FAULT_FLAGS) |= FAULT_COMM_SENSOR_CARD;
    REG(MREG_SYS_STATUS) &= (uint16_t)~SYS_STATUS_SENS_CARD_OK;
    increment_comm_error_count();
}

static void sensor_comm_succeeded(void)
{
    REG(MREG_FAULT_FLAGS) &= (uint16_t)~FAULT_COMM_SENSOR_CARD;
    REG(MREG_SYS_STATUS) |= SYS_STATUS_SENS_CARD_OK;
}

static void update_sensor_faults(void)
{
    uint16_t enabled = (uint16_t)REG(MREG_SREG_SENSOR_ENABLE);
    uint16_t faults = (uint16_t)REG(MREG_FAULT_FLAGS);
    uint16_t diag = 0U;

    bool ntc_error = (((uint16_t)REG(MREG_SREG_NTC_ERROR_BITS) &
                       enabled & 0x00FFU) != 0U);
    bool ds_error = (((enabled & ENABLE_DS18B20_1) != 0U) &&
                     (REG(MREG_SREG_DS18B20_ERROR) != 0)) ||
                    (((enabled & ENABLE_DS18B20_2) != 0U) &&
                     (REG(MREG_SREG_DS18B20_ERROR2) != 0));
    bool ktype_error = (((enabled & ENABLE_KTYPE) != 0U) &&
                        (REG(MREG_SREG_KTYPE_ERROR) != 0));
    bool dht_error = (((enabled & ENABLE_DHT22) != 0U) &&
                      (REG(MREG_SREG_DHT22_ERROR) != 0));

    faults &= (uint16_t)~(FAULT_NTC_ERROR | FAULT_KTYPE_ERROR | FAULT_DHT22_ERROR);
    if (ntc_error || ds_error) {
        faults |= FAULT_NTC_ERROR;
    }
    if (ktype_error) {
        faults |= FAULT_KTYPE_ERROR;
    }
    if (dht_error) {
        faults |= FAULT_DHT22_ERROR;
    }
    REG(MREG_FAULT_FLAGS) = faults;

    if (ntc_error)   diag |= 0x0001U;
    if (ktype_error) diag |= 0x0002U;
    if (dht_error)   diag |= 0x0004U;
    if (ds_error)    diag |= 0x0008U;
    REG(MREG_DIAG_SENSOR_ERRORS) |= diag;
}
#endif

static void pi_send_exception(uint8_t function, uint8_t exception)
{
    uint8_t resp[5] = { SLAVE_ID_PI, (uint8_t)(function | 0x80U), exception, 0U, 0U };
    uint16_t crc = CRC16(resp, 3U);
    resp[3] = (uint8_t)(crc & 0xFFU);
    resp[4] = (uint8_t)(crc >> 8);
    SLAVE_TX_SEND(resp, sizeof(resp));
}

static uint8_t validate_pi_register_write(uint16_t addr, uint16_t value)
{
    switch (addr) {
        case MREG_SP_DEFROST:
        case MREG_T_MAX_CHAMBER:
        case MREG_T_MIN_CHAMBER:
        case MREG_T_EVAP_MIN_ALLOWED:
        case MREG_EEV_KI_X10:
        case MREG_COMP_MIN_ON_TIME:
        case MREG_COMP_MIN_OFF_TIME:
        case MREG_HEATER_MAX_TEMP:
        case MREG_PID_SAMPLE_TIME_MS:
        case MREG_DEFROST_INTERVAL:
            return 0U;

        case MREG_T_SP_MAIN:
        {
            int16_t setpoint = (int16_t)value;
            int16_t minimum = (int16_t)REG(MREG_T_MIN_CHAMBER);
            int16_t maximum = (int16_t)REG(MREG_T_MAX_CHAMBER);
            return ((maximum > minimum) &&
                    ((setpoint < minimum) || (setpoint > maximum))) ? 3U : 0U;
        }

        case MREG_SH_TARGET:
            return (value <= 600U) ? 0U : 3U;

        case MREG_TEMP_HYSTERESIS:
        case MREG_EEV_KP_X10:
        case MREG_EEV_KI_X100:
        case MREG_PID_KP:
        case MREG_PID_KI:
        case MREG_PID_KD:
        case MREG_RAMP_RATE:
        case FREECOOL_MIN_DT_X10:
            return (value <= INT16_MAX) ? 0U : 3U;

        case MREG_PID_I_CLAMP:
            return (value <= 1000U) ? 0U : 3U;

        case MREG_SYSTEM_MODE:
            return (value <= 1U) ? 0U : 3U;

        case MREG_SAFETY_LATCHED:
            return (value == 0U) ? 0U : 3U;

        case MREG_MANUAL_EEV_PWM:
        case MREG_EEV_MIN_PWM:
        case MREG_EEV_MAX_PWM:
            return (value <= 100U) ? 0U : 3U;

        case MREG_MANUAL_HEATER_PWM:
            return (value <= 1000U) ? 0U : 3U;

        case MREG_HEATER_FAN_STATE:
        case MREG_COOL_FAN_STATE:
        case MREG_MANUAL_HEAT_FANS:
        case MREG_MANUAL_COOL_FANS:
        case MREG_MANUAL_BYPASS_FLAGS:
            return (value <= 1U) ? 0U : 3U;

        case MREG_OPER_MODE:
            return (value <= 2U) ? 0U : 3U;

        case MREG_SH_FILT_DIV:
            return ((value >= 1U) && (value <= 50U)) ? 0U : 3U;

        case MREG_EEV_MAX_STEP:
            return ((value >= 1U) && (value <= 100U)) ? 0U : 3U;

        default:
            return 2U;
    }
}

/* Called from the one-second RTC tick before the next poll is requested. */
void MB_SensorTick1s(void)
{
#if SENSOR_PORT_PI_SLAVE_TEST
    return;
#else
    if (!sensor_request_pending) {
        sensor_timeout_count = 0;
        return;
    }

    sensor_timeout_count++;

    if (sensor_timeout_count >= SENSOR_TIMEOUT_SECONDS) {
        /* Hard timeout: abort request */
        sensor_request_pending   = false;
        sensor_response_received = false;
        _FRAME_RX_INDEX[SENSOR]  = 0;
        _FULL_FRAME_RECEIVED[SENSOR] = false;
        sensor_timeout_count     = 0;
        sensor_comm_failure_pending = true;
        sensor_tx_active         = false;
        sensor_tx_length         = 0U;
        sensor_tx_index          = 0U;

        SENSOR_TX_EN(false);
        LED_TX_SENSOR_OFF();
    }
#endif
}

/*========================================
 *  MODBUS INIT Function
 ========================================= */

void MB_INIT(void)
{
    PI_RX_TIMEOUT_Stop();
    SENSOR_RX_TIMEOUT_Stop();
    SENSOR_REQUEST_TIMEOUT_Stop();

    PI_RX_TIMEOUT_CaptureCallbackRegister(PI_FRAME_COMPLETE);
    SENSOR_RX_TIMEOUT_CaptureCallbackRegister(SENSOR_FRAME_COMPLETE);
#if SENSOR_PORT_PI_SLAVE_TEST
    SENSOR_RX_TIMEOUT_PeriodSet(PI_RX_TIMEOUT_PeriodGet());
#endif

    UART0_RxCompleteCallbackRegister(PI_RX_RECEIVE);
    UART0_TxCompleteCallbackRegister(PI_TX_COMPLETE);
#if !SENSOR_LINK_TTL_DEBUG_TEST
    /* Use the same generated-driver receive path as the working Pi port. */
    UART3_RxCompleteCallbackRegister(SENSOR_RX_RECEIVE);
#endif

#if SENSOR_PORT_PI_SLAVE_TEST
    UART3_TxCompleteCallbackRegister(SENSOR_TX_COMPLETE);
#elif !SENSOR_LINK_TTL_DEBUG_TEST
    /* The sensor master path writes USART3 directly and polls completion. */
    SENSOR_USART.CTRLA &= (uint8_t)~(USART_DREIE_bm | USART_TXCIE_bm | USART_RXSIE_bm);
    SENSOR_USART.CTRLA |= USART_RXCIE_bm;
    SENSOR_USART.CTRLB |= USART_RXEN_bm;
    SENSOR_USART.STATUS = USART_TXCIF_bm;
#else
    SENSOR_USART.CTRLA &= (uint8_t)~(USART_DREIE_bm | USART_TXCIE_bm);
    SENSOR_USART.STATUS = USART_TXCIF_bm;
#endif

    /* Double-buffer pointer init */
    _FRAME_RX[PI]     = _FRAME_BUF[PI][0];
    _FRAME_PROC[PI]   = _FRAME_BUF[PI][1];
    _FRAME_RX[SENSOR]   = _FRAME_BUF[SENSOR][0];
    _FRAME_PROC[SENSOR] = _FRAME_BUF[SENSOR][1];

    _FRAME_RX_INDEX[PI]     = 0;
    _FRAME_RX_INDEX[SENSOR] = 0;
    _FRAME_PROC_LEN[PI]     = 0;
    _FRAME_PROC_LEN[SENSOR] = 0;

    _FULL_FRAME_RECEIVED[PI]     = false;
    _FULL_FRAME_RECEIVED[SENSOR] = false;
    _RX_RUNNING[PI]              = false;
    _RX_RUNNING[SENSOR]          = false;
    sensor_comm_failure_pending  = false;
    sensor_tx_active             = false;
    sensor_tx_length             = 0U;
    sensor_tx_index              = 0U;
}

/*========================================
 *  FUNCTION CODES (Pi Slave)
 =========================================*/

static bool fc03_read_holding(uint8_t* f, uint16_t len)
{
    if (len != 8U) {
        pi_send_exception(0x03U, 0x03U);
        return true;
    }

    uint16_t start = (uint16_t)((f[2] << 8) | f[3]);
    uint16_t count = (uint16_t)((f[4] << 8) | f[5]);
    if ((count == 0U) || (count > 125U)) {
        pi_send_exception(0x03U, 0x03U);
        return true;
    }
    if ((start >= SYS_REG_COUNT) || (count > (SYS_REG_COUNT - start))) {
        pi_send_exception(0x03U, 0x02U);
        return true;
    }

    uint8_t byte_count = (uint8_t)(2U * count);
    uint8_t resp[5 + byte_count];

    resp[0] = SLAVE_ID_PI;
    resp[1] = 0x03;
    resp[2] = byte_count;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t val = REG(start + i);
        resp[3 + 2 * i] = (uint8_t)(val >> 8);
        resp[4 + 2 * i] = (uint8_t)(val & 0xFF);
    }

    uint16_t crc = CRC16(resp, (uint8_t)(3 + byte_count));
    resp[3 + byte_count] = (uint8_t)(crc & 0xFF);
    resp[4 + byte_count] = (uint8_t)(crc >> 8);

    SLAVE_TX_SEND(resp, (uint16_t)(5 + byte_count));
    return true;
}

static bool fc06_write_single(uint8_t* f, uint16_t len)
{
    if (len != 8U) {
        pi_send_exception(0x06U, 0x03U);
        return true;
    }

    uint16_t addr  = (uint16_t)((f[2] << 8) | f[3]);
    uint16_t value = (uint16_t)((f[4] << 8) | f[5]);
    uint8_t exception = validate_pi_register_write(addr, value);

    if (exception != 0U) {
        pi_send_exception(0x06U, exception);
        return true;
    }

    REG(addr) = value;
    SLAVE_TX_SEND(f, 8);
    return true;
}

static bool fc10_write_multiple(uint8_t* f, uint16_t len)
{
    if (len < 9U) {
        pi_send_exception(0x10U, 0x03U);
        return true;
    }

    uint16_t start      = (uint16_t)((f[2] << 8) | f[3]);
    uint16_t count      = (uint16_t)((f[4] << 8) | f[5]);
    uint8_t  byte_count = f[6];

    if ((count == 0U) || (count > 123U) ||
        (byte_count != (uint16_t)(count * 2U)) ||
        (len != (uint16_t)(9U + byte_count))) {
        pi_send_exception(0x10U, 0x03U);
        return true;
    }
    if ((start >= SYS_REG_COUNT) || (count > (SYS_REG_COUNT - start))) {
        pi_send_exception(0x10U, 0x02U);
        return true;
    }

    /* Validate the complete request before changing any register. */
    for (uint16_t i = 0; i < count; i++) {
        uint16_t value = (uint16_t)((f[7U + (2U * i)] << 8) |
                                    f[8U + (2U * i)]);
        uint8_t exception = validate_pi_register_write((uint16_t)(start + i), value);
        if (exception != 0U) {
            pi_send_exception(0x10U, exception);
            return true;
        }
    }

    for (uint16_t i = 0; i < count; i++) {
        REG(start + i) = (uint16_t)((f[7U + (2U * i)] << 8) |
                                    f[8U + (2U * i)]);
    }

    uint8_t resp[8];
    memcpy(resp, f, 6);

    uint16_t crc = CRC16(resp, 6);
    resp[6] = (uint8_t)(crc & 0xFF);
    resp[7] = (uint8_t)(crc >> 8);

    SLAVE_TX_SEND(resp, 8);
    return true;
}

static bool fc05_write_coil(uint8_t* f, uint16_t len)
{
    if (len != 8U) {
        pi_send_exception(0x05U, 0x03U);
        return true;
    }

    uint16_t coil     = (uint16_t)((f[2] << 8) | f[3]);
    uint8_t  value_hi = f[4];
    uint8_t  value_lo = f[5];

    /* Only accept 0xFF00 (ON) or 0x0000 (OFF) */
    if (!((value_hi == 0xFF && value_lo == 0x00) ||
          (value_hi == 0x00 && value_lo == 0x00))) {
        pi_send_exception(0x05U, 0x03U);
        return true;
    }

    /* Only the implemented actuator bits are exposed as coils. */
    if ((coil < 112U) || (coil > 123U)) {
        pi_send_exception(0x05U, 0x02U);
        return true;
    }

    /* Direct actuator commands are accepted only in MANUAL mode. */
    if (REG(MREG_SYSTEM_MODE) == 0) {
        pi_send_exception(0x05U, 0x03U);
        return true;
    }

    uint8_t on = (value_hi == 0xFFU);
    uint8_t bit = (uint8_t)(coil - 112U);

    if (on) {
        REG(MREG_MANUAL_ACTUATOR_STATES) |= (uint16_t)(1U << bit);
    } else {
        REG(MREG_MANUAL_ACTUATOR_STATES) &= (uint16_t)~(1U << bit);
    }

    /* Echo the exact request back (standard for FC05) */
    SLAVE_TX_SEND(f, 8);
    return true;
}

/*========================================
 *  MASTER FUNCTIONS (Sensor)
 =========================================*/

/*------------------ Sensor Request & Start Timeout ----------------------*/

static void modbus_master_send_request(const uint8_t* req, uint16_t len)
{
    if ((len == 0U) || (len > MAX_FRAME_LENGTH) || sensor_tx_active) {
        return;
    }

    memcpy(sensor_tx_buffer, req, len);
    sensor_tx_length = len;
    sensor_tx_index = 0U;

    SENSOR_USART.STATUS = USART_TXCIF_bm;
    sensor_tx_active = true;
    LED_TX_SENSOR_ON();
    SENSOR_TX_EN(true);

    sensor_request_pending   = true;
    sensor_response_received = false;
    sensor_timeout_count     = 0;

    sensor_tx_service();
}

void modbus_master_read_holding(uint16_t start, uint16_t count)
{
    if (sensor_request_pending || count == 0 || count > 125) return;

    uint8_t req[8];

    req[0] = SENSOR_SLAVE_ID;
    req[1] = 0x03;
    req[2] = (uint8_t)(start >> 8);
    req[3] = (uint8_t)(start & 0xFF);
    req[4] = (uint8_t)(count >> 8);
    req[5] = (uint8_t)(count & 0xFF);

    uint16_t crc = CRC16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    sensor_fc = 0x03;

    modbus_master_send_request(req, 8);

//    printf("sensor send: %02X %02X %02X %02X %02X %02X %02X %02X\n",
//           req[0], req[1], req[2], req[3], req[4], req[5], req[6], req[7]);
}

void modbus_master_write_multiple(uint16_t start, const uint16_t* values, uint16_t count)
{
    if (sensor_request_pending || count == 0 || count > 123) return;

    uint8_t  byte_count = (uint8_t)(count * 2);
    uint8_t  req[9 + byte_count];

    req[0] = SENSOR_SLAVE_ID;
    req[1] = 0x10;
    req[2] = (uint8_t)(start >> 8);
    req[3] = (uint8_t)(start & 0xFF);
    req[4] = (uint8_t)(count >> 8);
    req[5] = (uint8_t)(count & 0xFF);
    req[6] = byte_count;

    for (uint16_t i = 0; i < count; i++) {
        req[7 + 2 * i] = (uint8_t)(values[i] >> 8);
        req[8 + 2 * i] = (uint8_t)(values[i] & 0xFF);
    }

    uint16_t crc = CRC16(req, (uint8_t)(7 + byte_count));
    req[7 + byte_count] = (uint8_t)(crc & 0xFF);
    req[8 + byte_count] = (uint8_t)(crc >> 8);

    sensor_fc = 0x10;
    modbus_master_send_request(req, (uint16_t)(9 + byte_count));
}

/*========================================
 *  PROCESS FUNCTIONS
 =========================================*/

static void process_slave_frame(uint8_t port)
{
    if (!_FULL_FRAME_RECEIVED[port]) return;

    uint8_t  idx = _FRAME_PROC_LEN[port];
    uint8_t* f   = _FRAME_PROC[port];
    slave_response_port = port;

//    printf("pi receive: ");
//    for (uint8_t i = 0; i < idx; i++) {
//        printf("%02X ", f[i]);
//    }
//    printf("\n");

    if ((idx < 4U) || (f[0] != SLAVE_ID_PI)) goto cleanup;

    uint16_t crc_calc = CRC16(f, (uint8_t)(idx - 2U));
    uint16_t crc_recv = (uint16_t)(f[idx - 2U] | (f[idx - 1U] << 8));
    if (crc_calc != crc_recv) {
        increment_comm_error_count();
        goto cleanup;
    }

    switch (f[1]) {
        case 0x03: if (!fc03_read_holding(f, idx))      goto cleanup; break;
        case 0x05: if (!fc05_write_coil(f, idx))        goto cleanup; break;
        case 0x06: if (!fc06_write_single(f, idx))      goto cleanup; break;
        case 0x10: if (!fc10_write_multiple(f, idx))    goto cleanup; break;
        default:    pi_send_exception(f[1], 0x01U);     break;
    }

cleanup:
    _FULL_FRAME_RECEIVED[port] = false;
    _FRAME_PROC_LEN[port]      = 0;
}

void process_pi(void)
{
    process_slave_frame(PI);
}

void process_sensor(void)
{
#if SENSOR_PORT_PI_SLAVE_TEST
    process_slave_frame(SENSOR);
#else
    if (!_FULL_FRAME_RECEIVED[SENSOR] || !sensor_response_received) return;

    uint8_t  idx = _FRAME_PROC_LEN[SENSOR];
    uint8_t* f   = _FRAME_PROC[SENSOR];
    bool valid_response = false;

    REG(MREG_SENSOR_RX_REJECT_REASON) = 0;
    if (idx < 5 || f[0] != SENSOR_SLAVE_ID) {
        REG(MREG_SENSOR_RX_REJECT_REASON) = 1;
        goto cleanup;
    }

    if (f[1] & 0x80U) {
        /* Exception response */
        REG(MREG_SENSOR_RX_REJECT_REASON) = 2;
        goto cleanup;
    }

    if (sensor_fc == 0x03U) {
        if (idx < 5 || f[1] != 0x03U) {
            REG(MREG_SENSOR_RX_REJECT_REASON) = 3;
            goto cleanup;
        }

        uint8_t byte_count = f[2];
        if (byte_count != (uint8_t)(SREG_REGISTER_COUNT * 2U)) {
            REG(MREG_SENSOR_RX_REJECT_REASON) = 4;
            goto cleanup;
        }
        if (idx != (uint8_t)(5U + byte_count)) {
            REG(MREG_SENSOR_RX_REJECT_REASON) = 5;
            goto cleanup;
        }

        uint16_t crc_calc = CRC16(f, (uint8_t)(3U + byte_count));
        uint16_t crc_recv = (uint16_t)(f[3 + byte_count] | (f[4 + byte_count] << 8));
        if (crc_calc != crc_recv) {
            REG(MREG_SENSOR_RX_REJECT_REASON) = 6;
            LED_ERR_ON();
            goto cleanup;
        }

//        printf("sensor receive: ");
        for (uint8_t reg = 0; reg < SREG_REGISTER_COUNT; reg++) {
            uint16_t frame_offset = (uint16_t)(3U + (reg * 2U));
            uint16_t value        = (uint16_t)((f[frame_offset] << 8) |
                                               f[frame_offset + 1]);
            system_regs[SENSOR_BASE + reg] = value;
//            printf("%02X%02X ", (uint8_t)(value >> 8), (uint8_t)(value & 0xFF));
        }
//        printf("\n");

            // 1) temps
            REG(MREG_T_CHAMBER)        = REG(MREG_SREG_CHAMBER_INTERNAL);
            REG(MREG_T_HEATER_ELEMENT) = REG(MREG_SREG_HEATING_ELEMENT);
            REG(MREG_T_EVAP_BOX)       = REG(MREG_SREG_EVAPORATOR);
            REG(MREG_SUPERHEAT)        = REG(MREG_SREG_SUPERHEAT);
            REG(MREG_SUBCOOLING)       = REG(MREG_SREG_SUBCOOLING);
            REG(MREG_T_EVAP_IN)        = REG(MREG_SREG_NTC5_TEMP);
            REG(MREG_T_EVAP_OUT)       = REG(MREG_SREG_NTC6_TEMP);
            REG(MREG_EXTERNAL)         = REG(MREG_SREG_EXTERNAL);

            update_sensor_faults();
            valid_response = true;

    }
    else if (sensor_fc == 0x10U) {
        if (idx != 8U || f[1] != 0x10U) { goto cleanup; }

        uint16_t crc_calc = CRC16(f, 6U);
        uint16_t crc_recv = (uint16_t)(f[6] | (f[7] << 8));
        if (crc_calc != crc_recv) { goto cleanup; }
        /* Write success */
        valid_response = true;
    }

cleanup:
    if (valid_response) {
        sensor_comm_succeeded();
    } else {
        sensor_comm_failed();
    }
    _FULL_FRAME_RECEIVED[SENSOR] = false;
    _FRAME_PROC_LEN[SENSOR]      = 0;
    sensor_response_received     = false;
    sensor_request_pending       = false;
#endif
}

void poll_sensor(void)
{
    _POLL_SENSOR = false;
    modbus_master_read_holding(0, SREG_REGISTER_COUNT);
}

void MB_process(void)
{
#if !SENSOR_PORT_PI_SLAVE_TEST
    bool sensor_comm_failed_since_last_process;
#endif

#if SENSOR_LINK_TTL_DEBUG_TEST
    while ((SENSOR_USART.STATUS & USART_RXCIF_bm) != 0U) {
        sensor_rx_byte(SENSOR_USART.RXDATAL);
    }
#endif
#if !SENSOR_PORT_PI_SLAVE_TEST
    sensor_tx_service();
#endif

    if (_FULL_FRAME_RECEIVED[PI])     process_pi();
    if (_FULL_FRAME_RECEIVED[SENSOR]) process_sensor();

#if SENSOR_PORT_PI_SLAVE_TEST
    _POLL_SENSOR = false;
#else
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        sensor_comm_failed_since_last_process = sensor_comm_failure_pending;
        sensor_comm_failure_pending = false;
    }

    if (sensor_comm_failed_since_last_process) {
        sensor_comm_failed();
    }
    if (_POLL_SENSOR)                 poll_sensor();
#endif
}






///*
// 
// * Modbus Implementation
// * Ruhan Louw
// * 
// * PI and SENSOR cards
// 
// */
//#
//#include <string.h>
//
//#include "system_registers.h"
//#include "MB.h"
//#include "Functions_Lib/RTC/debug_uart.h"
//#include <util/crc16.h>
//#include <util/delay.h>
//
//
///*========================================
// *  VARS
// =========================================*/
//static volatile bool _FULL_FRAME_RECEIVED[2] = {false,false};
//static volatile bool _RX_RUNNING[2] = {false,false};
//
//static uint8_t _FRAME[2][MAX_FRAME_LENGTH] = {0};
//static volatile uint8_t _FRAME_INDEX[2] = {0};
//
//
//
//volatile bool _POLL_SENSOR = false;
//uint8_t _POLL_SENSOR_INTERVAL_ms = 1500;
//
///* Sensor's Master state */
//static volatile bool sensor_request_pending = false;
//static volatile bool sensor_response_received = false;
//static volatile uint8_t sensor_fc = 0;
//static volatile uint8_t sensor_timeout_count = 0;
///* Master timeout: 13 × 8.192 ms = 106.5 ms */
//#define SENSOR_TIMEOUT      60
//#define RS_DIR_TIMEOUT_ms   0.1
//
///*========================================
// *  STATICS FUNCTIONS
// =========================================*/
////--------------RS485 Direction---------------------------
//static inline void PI_TX_EN(bool enable){
//    if(enable) DE_PI_PORT.OUTSET = (1 << DE_PI_PIN);
//    else DE_PI_PORT.OUTCLR = (1 << DE_PI_PIN);
//    _delay_ms(RS_DIR_TIMEOUT_ms);
//}
//static inline void SENSOR_TX_EN(bool enable){
//    _delay_ms(RS_DIR_TIMEOUT_ms);
//    if(enable) DE_SENSOR_PORT.OUTSET = (1 << DE_SENSOR_PIN);
//    else DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN);
//    _delay_ms(RS_DIR_TIMEOUT_ms);
//
//}
////--------------------CRC16------------------------------
//static uint16_t CRC16(const uint8_t *buf, uint8_t len)
//{
//    uint16_t crc = 0xFFFF;
//    for(uint8_t i = 0; i < len; ++i) {
//        crc = _crc16_update(crc, buf[i]);
//    }
//    return crc;
//}
//
///*========================================
// *  RX / TX INTERRUPT CALLBACK FUNCTIONS
// =========================================*/
///* ======== UART Interrupts =========*/
//void PI_RX_RECEIVE(void){
//    
//    if(_FRAME_INDEX[PI] >= MAX_FRAME_LENGTH) return;
//    LED_RX_PI_ON();
//    _FRAME[PI][_FRAME_INDEX[PI]++] = UART0_Read();
//    if(!_RX_RUNNING[PI]){
//        
//        _RX_RUNNING[PI] = true;
//        PI_RX_TIMEOUT_Start();
//    }
//    PI_RX_TIMEOUT_CounterSet(0);   
//}
//void PI_TX_COMPLETE(void){
//    LED_TX_PI_OFF();
//    PI_TX_EN(false);
//}
//
//void SENSOR_RX_RECEIVE(void){
//    if (_FRAME_INDEX[SENSOR] >= MAX_FRAME_LENGTH) return;
//    LED_RX_SENSOR_ON();
//    _FRAME[SENSOR][_FRAME_INDEX[SENSOR]++] = UART3_Read();
//    if(!_RX_RUNNING[SENSOR]){
//        _RX_RUNNING[SENSOR] = true;
//        SENSOR_RX_TIMEOUT_Start();
//    }
//    SENSOR_RX_TIMEOUT_CounterSet(0);
//}
//void SENSOR_TX_COMPLETE(void){
//    SENSOR_TX_EN(false);
//    LED_TX_SENSOR_OFF();
//}
///* ======== Timer Interrupts =========*/
//void PI_FRAME_COMPLETE(void){ //_TCB0
//    _FULL_FRAME_RECEIVED[PI] = true;
//    _RX_RUNNING[PI] = false;
//    LED_RX_PI_OFF();
//    PI_RX_TIMEOUT_Stop();
//}
//
//void SENSOR_FRAME_COMPLETE(void){ //_TCB1
//    _FULL_FRAME_RECEIVED[SENSOR] = true;
//    sensor_response_received = true;
//    sensor_request_pending = false;
//    _RX_RUNNING[SENSOR] = false;  
//    LED_RX_SENSOR_OFF();
//    SENSOR_RX_TIMEOUT_Stop(); 
//    SENSOR_REQUEST_TIMEOUT_Stop();
////    printf("%d\n", (int)_FRAME_INDEX[SENSOR]);
//    
//}
//static void PI_TX_SEND(const uint8_t* data, uint16_t len){
//    if(len > MAX_FRAME_LENGTH) return;
//    LED_TX_PI_ON();
//    PI_TX_EN(true);
//    for(uint16_t i=0; i<len; i++){
//        UART0_Write(data[i]);
//    }
////    while(!UART0_IsTxDone());
////    PI_TX_EN(false);
////    LED_TX_PI_OFF();
//    
//    printf("pi send: ");
//    for(uint8_t i = 0; i < len; i++){
//        printf("%02X ", data[i]);
//    }
//    printf("\n");
//}
//
//void Sensor_Request_Timeout(void) {
//    if (!sensor_request_pending) {
//        sensor_timeout_count = 0;
//        return;
//    }
//    sensor_timeout_count++;
//    if (sensor_timeout_count >= SENSOR_TIMEOUT) {
//        sensor_request_pending = false;
//        sensor_response_received = false;
//        _FRAME_INDEX[SENSOR] = 0;
//        _FULL_FRAME_RECEIVED[SENSOR] = false;
//        sensor_timeout_count = 0;
//        SENSOR_TX_EN(false);
//        LED_TX_SENSOR_OFF();
//        SENSOR_REQUEST_TIMEOUT_Stop();
//    } else {
//        SENSOR_REQUEST_TIMEOUT_CounterSet(0);
//    }
//}
//
///*========================================
// *  MODBUS INIT Function
// ========================================= */
//void MB_INIT(void){
//    PI_RX_TIMEOUT_Stop();
//    SENSOR_RX_TIMEOUT_Stop();
//    SENSOR_REQUEST_TIMEOUT_Stop();
//    
//    PI_RX_TIMEOUT_CaptureCallbackRegister(PI_FRAME_COMPLETE);
//    SENSOR_RX_TIMEOUT_CaptureCallbackRegister(SENSOR_FRAME_COMPLETE);
//    SENSOR_REQUEST_TIMEOUT_CaptureCallbackRegister(Sensor_Request_Timeout);
//
//    UART0_RxCompleteCallbackRegister(PI_RX_RECEIVE);
//    UART0_TxCompleteCallbackRegister(PI_TX_COMPLETE);
//    UART3_RxCompleteCallbackRegister(SENSOR_RX_RECEIVE);
//    UART3_TxCompleteCallbackRegister(SENSOR_TX_COMPLETE);   
//}
//
///*========================================
// *  FUNCTION CODES (Pi Slave)
// =========================================*/
//static bool fc03_read_holding(uint8_t* f, uint16_t len) {    
//    if (len != 8) return false;
//    uint16_t start = (f[2] << 8) | f[3];
//    uint16_t count = (f[4] << 8) | f[5];
//    if (start + count > SYS_REG_COUNT) return false;
//
//    uint8_t byte_count = 2 * count;
//    uint8_t resp[5 + byte_count];
//    resp[0] = SLAVE_ID_PI;
//    resp[1] = 0x03;
//    resp[2] = byte_count;
//    for (int i = 0; i < count; i++) {
//        uint16_t val = REG(start + i);
//        resp[3 + 2*i] = val >> 8;
//        resp[4 + 2*i] = val & 0xFF;
//    }
//    uint16_t crc = CRC16(resp, 3 + byte_count);
//    resp[3 + byte_count] = crc & 0xFF;
//    resp[4 + byte_count] = crc >> 8;
//    
//    
//    
//    PI_TX_SEND(resp, 5 + byte_count);
//    return true;
//}
//
//static bool fc06_write_single(uint8_t* f, uint16_t len) {
//    if (len != 8) return false;
//    uint16_t addr = (f[2] << 8) | f[3];
//    uint16_t value = (f[4] << 8) | f[5];
//    if (addr >= SYS_REG_COUNT) return false;
//    REG(addr) = value;
//    PI_TX_SEND(f, 8);
//    return true;
//}
//
//static bool fc10_write_multiple(uint8_t* f, uint16_t len) {
//    if (len < 9) return false;
//    uint16_t start = (f[2] << 8) | f[3];
//    uint16_t count = (f[4] << 8) | f[5];
//    uint8_t byte_count = f[6];
//    if (len != 9 + byte_count || byte_count != count * 2 || start + count > SYS_REG_COUNT) return false;
//
//    for (int i = 0; i < count; i++) {
//        REG(start + i) = (f[7 + 2*i] << 8) | f[8 + 2*i];
//    }
//
//    uint8_t resp[8];
//    memcpy(resp, f, 6);
//    uint16_t crc = CRC16(resp, 6);
//    resp[6] = crc & 0xFF;
//    resp[7] = crc >> 8;
//    PI_TX_SEND(resp, 8);
//    return true;
//}
//
//static bool fc05_write_coil(uint8_t* f, uint16_t len)
//{
//    if (len != 8) return false;
//
//    uint16_t coil = (f[2] << 8) | f[3];
//    uint8_t  value_hi = f[4];
//    uint8_t  value_lo = f[5];
//
//    // Only accept 0xFF00 (ON) or 0x0000 (OFF)
//    if (!((value_hi == 0xFF && value_lo == 0x00) || 
//          (value_hi == 0x00 && value_lo == 0x00))) {
//        goto illegal_value;
//    }
//
//    uint8_t  on  = (value_hi == 0xFF);  // true = ON, false = OFF
//    uint16_t reg = 0xFFFF;
//    uint8_t  bit = 0;
//
//    /* ====================== COIL MAPPING ====================== */
//    if (coil >= 48 && coil < 64) {           // 0x0003 SYS_STATUS
//        reg = 0x0003; bit = coil - 48;
//    }
//    else if (coil >= 80 && coil < 96) {      // 0x0005 FAULT_FLAGS ? acknowledge = clear
//        reg = 0x0005; bit = coil - 80;
//        on = 0;                               // Writing to fault flag = clear it
//    }
//    else if (coil >= 96 && coil < 112) {     // 0x0006 MODE_FLAGS
//        reg = 0x0006; bit = coil - 96;
//    }
//    else if (coil >= 112 && coil < 128) {    // 0x0007 ACTUATOR_STATES
//        reg = 0x0007; bit = coil - 112;
//    }
//    else {
//        goto illegal_address;
//    }
//
//    /* =================== MODIFY THE BIT =================== */
//    if (on) {
//        REG(reg) |=  (1 << bit);   // Set bit
//    } else {
//        REG(reg) &= ~(1 << bit);   // Clear bit
//    }
//
//    /* =================== SUCCESS RESPONSE ================= */
//    // Echo the exact request back (standard for FC05)
//    PI_TX_SEND(f, 8);
//    return true;
//
//illegal_address:
//    // Exception 02: Illegal Data Address
//    {
//        uint8_t resp[5] = { SLAVE_ID_PI, 0x85, 0x02 };
//        uint16_t crc = CRC16(resp, 3);
//        resp[3] = crc & 0xFF;
//        resp[4] = crc >> 8;
//        PI_TX_SEND(resp, 5);
//    }
//    return true;
//
//illegal_value:
//    // Exception 03: Illegal Data Value
//    {
//        uint8_t resp[5] = { SLAVE_ID_PI, 0x85, 0x03 };
//        uint16_t crc = CRC16(resp, 3);
//        resp[3] = crc & 0xFF;
//        resp[4] = crc >> 8;
//        PI_TX_SEND(resp, 5);
//    }
//    return true;
//}
//
///*========================================
// *  MASTER FUNCTIONS (Sensor)
// =========================================*/
////------------------ Sensor Request & Start Timeout ----------------------
//
//static void modbus_master_send_request(const uint8_t* req, uint16_t len) {
//    LED_TX_SENSOR_ON();
//    SENSOR_TX_EN(true);
//    for (uint16_t i = 0; i < len; i++) {
////        while(!UART3_IsTxReady()) { }
//        UART3_Write(req[i]);
//    }
////    while(!UART3_IsTxReady()) { }
//
////    _delay_ms(100);
////    SENSOR_TX_EN(false);
////    LED_TX_SENSOR_OFF();
//    
//    sensor_request_pending = true;
//    sensor_response_received = false;
//    sensor_timeout_count = 0;
//
//    SENSOR_REQUEST_TIMEOUT_CounterSet(0);
//    SENSOR_REQUEST_TIMEOUT_Start();  // Start first 8. ..  ms
//}
//
//void modbus_master_read_holding(uint16_t start, uint16_t count) {
//    if (sensor_request_pending || count == 0 || count > 125) return;
//    uint8_t req[8];
//    req[0] = SENSOR_SLAVE_ID;
//    req[1] = 0x03;
//    req[2] = start >> 8;
//    req[3] = start & 0xFF;
//    req[4] = count >> 8;
//    req[5] = count & 0xFF;
//    uint16_t crc = CRC16(req, 6);
//    req[6] = crc & 0xFF;
//    req[7] = crc >> 8;
//    
//    sensor_fc = 0x03;
//
//    modbus_master_send_request(req, 8);
//    
////    uint8_t req13[] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x0D, 0x84, 0x3C};
////    modbus_master_send_request(req13, 8);
//    printf("sensor send: %02X %02X %02X %02X %02X %02X %02X %02X\n", req[0],req[1],req[2],req[3],req[4],req[5],req[6],req[7]);
//
//}
//
//void modbus_master_write_multiple(uint16_t start, const uint16_t* values, uint16_t count) {
//    if (sensor_request_pending || count == 0 || count > 123) return;
//
//    uint8_t byte_count = count * 2;
//    uint8_t req[9 + byte_count];
//    req[0] = SENSOR_SLAVE_ID;
//    req[1] = 0x10;
//    req[2] = start >> 8;
//    req[3] = start & 0xFF;
//    req[4] = count >> 8;
//    req[5] = count & 0xFF;
//    req[6] = byte_count;
//    for (int i = 0; i < count; i++) {
//        req[7 + 2*i] = values[i] >> 8;
//        req[8 + 2*i] = values[i] & 0xFF;
//    }
//    uint16_t crc = CRC16(req, 7 + byte_count);
//    req[7 + byte_count] = crc & 0xFF;
//    req[8 + byte_count] = crc >> 8;
//
//    sensor_fc = 0x10;
//    modbus_master_send_request(req, 9 + byte_count);
//}
//
///*========================================
// *  PROCESS FUNCTIONS
// =========================================*/
//void process_pi(void) {
//    if (!_FULL_FRAME_RECEIVED[PI]) return;
//    uint16_t idx = _FRAME_INDEX[PI];
//    uint8_t* f = _FRAME[PI];
//    
//    printf("pi receive: ");
//    for(uint8_t i = 0; i < _FRAME_INDEX[PI]; i++){
//        printf("%02X ", _FRAME[PI][i]);
//    }
//    printf("\n");
//    if (idx < 4 || f[0] != SLAVE_ID_PI) goto cleanup;
//
//    switch (f[1]) {
//        case 0x03: if (!fc03_read_holding(f, idx)) goto cleanup; break;
//        case 0x05: if (!fc05_write_coil(f, idx)) goto cleanup; break;
//        case 0x06: if (!fc06_write_single(f, idx)) goto cleanup; break;
//        case 0x10: if (!fc10_write_multiple(f, idx)) goto cleanup; break;
//        default: goto cleanup;
//    }
//
//cleanup:
//    _FRAME_INDEX[PI] = 0;
//    _FULL_FRAME_RECEIVED[PI] = false;
//}
//
//void process_sensor(void) {
//    if (!_FULL_FRAME_RECEIVED[SENSOR] || !sensor_response_received) return;
//    uint16_t idx = _FRAME_INDEX[SENSOR];
//    uint8_t* f = _FRAME[SENSOR];
//    if (idx < 5 || f[0] != SENSOR_SLAVE_ID) { goto cleanup;}
//
//    if (f[1] & 0x80) {
//        // Exception
//        goto cleanup;
//    }
//
//    if (sensor_fc == 0x03) {
//        if (idx < 5 || f[1] != 0x03) { goto cleanup;}
//        uint8_t byte_count = f[2];
//        if (byte_count != SREG_REGISTER_COUNT*2) { goto cleanup;}
//        if (idx != 5 + byte_count) { goto cleanup;}
//        uint16_t crc_calc = CRC16(f, 3 + byte_count);
//        uint16_t crc_recv = f[3 + byte_count] | (f[4 + byte_count] << 8);
//        if (crc_calc != crc_recv) {LED_ERR_ON(); goto cleanup;}
//        
//        printf("sensor receive: ");
//        for (uint8_t reg = 0; reg < SREG_REGISTER_COUNT; reg++){
//            uint16_t frame_offset = 3 + (reg*2);
//            uint16_t value = (f[frame_offset] << 8) | f[frame_offset + 1];
//            system_regs[SENSOR_BASE + reg] = value;
//            
//            printf("%02X%02X ", (value >> 8), (value & 0xFF));
//        }
//        printf("\n");
//
//        // --- Optional: Copy key values to snapshot block ---
//        REG(MREG_CHAMBER_INTERNAL) = REG(MREG_SREG_CHAMBER_INTERNAL);
//        REG(MREG_T_HEATING_ELEMENT)  = REG(MREG_SREG_HEATING_ELEMENT);
//        REG(MREG_EVAPORATOR)       = REG(MREG_SREG_EVAPORATOR);
//        REG(MREG_SUPERHEAT)        = REG(MREG_SREG_SUPERHEAT);
//        REG(MREG_SUBCOOLING)       = REG(MREG_SREG_SUBCOOLING);       
//        
//    } else if (sensor_fc == 0x10) {
//        if (idx != 8 || f[1] != 0x10) { goto cleanup;}
//        uint16_t crc_calc = CRC16(f, 6);
//        uint16_t crc_recv = f[6] | (f[7] << 8);
//        if (crc_calc != crc_recv) { goto cleanup;}
//        // Write success
//    }
//
//cleanup:
//    _FRAME_INDEX[SENSOR] = 0;
//    _FULL_FRAME_RECEIVED[SENSOR] = false;
//    sensor_response_received = false;
//    sensor_request_pending = false;
//    SENSOR_REQUEST_TIMEOUT_Stop();
//
//
//}
//
//void poll_sensor(void){
//    _POLL_SENSOR = false;
//    modbus_master_read_holding(0, SREG_REGISTER_COUNT);
//
//}
//
//void MB_process(void) {
//    if (_FULL_FRAME_RECEIVED[PI]) process_pi();
//    if (_FULL_FRAME_RECEIVED[SENSOR]) process_sensor();
//    if (_POLL_SENSOR) poll_sensor();
//}
//
//
//
//
