#define F_CPU 16000000UL

#include "communications.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <string.h>
#include <util/atomic.h>
#include <util/crc16.h>

#include "application_registers.h"
#include "definitions.h"
#include "mcc_generated_files/system/pins.h"
#include "sensor_measurements.h"

#define BAUD_RATE                    9600UL
#define USART_BAUD_VALUE             ((uint16_t)((F_CPU * 64UL) / (16UL * BAUD_RATE)))
#define MODBUS_MAX_FRAME             255U
#define SENSOR_REGISTER_COUNT        APP_SENSOR_REGISTER_COUNT
#define SENSOR_RESPONSE_LENGTH       (5U + (2U * SENSOR_REGISTER_COUNT))
#define SENSOR_POLL_PERIOD_MS        1000UL
#define SENSOR_RESPONSE_TIMEOUT_MS   500UL
#define SENSOR_FAILURE_LED_THRESHOLD 3U
#define MODBUS_INTERBYTE_TIMEOUT_MS  5UL

typedef enum {
    LINK_PI = 0,
    LINK_SENSOR = 1,
    LINK_COUNT = 2
} link_id_t;

typedef enum {
    SENSOR_IDLE = 0,
    SENSOR_TRANSMITTING,
    SENSOR_WAITING_RESPONSE
} sensor_state_t;

typedef struct {
    volatile uint8_t rx[MODBUS_MAX_FRAME];
    volatile uint8_t rx_length;
    volatile uint8_t rx_expected;
    volatile uint32_t rx_last_byte_ms;
    volatile bool rx_ready;
    volatile bool rx_error;

    uint8_t tx[MODBUS_MAX_FRAME];
    volatile uint8_t tx_length;
    volatile uint8_t tx_index;
    volatile bool tx_busy;
    volatile bool tx_done;
} link_t;

static link_t links[LINK_COUNT];
static volatile uint32_t milliseconds = 0U;
static sensor_state_t sensor_state = SENSOR_IDLE;
static uint32_t sensor_deadline_ms = 0U;
static uint32_t next_sensor_poll_ms = 0U;
static uint8_t sensor_failure_count = 0U;

static uint16_t modbus_crc(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    for (uint8_t i = 0U; i < length; i++) {
        crc = _crc16_update(crc, data[i]);
    }
    return crc;
}

uint32_t Communications_Millis(void)
{
    uint32_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        value = milliseconds;
    }
    return value;
}

static void set_direction(link_id_t id, bool transmit)
{
    if (id == LINK_PI) {
        if (transmit) IO_PA7_SetHigh(); else IO_PA7_SetLow();
    } else {
        if (transmit) IO_PB3_SetHigh(); else IO_PB3_SetLow();
    }
}

static void set_tx_led(link_id_t id, bool on)
{
    if (id == LINK_PI) {
        if (on) LED_TX_PI_ON(); else LED_TX_PI_OFF();
    } else {
        if (on) LED_TX_SENSOR_ON(); else LED_TX_SENSOR_OFF();
    }
}

static void set_rx_led(link_id_t id, bool on)
{
    if (id == LINK_PI) {
        if (on) LED_RX_PI_ON(); else LED_RX_PI_OFF();
    } else {
        if (on) LED_RX_SENSOR_ON(); else LED_RX_SENSOR_OFF();
    }
}

static volatile USART_t *link_usart(link_id_t id)
{
    return (id == LINK_PI) ? &USART0 : &USART3;
}

static uint8_t expected_request_length(const link_t *link)
{
    if (link->rx_length < 2U) return 0U;
    switch (link->rx[1]) {
        case 0x03U:
        case 0x05U:
        case 0x06U:
            return 8U;
        case 0x10U:
            return (link->rx_length >= 7U) ? (uint8_t)(9U + link->rx[6]) : 0U;
        default:
            return 0U;
    }
}

static uint8_t expected_response_length(const link_t *link)
{
    if (link->rx_length < 2U) return 0U;
    if ((link->rx[1] & 0x80U) != 0U) return 5U;
    if ((link->rx[1] == 0x03U) && (link->rx_length >= 3U)) {
        return (uint8_t)(5U + link->rx[2]);
    }
    return 0U;
}

static void receive_byte(link_id_t id)
{
    link_t *link = &links[id];
    volatile USART_t *usart = link_usart(id);
    uint8_t status = usart->RXDATAH;
    uint8_t value = usart->RXDATAL;

    if (link->tx_busy || link->rx_ready) return;
    if ((status & (USART_FERR_bm | USART_PERR_bm | USART_BUFOVF_bm)) != 0U) {
        link->rx_error = true;
    }
    if (link->rx_length >= MODBUS_MAX_FRAME) {
        link->rx_error = true;
        return;
    }

    set_rx_led(id, true);
    link->rx[link->rx_length++] = value;
    link->rx_last_byte_ms = milliseconds;
    link->rx_expected = (id == LINK_PI) ? expected_request_length(link)
                                        : expected_response_length(link);

    if (id == LINK_SENSOR) {
        ApplicationRegisters_SetInternal(
            APP_REG_SENSOR_RX_BYTE_COUNT,
            (uint16_t)(ApplicationRegisters_Get(APP_REG_SENSOR_RX_BYTE_COUNT) + 1U));
        ApplicationRegisters_SetInternal(APP_REG_SENSOR_RX_LAST_STATUS, status);
        ApplicationRegisters_SetInternal(APP_REG_SENSOR_RX_LAST_BYTE, value);
    }

    if ((link->rx_expected != 0U) && (link->rx_length == link->rx_expected)) {
        link->rx_ready = true;
        set_rx_led(id, false);
    }
}

static bool take_frame(link_id_t id, uint8_t *frame, uint8_t *length, bool *error)
{
    bool available = false;
    link_t *link = &links[id];

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (link->rx_ready) {
            *length = link->rx_length;
            *error = link->rx_error;
            memcpy(frame, (const void *)link->rx, link->rx_length);
            link->rx_length = 0U;
            link->rx_expected = 0U;
            link->rx_ready = false;
            link->rx_error = false;
            available = true;
        }
    }
    return available;
}

static bool start_transmit(link_id_t id, const uint8_t *data, uint8_t length)
{
    link_t *link = &links[id];
    volatile USART_t *usart = link_usart(id);

    if ((length == 0U) || link->tx_busy) return false;
    memcpy(link->tx, data, length);
    link->tx_length = length;
    link->tx_index = 0U;
    link->tx_done = false;
    link->tx_busy = true;

    usart->CTRLB &= (uint8_t)~USART_RXEN_bm;
    usart->CTRLA &= (uint8_t)~(USART_DREIE_bm | USART_TXCIE_bm);
    usart->STATUS = USART_TXCIF_bm;
    set_tx_led(id, true);
    set_direction(id, true);

    /* Seeding avoids depending on an initially pending DRE interrupt. */
    usart->TXDATAL = link->tx[link->tx_index++];
    if (link->tx_index < link->tx_length) {
        usart->CTRLA |= USART_DREIE_bm;
    } else {
        usart->CTRLA |= USART_TXCIE_bm;
    }
    return true;
}

static void transmit_dre(link_id_t id)
{
    link_t *link = &links[id];
    volatile USART_t *usart = link_usart(id);

    if (link->tx_index < link->tx_length) {
        usart->TXDATAL = link->tx[link->tx_index++];
    }
    if (link->tx_index >= link->tx_length) {
        usart->CTRLA &= (uint8_t)~USART_DREIE_bm;
        usart->CTRLA |= USART_TXCIE_bm;
    }
}

static void transmit_complete(link_id_t id)
{
    link_t *link = &links[id];
    volatile USART_t *usart = link_usart(id);

    usart->CTRLA &= (uint8_t)~USART_TXCIE_bm;
    usart->STATUS = USART_TXCIF_bm;
    set_direction(id, false);
    usart->CTRLB |= USART_RXEN_bm;
    set_tx_led(id, false);
    link->tx_busy = false;
    link->tx_done = true;
}

static void reset_partial_frames(uint32_t now)
{
    for (uint8_t id = 0U; id < LINK_COUNT; id++) {
        link_t *link = &links[id];
        if (!link->rx_ready && (link->rx_length > 0U) &&
            ((uint32_t)(now - link->rx_last_byte_ms) >= MODBUS_INTERBYTE_TIMEOUT_MS)) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                link->rx_length = 0U;
                link->rx_expected = 0U;
                link->rx_error = false;
            }
            set_rx_led((link_id_t)id, false);
        }
    }
}

static void pi_send_exception(uint8_t function, uint8_t code)
{
    uint8_t response[5] = {1U, (uint8_t)(function | 0x80U), code, 0U, 0U};
    uint16_t crc = modbus_crc(response, 3U);
    response[3] = (uint8_t)crc;
    response[4] = (uint8_t)(crc >> 8);
    (void)start_transmit(LINK_PI, response, sizeof(response));
}

static void process_pi_request(void)
{
    uint8_t frame[MODBUS_MAX_FRAME];
    uint8_t length;
    bool uart_error;

    if (!take_frame(LINK_PI, frame, &length, &uart_error)) return;
    if (uart_error || (length < 4U) || (frame[0] != 1U)) return;

    uint16_t received_crc = (uint16_t)frame[length - 2U] |
                            ((uint16_t)frame[length - 1U] << 8);
    if (modbus_crc(frame, (uint8_t)(length - 2U)) != received_crc) return;
    uint8_t function = frame[1];
    if (function == 0x03U) {
        uint16_t start = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
        uint16_t count = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
        if ((count == 0U) || (count > 125U)) {
            pi_send_exception(function, 0x03U);
            return;
        }
        if ((start >= APP_REGISTER_COUNT) ||
            (count > (APP_REGISTER_COUNT - start))) {
            pi_send_exception(function, 0x02U);
            return;
        }

        uint8_t response[MODBUS_MAX_FRAME];
        uint8_t response_length = (uint8_t)(5U + (2U * count));
        response[0] = 1U;
        response[1] = function;
        response[2] = (uint8_t)(2U * count);
        for (uint16_t i = 0U; i < count; i++) {
            uint16_t value = ApplicationRegisters_Get((uint16_t)(start + i));
            response[3U + (2U * i)] = (uint8_t)(value >> 8);
            response[4U + (2U * i)] = (uint8_t)value;
        }
        uint16_t crc = modbus_crc(response, (uint8_t)(response_length - 2U));
        response[response_length - 2U] = (uint8_t)crc;
        response[response_length - 1U] = (uint8_t)(crc >> 8);
        (void)start_transmit(LINK_PI, response, response_length);
        return;
    }

    if (function == 0x05U) {
        uint16_t coil = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
        uint16_t raw_value = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
        if ((raw_value != 0xFF00U) && (raw_value != 0x0000U)) {
            pi_send_exception(function, 0x03U);
            return;
        }
        app_write_result_t result =
            ApplicationRegisters_WriteCoil(coil, raw_value == 0xFF00U);
        if (result != APP_WRITE_OK) {
            pi_send_exception(function, (uint8_t)result);
            return;
        }
        (void)start_transmit(LINK_PI, frame, 8U);
        return;
    }

    if (function == 0x06U) {
        uint16_t address = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
        uint16_t value = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
        app_write_result_t result = ApplicationRegisters_Write(address, &value, 1U);
        if (result != APP_WRITE_OK) {
            pi_send_exception(function, (uint8_t)result);
            return;
        }
        (void)start_transmit(LINK_PI, frame, 8U);
        return;
    }

    if (function == 0x10U) {
        uint16_t start = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
        uint16_t count = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
        uint8_t byte_count = frame[6];
        if ((count == 0U) || (count > 123U) ||
            (byte_count != (uint8_t)(2U * count)) ||
            (length != (uint8_t)(9U + byte_count))) {
            pi_send_exception(function, 0x03U);
            return;
        }

        uint16_t values[123];
        for (uint16_t i = 0U; i < count; i++) {
            values[i] = (uint16_t)(((uint16_t)frame[7U + (2U * i)] << 8) |
                                   frame[8U + (2U * i)]);
        }
        app_write_result_t result = ApplicationRegisters_Write(start, values, count);
        if (result != APP_WRITE_OK) {
            pi_send_exception(function, (uint8_t)result);
            return;
        }

        uint8_t response[8];
        memcpy(response, frame, 6U);
        uint16_t crc = modbus_crc(response, 6U);
        response[6] = (uint8_t)crc;
        response[7] = (uint8_t)(crc >> 8);
        (void)start_transmit(LINK_PI, response, sizeof(response));
        return;
    }

    pi_send_exception(function, 0x01U);
}

static void start_sensor_poll(uint32_t now)
{
    uint8_t request[8] = {2U, 0x03U, 0U, 0U, 0U, SENSOR_REGISTER_COUNT, 0U, 0U};
    uint16_t crc = modbus_crc(request, 6U);
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8);

    if (start_transmit(LINK_SENSOR, request, sizeof(request))) {
        ApplicationRegisters_SetInternal(
            APP_REG_SENSOR_POLL_COUNT,
            (uint16_t)(ApplicationRegisters_Get(APP_REG_SENSOR_POLL_COUNT) + 1U));
        sensor_state = SENSOR_TRANSMITTING;
        sensor_deadline_ms = now + SENSOR_RESPONSE_TIMEOUT_MS;
    }
}

static void accept_sensor_frame(const uint8_t *frame, uint32_t now)
{
    for (uint8_t reg = 0U; reg < SENSOR_REGISTER_COUNT; reg++) {
        uint8_t offset = (uint8_t)(3U + (2U * reg));
        ApplicationRegisters_SetInternal(
            (uint16_t)(APP_SENSOR_BASE + reg),
            (uint16_t)(((uint16_t)frame[offset] << 8) | frame[offset + 1U]));
    }
    ApplicationRegisters_SetInternal(APP_REG_SENSOR_RX_FRAME_LENGTH,
                                     SENSOR_RESPONSE_LENGTH);
    ApplicationRegisters_SetInternal(APP_REG_SENSOR_REJECT_REASON, 0U);
    ApplicationRegisters_SetInternal(
        APP_REG_SYSTEM_STATUS,
        (uint16_t)(ApplicationRegisters_Get(APP_REG_SYSTEM_STATUS) |
                   APP_STATUS_SENSOR_CARD_OK));
    SensorMeasurements_OnValidFrame(now);
    sensor_failure_count = 0U;
    LED_ERR_OFF();
}

static void record_sensor_failure(uint16_t reason, uint16_t frame_length)
{
    ApplicationRegisters_SetInternal(APP_REG_SENSOR_REJECT_REASON, reason);
    ApplicationRegisters_SetInternal(APP_REG_SENSOR_RX_FRAME_LENGTH,
                                     frame_length);
    if (sensor_failure_count < UINT8_MAX) {
        sensor_failure_count++;
    }
    if (sensor_failure_count >= SENSOR_FAILURE_LED_THRESHOLD) {
        ApplicationRegisters_SetInternal(
            APP_REG_SYSTEM_STATUS,
            (uint16_t)(ApplicationRegisters_Get(APP_REG_SYSTEM_STATUS) &
                       (uint16_t)~APP_STATUS_SENSOR_CARD_OK));
        LED_ERR_ON();
    }
}

static void process_sensor(uint32_t now)
{
    link_t *link = &links[LINK_SENSOR];

    if ((sensor_state == SENSOR_TRANSMITTING) && link->tx_done) {
        link->tx_done = false;
        sensor_state = SENSOR_WAITING_RESPONSE;
    }

    if (sensor_state == SENSOR_WAITING_RESPONSE) {
        uint8_t frame[MODBUS_MAX_FRAME];
        uint8_t length;
        bool uart_error;
        if (take_frame(LINK_SENSOR, frame, &length, &uart_error)) {
            ApplicationRegisters_SetInternal(APP_REG_SENSOR_RX_FRAME_LENGTH, length);
            if (length < 2U) {
                record_sensor_failure(6U, length);
                sensor_state = SENSOR_IDLE;
                return;
            }
            uint16_t received_crc = (uint16_t)frame[length - 2U] |
                                    ((uint16_t)frame[length - 1U] << 8);
            if (!uart_error && (length == SENSOR_RESPONSE_LENGTH) &&
                (frame[0] == 2U) && (frame[1] == 0x03U) &&
                (frame[2] == (2U * SENSOR_REGISTER_COUNT)) &&
                (modbus_crc(frame, (uint8_t)(length - 2U)) == received_crc)) {
                accept_sensor_frame(frame, now);
            } else {
                record_sensor_failure(6U, length);
            }
            sensor_state = SENSOR_IDLE;
        } else if ((int32_t)(now - sensor_deadline_ms) >= 0) {
            record_sensor_failure(5U, link->rx_length);
            link->rx_length = 0U;
            link->rx_expected = 0U;
            link->rx_error = false;
            set_rx_led(LINK_SENSOR, false);
            sensor_state = SENSOR_IDLE;
        }
    }

    if ((sensor_state == SENSOR_IDLE) && ((int32_t)(now - next_sensor_poll_ms) >= 0)) {
        next_sensor_poll_ms += SENSOR_POLL_PERIOD_MS;
        start_sensor_poll(now);
    }
}

void Communications_Initialize(void)
{
    memset(links, 0, sizeof(links));
    ApplicationRegisters_Initialize();
    SensorMeasurements_Initialize();

    USART0.BAUD = USART_BAUD_VALUE;
    USART0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc |
                   USART_SBMODE_1BIT_gc | USART_CHSIZE_8BIT_gc;
    USART0.CTRLB = USART_RXEN_bm | USART_TXEN_bm;
    USART0.CTRLA = USART_RXCIE_bm;

    USART3.BAUD = USART_BAUD_VALUE;
    USART3.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc |
                   USART_SBMODE_1BIT_gc | USART_CHSIZE_8BIT_gc;
    USART3.CTRLB = USART_RXEN_bm | USART_TXEN_bm;
    USART3.CTRLA = USART_RXCIE_bm;

    set_direction(LINK_PI, false);
    set_direction(LINK_SENSOR, false);
    set_tx_led(LINK_PI, false);
    set_rx_led(LINK_PI, false);
    set_tx_led(LINK_SENSOR, false);
    set_rx_led(LINK_SENSOR, false);
    LED_ERR_OFF();
    LED_RUN_OFF();

    TCB0.CTRLA = 0U;
    TCB0.CTRLB = TCB_CNTMODE_INT_gc;
    TCB0.CCMP = 7999U;
    TCB0.CNT = 0U;
    TCB0.INTFLAGS = TCB_CAPT_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA = TCB_CLKSEL_DIV2_gc | TCB_ENABLE_bm;

    next_sensor_poll_ms = 10U;
    sensor_failure_count = 0U;
}

void Communications_Process(void)
{
    uint32_t now = Communications_Millis();
    reset_partial_frames(now);
    process_pi_request();
    process_sensor(now);
}

ISR(TCB0_INT_vect)
{
    TCB0.INTFLAGS = TCB_CAPT_bm;
    milliseconds++;
}

ISR(USART0_RXC_vect) { receive_byte(LINK_PI); }
ISR(USART0_DRE_vect) { transmit_dre(LINK_PI); }
ISR(USART0_TXC_vect) { transmit_complete(LINK_PI); }
ISR(USART3_RXC_vect) { receive_byte(LINK_SENSOR); }
ISR(USART3_DRE_vect) { transmit_dre(LINK_SENSOR); }
ISR(USART3_TXC_vect) { transmit_complete(LINK_SENSOR); }
