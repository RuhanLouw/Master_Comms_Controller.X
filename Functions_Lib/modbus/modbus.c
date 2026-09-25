
/*
 * modbus.c
 * Clean, simplified Modbus RTU driver for AVR128DA48 controller.
 * - uses MCC USART wrappers (USART0_Write/Read/IsRxReady etc.)
 * - uses TCB0 for Pi silence detection, TCB1 for Sensor silence detection,
 *   and TCB2 for Sensor response timeout (reused).
 *
 * Adjust the small macros below if your MCC uses different register names.
 */

#include "modbus.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../mcc_generated_files/uart/usart0.h"
#include "../../mcc_generated_files/uart/usart3.h"
#include "../../mcc_generated_files/timer/tcb0.h"
#include "../../mcc_generated_files/timer/tcb1.h"
#include "../../mcc_generated_files/timer/tcb2.h"

/* RS485 DE pins (change if needed) */
#define DE_PI_PORT      PORTA
#define DE_PI_PIN       7
#define DE_SENSOR_PORT  PORTB
#define DE_SENSOR_PIN   3

/* local buffers/state */
static uint8_t tx_buf[NUM_PORTS][MAX_FRAME_SIZE];
static uint8_t rx_buf[NUM_PORTS][MAX_FRAME_SIZE];
static volatile uint16_t rx_len[NUM_PORTS];       // number of bytes currently in rx buffer
static volatile bool frame_ready[NUM_PORTS];      // set when silence timer indicates end of frame

/* Holding registers pointer provided by application */
static uint16_t *holding_registers = NULL;
static uint16_t holding_register_count = 0;

/* Timeout state for sensor master operations */
static volatile bool sensor_timeout_flag = false;

/* ---------- Helpers ---------- */

/* Clear DE (re-enable RX) */
static inline void de_low(uint8_t port) {
    if (port == PORT_PI) DE_PI_PORT.OUTCLR = (1 << DE_PI_PIN);
    else DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN);
}

/* Set DE (enable TX) */
static inline void de_high(uint8_t port) {
    if (port == PORT_PI) DE_PI_PORT.OUTSET = (1 << DE_PI_PIN);
    else DE_SENSOR_PORT.OUTSET = (1 << DE_SENSOR_PIN);
}

/* Modbus CRC16 */
static uint16_t modbus_crc16(const uint8_t *buf, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* Small wrapper to write a byte to selected USART (MCC wrapper) */
static inline void uart_write_byte(uint8_t port, uint8_t b) {
    if (port == PORT_PI) USART0_Write(b);
    else USART3_Write(b);
}

/* Small wrapper to read a byte (MCC wrapper) */
static inline uint8_t uart_read_byte(uint8_t port) {
    return (port == PORT_PI) ? USART0_Read() : USART3_Read();
}

/* Use this macro to test TX complete. MCC/driver-dependent.
 * On AVR UDRE/TXC are in USARTx.STATUS, TXC flag bit name differs.
 * For the AVR128DA MCC style used earlier, TXC flag was USARTn_STATUS_TXC or USART_TXCIF_bm
 * If your driver exposes different names, adapt the check below.
 */
static inline bool uart_tx_complete_poll(uint8_t port) {
    if (port == PORT_PI) {
        return (USART0.STATUS & USART_TXCIF_bm);
    } else {
        return (USART3.STATUS & USART_TXCIF_bm);
    }
}

/* Clear TX complete flag (driver dependent) */
static inline void uart_clear_txc(uint8_t port) {
    if (port == PORT_PI) USART0.STATUS = USART_TXCIF_bm;
    else USART3.STATUS = USART_TXCIF_bm;
}

/* Wait for full frame to be transmitted: blocking. After caller finished
 * writing all bytes via uart_write_byte(), call this to ensure shift reg empty.
 */
static void wait_for_tx_complete_and_start_silence(uint8_t port) {
    // wait until TX complete
    while (!uart_tx_complete_poll(port)) { /* busy wait minimal */ }
    // clear flag
    uart_clear_txc(port);
    // Start the appropriate silence timer to detect frame end for receiver
    if (port == PORT_PI) {
        TCB0_CounterSet(0);
        TCB0_Start();
    } else {
        TCB1_CounterSet(0);
        TCB1_Start();
    }
}

/* ---------- Timer callbacks (register these with MCC TCB callbacks) ---------- */

/* Called when silence timer for Pi fires. If we were receiving, mark frame. If we were transmitting,
   the code that called wait_for_tx_complete_and_start_silence started the timer ? in both cases
   we treat this as "end of frame" for RX collection. */
static void tcb0_cb(void) {
    frame_ready[PORT_PI] = true;
}

/* Silence timer for sensor port */
static void tcb1_cb(void) {
    frame_ready[PORT_SENSOR] = true;
}

/* TCB2 used for sensor response timeout; increments cycles until threshold */
static volatile uint8_t tcb2_count = 0;
static void tcb2_cb(void) {
    if (++tcb2_count * 262 >= SENSOR_TIMEOUT_MS) { // approximate
        sensor_timeout_flag = true;
        TCB2_Stop();
        tcb2_count = 0;
    } else {
        // restart next cycle
        TCB2_CounterSet(0);
        TCB2_Start();
    }
}

/* ---------- Simple blocking transmit (no tx state machine) ----------
 * - set DE high
 * - write bytes with uart_write_byte()
 * - wait TX complete (shift register empty)
 * - start silence timer (TCB)
 * - when silence callback fires application ISR will mark frame_ready
 */
static void port_send_blocking(uint8_t port, const uint8_t *frame, uint16_t len) {
    if (!frame || len == 0 || len > MAX_FRAME_SIZE) return;

    // Drive DE high
    de_high(port);

    // Write all bytes to UART (MCC driver will buffer or block minimally)
    for (uint16_t i = 0; i < len; i++) {
        uart_write_byte(port, frame[i]);
    }

    // Wait for hardware to finish sending last bit, then start silence timer.
    wait_for_tx_complete_and_start_silence(port);

    // After we start silence timer, DE will be left asserted until TCB silence fires.
    // Silence callback runs and frame_ready[port] will be set; we must clear DE there or after.
    // to be safe we clear DE here after small delay is guaranteed by TCB callback (we use frame_ready)
}

/* ---------- Public API implementations ---------- */

void controller_modbus_init(uint16_t *holding_registers_ptr, uint16_t reg_count) {
    holding_registers = holding_registers_ptr;
    holding_register_count = reg_count;

    // Reset buffers/state
    for (int p = 0; p < NUM_PORTS; p++) {
        rx_len[p] = 0;
        frame_ready[p] = false;
    }
    sensor_timeout_flag = false;
    tcb2_count = 0;

    // Configure DE pins, default RX (DE low)
    DE_PI_PORT.DIRSET = (1 << DE_PI_PIN);
    DE_SENSOR_PORT.DIRSET = (1 << DE_SENSOR_PIN);
    DE_PI_PORT.OUTCLR = (1 << DE_PI_PIN);
    DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN);

    // Init TCB timers and register callbacks
    TCB0_Initialize();
    TCB0_PeriodSet(SILENCE_TICKS);
    TCB0_CaptureCallbackRegister(tcb0_cb);

    TCB1_Initialize();
    TCB1_PeriodSet(SILENCE_TICKS);
    TCB1_CaptureCallbackRegister(tcb1_cb);

    TCB2_Initialize();
    TCB2_PeriodSet(0xFFFF);
    TCB2_CaptureCallbackRegister(tcb2_cb);
}

/* This function is expected to be called frequently from main loop.
 * It will process frames from Pi and (optionally) completed sensor frames.
 */
void controller_modbus_process(void) {
    // Handle Pi frame if ready
    if (frame_ready[PORT_PI]) {
        uint16_t len = rx_len[PORT_PI];
        if (len >= 5) { // minimal Modbus frame length
            // Validate CRC
            uint16_t crc = modbus_crc16(rx_buf[PORT_PI], len - 2);
            uint16_t frame_crc = (uint16_t)rx_buf[PORT_PI][len - 2] |
                                 ((uint16_t)rx_buf[PORT_PI][len - 1] << 8);
            if (crc == frame_crc) {
                // Parse (support 0x03 and 0x10)
                uint8_t addr = rx_buf[PORT_PI][0];
                uint8_t func = rx_buf[PORT_PI][1];
                uint16_t reg_addr = (rx_buf[PORT_PI][2] << 8) | rx_buf[PORT_PI][3];
                uint16_t reg_count = (rx_buf[PORT_PI][4] << 8) | rx_buf[PORT_PI][5];

                if (addr == CONTROLLER_SLAVE_ID) {
                    if (func == 0x03) {
                        // read holding registers
                        if ((reg_addr + reg_count) <= holding_register_count) {
                            uint8_t out[MAX_FRAME_SIZE];
                            out[0] = CONTROLLER_SLAVE_ID;
                            out[1] = 0x03;
                            out[2] = (uint8_t)(reg_count * 2);
                            uint16_t outidx = 3;
                            for (uint16_t i = 0; i < reg_count; i++) {
                                uint16_t v = holding_registers[reg_addr + i];
                                out[outidx++] = (uint8_t)(v >> 8);
                                out[outidx++] = (uint8_t)(v & 0xFF);
                            }
                            uint16_t crc16 = modbus_crc16(out, outidx);
                            out[outidx++] = (uint8_t)(crc16 & 0xFF);
                            out[outidx++] = (uint8_t)(crc16 >> 8);

                            // send response (blocking)
                            port_send_blocking(PORT_PI, out, outidx);

                            // clear DE after silence timer completion (TCB callback will set frame_ready)
                            de_low(PORT_PI);
                        }
                    } else if (func == 0x10) {
                        // write multiple registers
                        uint8_t byte_count = rx_buf[PORT_PI][6];
                        if (byte_count == reg_count * 2 && (reg_addr + reg_count) <= holding_register_count) {
                            uint16_t vals[reg_count];
                            for (uint16_t i = 0; i < reg_count; i++) {
                                vals[i] = (rx_buf[PORT_PI][7 + i*2] << 8) | rx_buf[PORT_PI][7 + i*2 + 1];
                            }
                            // application hook
                            controller_update_holding_registers(vals, reg_addr, reg_count);

                            // echo response
                            uint8_t out[8];
                            out[0] = CONTROLLER_SLAVE_ID;
                            out[1] = 0x10;
                            out[2] = (uint8_t)(reg_addr >> 8);
                            out[3] = (uint8_t)(reg_addr & 0xFF);
                            out[4] = (uint8_t)(reg_count >> 8);
                            out[5] = (uint8_t)(reg_count & 0xFF);
                            uint16_t crc16 = modbus_crc16(out, 6);
                            out[6] = (uint8_t)(crc16 & 0xFF);
                            out[7] = (uint8_t)(crc16 >> 8);
                            port_send_blocking(PORT_PI, out, 8);
                            de_low(PORT_PI);
                        }
                    }
                    // else: unsupported function (could send exception)
                }
            }
        }
        // reset rx state
        rx_len[PORT_PI] = 0;
        frame_ready[PORT_PI] = false;
    }

    // Sensor frame handled by blocking master functions that wait for frame_ready[PORT_SENSOR].
    // No extra processing here.
}

/* ---------- Master: read/write sensor (blocking) ---------- */
static bool sensor_wait_for_response(uint16_t expected_count, uint16_t *out_data) {
    // start sensor timeout via TCB2
    sensor_timeout_flag = false;
    tcb2_count = 0;
    TCB2_CounterSet(0);
    TCB2_Start();

    // wait for frame_ready or timeout
    while (!frame_ready[PORT_SENSOR] && !sensor_timeout_flag) {
        // busy loop; you may add a small delay or allow other tasks
    }
    TCB2_Stop();

    if (sensor_timeout_flag || !frame_ready[PORT_SENSOR]) {
        // clear rx buffer
        rx_len[PORT_SENSOR] = 0;
        frame_ready[PORT_SENSOR] = false;
        return false;
    }

    uint16_t len = rx_len[PORT_SENSOR];
    // validate CRC
    if (len < 5) {
        rx_len[PORT_SENSOR] = 0;
        frame_ready[PORT_SENSOR] = false;
        return false;
    }
    uint16_t crc = modbus_crc16(rx_buf[PORT_SENSOR], len - 2);
    uint16_t frame_crc = (uint16_t)rx_buf[PORT_SENSOR][len - 2] | ((uint16_t)rx_buf[PORT_SENSOR][len - 1] << 8);
    if (crc != frame_crc) {
        rx_len[PORT_SENSOR] = 0;
        frame_ready[PORT_SENSOR] = false;
        return false;
    }

    uint8_t func = rx_buf[PORT_SENSOR][1];
    if (func == 0x03) {
        uint8_t bytecount = rx_buf[PORT_SENSOR][2];
        if (bytecount != expected_count * 2) {
            rx_len[PORT_SENSOR] = 0;
            frame_ready[PORT_SENSOR] = false;
            return false;
        }
        for (uint16_t i = 0; i < expected_count; i++) {
            out_data[i] = (rx_buf[PORT_SENSOR][3 + i*2] << 8) | rx_buf[PORT_SENSOR][3 + i*2 + 1];
        }
        rx_len[PORT_SENSOR] = 0;
        frame_ready[PORT_SENSOR] = false;
        return true;
    } else if (func == 0x10) {
        // expect echo (8 bytes)
        if (len >= 8) {
            // echo validated earlier by CRC
            rx_len[PORT_SENSOR] = 0;
            frame_ready[PORT_SENSOR] = false;
            return true;
        }
    }
    rx_len[PORT_SENSOR] = 0;
    frame_ready[PORT_SENSOR] = false;
    return false;
}

bool controller_read_sensor_holding(uint8_t sensor_slave_id, uint16_t reg_addr, uint16_t reg_count, uint16_t *out_data) {
    if (!out_data || reg_count == 0 || reg_count > 120) return false;

    uint8_t req[MAX_FRAME_SIZE];
    req[0] = sensor_slave_id;
    req[1] = 0x03;
    req[2] = (uint8_t)(reg_addr >> 8);
    req[3] = (uint8_t)(reg_addr & 0xFF);
    req[4] = (uint8_t)(reg_count >> 8);
    req[5] = (uint8_t)(reg_count & 0xFF);
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    // clear rx state
    rx_len[PORT_SENSOR] = 0;
    frame_ready[PORT_SENSOR] = false;

    // send request blocking
    port_send_blocking(PORT_SENSOR, req, 8);
    de_low(PORT_SENSOR); // After starting silence timer, we can lower DE (the silence timer already running)

    // wait for response (blocking)
    return sensor_wait_for_response(reg_count, out_data);
}

bool controller_write_sensor_multiple(uint8_t sensor_slave_id, uint16_t reg_addr, uint16_t reg_count, const uint16_t *data) {
    if (!data || reg_count == 0 || reg_count > 120) return false;

    uint8_t req[MAX_FRAME_SIZE];
    req[0] = sensor_slave_id;
    req[1] = 0x10;
    req[2] = (uint8_t)(reg_addr >> 8);
    req[3] = (uint8_t)(reg_addr & 0xFF);
    req[4] = (uint8_t)(reg_count >> 8);
    req[5] = (uint8_t)(reg_count & 0xFF);
    req[6] = (uint8_t)(reg_count * 2);
    uint16_t idx = 7;
    for (uint16_t i = 0; i < reg_count; i++) {
        req[idx++] = (uint8_t)(data[i] >> 8);
        req[idx++] = (uint8_t)(data[i] & 0xFF);
    }
    uint16_t crc = modbus_crc16(req, idx);
    req[idx++] = (uint8_t)(crc & 0xFF);
    req[idx++] = (uint8_t)(crc >> 8);

    rx_len[PORT_SENSOR] = 0;
    frame_ready[PORT_SENSOR] = false;

    port_send_blocking(PORT_SENSOR, req, idx);
    de_low(PORT_SENSOR);

    return sensor_wait_for_response(reg_count, NULL);
}

/* ---------- RX callbacks (call from USART RX ISR) ---------- */
/* These simply append bytes and restart the silence detection timer. */

void controller_pi_rx_callback(void) {
    if (USART0_IsRxReady()) {
        uint8_t b = USART0_Read();
        if (rx_len[PORT_PI] < MAX_FRAME_SIZE) rx_buf[PORT_PI][rx_len[PORT_PI]++] = b;
        // restart silence timer (single-shot)
        TCB0_CounterSet(0);
        TCB0_Start();
        frame_ready[PORT_PI] = false;
    }
}

void controller_sensor_rx_callback(void) {
    if (USART3_IsRxReady()) {
        uint8_t b = USART3_Read();
        if (rx_len[PORT_SENSOR] < MAX_FRAME_SIZE) rx_buf[PORT_SENSOR][rx_len[PORT_SENSOR]++] = b;
        TCB1_CounterSet(0);
        TCB1_Start();
        frame_ready[PORT_SENSOR] = false;
    }
}

/* Optional: If you use TX complete ISRs rather than polling, call these */
void controller_pi_tx_complete_callback(void) {
    // not used in current blocking TX approach
}

void controller_sensor_tx_complete_callback(void) {
    // not used in current blocking TX approach
}

/* Default application hook ? weak symbol pattern would be better.
 * Implement this in your app to handle writes from Pi.
 */
__attribute__((weak))
void controller_update_holding_registers(const uint16_t *values, uint16_t addr, uint16_t count) {
    if (!holding_registers) return;
    if ((addr + count) > holding_register_count) return;
    for (uint16_t i = 0; i < count; i++) holding_registers[addr + i] = values[i];
}





///*
// * Modbus RTU implementation for AVR128DA48:
// * - Controller is slave to Pi (USART0, RS485, DE pin PA7).
// * - Controller as master to sensor (USART3, RS485, DE pin PB3).
// * - Uses MCC TCB0 (Pi), TCB1 (sensor) for 3.5-char silence (~3.645 ms).
// * - Uses MCC TCB2 for sensor response timeout (~1000 ms).
// * - Uses MCC 256-byte RX/TX buffers with RX/TX interrupts.
// * - Supports Function Codes 03 (Read Holding Registers), 16 (Write Multiple Registers).
// * - Handles slave exception responses (e.g., 0x83, 0x90).
// * - Non-blocking operation to avoid missing Pi messages.
// */
//
//#include "modbus.h"
//
//// Buffers for TX and RX (used for frame building, MCC handles actual buffering)
//uint8_t tx_buffer[NUM_PORTS][MAX_FRAME_SIZE];
//uint8_t rx_buffer[NUM_PORTS][MAX_FRAME_SIZE];
//static volatile uint8_t rx_index[NUM_PORTS] = {0};    // Tracks received bytes
//static volatile bool frame_complete[NUM_PORTS] = {false}; // Flags complete RX frame
//volatile bool tx_silence_complete[NUM_PORTS] = {false}; // Flags TX silence period (non-static)
//volatile bool tx_in_progress[NUM_PORTS] = {false}; // Flags active TX (non-static)
//static volatile bool timeout_occurred = false; // Flags sensor response timeout
//static uint16_t *holding_registers;           // Pointer to application?s holding registers
//static uint16_t holding_reg_count;           // Number of holding registers
//volatile uint8_t tx_index[NUM_PORTS] = {0};   // Tracks TX bytes
//volatile uint8_t tx_length[NUM_PORTS] = {0};  // Total TX frame length
//static volatile uint8_t timeout_cycle_count = 0; // TCB2 cycles for 1000 ms
//
//// Set DE/RE pin high for TX
//static void set_de_high(uint8_t port) {
//    if (port == PORT_PI) {
//        DE_PI_PORT.OUTSET = (1 << DE_PI_PIN); // PA7 high
//    } else if (port == PORT_SENSOR) {m
//        DE_SENSOR_PORT.OUTSET = (1 << DE_SENSOR_PIN); // PB3 high
//    }
//}
//
//// TCB0 callback: Pi frame or TX silence complete
//static void modbus_tcb0_capt_isr(void) {
//    if (tx_in_progress[PORT_PI]) {
//        tx_silence_complete[PORT_PI] = true; // TX silence complete
//        DE_PI_PORT.OUTCLR = (1 << DE_PI_PIN); // PA7 low (RX mode)
//        tx_in_progress[PORT_PI] = false;
//    } else {
//        frame_complete[PORT_PI] = true; // RX frame complete
//    }
//}
//
//// TCB1 callback: Sensor frame or TX silence complete
//static void modbus_tcb1_capt_isr(void) {
//    if (tx_in_progress[PORT_SENSOR]) {
//        tx_silence_complete[PORT_SENSOR] = true; // TX silence complete
//        DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN); // PB3 low (RX mode)
//        tx_in_progress[PORT_SENSOR] = false;
//        // Start TCB2 for response timeout
//        timeout_cycle_count = 0;
//        TCB2_CounterSet(0);
//        TCB2_PeriodSet(0xFFFF); // ~262 ms per cycle
//        TCB2_CAPTInterruptEnable();
//        TCB2_Start();
//    } else {
//        frame_complete[PORT_SENSOR] = true; // RX frame complete
//    }
//}
//
//// TCB2 callback: Sensor response timeout
//static void modbus_tcb2_capt_isr(void) {
//    if (++timeout_cycle_count >= TIMEOUT_CYCLES) {
//        timeout_occurred = true; // Flag timeout after ~1000 ms
//        TCB2_Stop();
//    } else {
//        TCB2_CounterSet(0); // Restart for next cycle
//        TCB2_Start();
//    }
//}
//
//// Initialize timers using MCC drivers
//static void init_timers(void) {
//    // TCB0: Pi port, single-shot, 3.645 ms
//    TCB0_Initialize();
//    TCB0_PeriodSet(SILENCE_TICKS); // Correct to 911 (0x038F)
//    TCB0_CaptureCallbackRegister(modbus_tcb0_capt_isr);
//    TCB0_CAPTInterruptEnable();
//
//    // TCB1: Sensor port, single-shot, 3.645 ms
//    TCB1_Initialize();
//    TCB1_PeriodSet(SILENCE_TICKS);
//    TCB1_CaptureCallbackRegister(modbus_tcb1_capt_isr);
//    TCB1_CAPTInterruptEnable();
//
//    // TCB2: Sensor timeout, single-shot, ~1000 ms (4 * 262 ms)
//    TCB2_Initialize();
//    TCB2_PeriodSet(0xFFFF); // Max 16-bit for ~262 ms
//    TCB2_CaptureCallbackRegister(modbus_tcb2_capt_isr);
//}
//
//// Write a byte to MCC TX buffer
//static void usart_write(uint8_t port, uint8_t data) {
//    if (port == PORT_PI) {
//        USART0_Write(data); // Queue to MCC TX buffer
//    } else if (port == PORT_SENSOR) {
//        USART3_Write(data);
//    }
//}
//
//// Read a byte from MCC RX buffer
//static uint8_t usart_read(uint8_t port) {
//    return (port == PORT_PI) ? USART0_Read() : USART3_Read();
//}
//
//// Check if RX data is available in MCC buffer
//static bool usart_is_rx_ready(uint8_t port) {
//    return (port == PORT_PI) ? USART0_IsRxReady() : USART3_IsRxReady();
//}
//
//// Calculate Modbus RTU CRC-16
//static uint16_t calculate_crc16(uint8_t *buffer, uint8_t length) {
//    uint16_t crc = 0xFFFF;
//    for (uint8_t pos = 0; pos < length; pos++) {
//        crc ^= buffer[pos];
//        for (uint8_t i = 8; i != 0; i--) {
//            if (crc & 0x0001) {
//                crc >>= 1;
//                crc ^= 0xA001;
//            } else {
//                crc >>= 1;
//            }
//        }
//    }
//    return crc;
//}
//
//// Build Modbus request for sensor (Function Code 03 or 16)
//static uint8_t build_sensor_request(uint8_t sensor_slave_id, uint8_t func_code, uint16_t reg_addr, uint16_t reg_count, const uint16_t *data) {
//    uint8_t *buffer = tx_buffer[PORT_SENSOR];
//    buffer[0] = sensor_slave_id; // Slave ID
//    buffer[1] = func_code; // 03 (read) or 10 (write multiple)
//    buffer[2] = reg_addr >> 8; // Register address high byte
//    buffer[3] = reg_addr & 0xFF; // Register address low byte
//    buffer[4] = reg_count >> 8; // Register count high byte
//    buffer[5] = reg_count & 0xFF; // Register count low byte
//    uint8_t len = 6;
//    if (func_code == 0x10) { // Write multiple registers
//        buffer[6] = reg_count * 2; // Byte count
//        for (uint16_t i = 0; i < reg_count; i++) {
//            buffer[7 + (i * 2)] = data[i] >> 8; // Data high byte
//            buffer[7 + (i * 2) + 1] = data[i] & 0xFF; // Data low byte
//        }
//        len = 7 + (reg_count * 2);
//    }
//    uint16_t crc = calculate_crc16(buffer, len);
//    buffer[len] = crc & 0xFF; // CRC low byte
//    buffer[len + 1] = crc >> 8; // CRC high byte
//    return len + 2; // Total frame length
//}
//
//// Validate sensor response, including exception responses
//static bool validate_sensor_response(uint8_t sensor_slave_id, uint8_t func_code, uint8_t length, uint16_t reg_addr, uint16_t reg_count) {
//    uint8_t *buffer = rx_buffer[PORT_SENSOR];
//    // Check for exception response
//    if (buffer[0] == sensor_slave_id && buffer[1] == (func_code | 0x80) && length >= 3) {
//        return false; // Exception code in buffer[2]
//    }
//    // Validate normal response
//    if (length < 5 || buffer[0] != sensor_slave_id || buffer[1] != func_code) return false;
//    if (func_code == 0x03) { // Read holding registers
//        if (length != (3 + buffer[2] + 2) || buffer[2] != (reg_count * 2)) return false;
//    } else if (func_code == 0x10) { // Write multiple registers
//        if (length != 8) return false;
//        if (buffer[2] != (reg_addr >> 8) || buffer[3] != (reg_addr & 0xFF)) return false;
//        if (buffer[4] != (reg_count >> 8) || buffer[5] != (reg_count & 0xFF)) return false;
//    }
//    // Verify CRC
//    uint16_t crc = calculate_crc16(buffer, length - 2);
//    return (buffer[length - 2] == (crc & 0xFF)) && (buffer[length - 1] == (crc >> 8));
//}
//
//// Send request to sensor and process response
//static bool send_sensor_request(uint8_t sensor_slave_id, uint8_t func_code, uint16_t reg_addr, uint16_t reg_count, uint16_t *data, bool is_write) {
//    // Build and queue request
//    tx_length[PORT_SENSOR] = build_sensor_request(sensor_slave_id, func_code, reg_addr, reg_count, is_write ? data : NULL);
//    rx_index[PORT_SENSOR] = 0;
//    frame_complete[PORT_SENSOR] = false;
//    tx_index[PORT_SENSOR] = 0;
//    tx_in_progress[PORT_SENSOR] = true;
//    tx_silence_complete[PORT_SENSOR] = false;
//    timeout_occurred = false;
//    set_de_high(PORT_SENSOR);
//    // Queue frame to MCC TX buffer
//    for (uint8_t i = 0; i < tx_length[PORT_SENSOR]; i++) {
//        usart_write(PORT_SENSOR, tx_buffer[PORT_SENSOR][i]);
//    }
//    // Wait for TX silence and response
//    while (!tx_silence_complete[PORT_SENSOR]);
//    while (!frame_complete[PORT_SENSOR] && !timeout_occurred);
//    if (timeout_occurred || !frame_complete[PORT_SENSOR]) return false;
//    // Read response from MCC RX buffer
//    rx_index[PORT_SENSOR] = 0;
//    while (usart_is_rx_ready(PORT_SENSOR) && rx_index[PORT_SENSOR] < MAX_FRAME_SIZE) {
//        rx_buffer[PORT_SENSOR][rx_index[PORT_SENSOR]++] = usart_read(PORT_SENSOR);
//    }
//    // Validate response
//    if (!validate_sensor_response(sensor_slave_id, func_code, rx_index[PORT_SENSOR], reg_addr, reg_count)) return false;
//    // Extract data for read requests
//    if (func_code == 0x03 && !is_write) {
//        for (uint16_t i = 0; i < reg_count; i++) {
//            data[i] = (rx_buffer[PORT_SENSOR][3 + (i * 2)] << 8) | rx_buffer[PORT_SENSOR][3 + (i * 2) + 1];
//        }
//    }
//    return true;
//}
//
//// Handle Pi request to read Controller?s holding registers
//static void handle_pi_read_holding(uint16_t reg_addr, uint16_t reg_count) {
//    if (reg_addr + reg_count > holding_reg_count) return;
//    uint8_t *buffer = tx_buffer[PORT_PI];
//    buffer[0] = CONTROLLER_SLAVE_ID;
//    buffer[1] = 0x03;
//    buffer[2] = reg_count * 2;
//    for (uint16_t i = 0; i < reg_count; i++) {
//        buffer[3 + (i * 2)] = holding_registers[reg_addr + i] >> 8;
//        buffer[3 + (i * 2) + 1] = holding_registers[reg_addr + i] & 0xFF;
//    }
//    uint8_t len = 3 + (reg_count * 2);
//    uint16_t crc = calculate_crc16(buffer, len);
//    buffer[len] = crc & 0xFF;
//    buffer[len + 1] = crc >> 8;
//    tx_length[PORT_PI] = len + 2;
//    tx_index[PORT_PI] = 0;
//    tx_in_progress[PORT_PI] = true;
//    tx_silence_complete[PORT_PI] = false;
//    set_de_high(PORT_PI);
//    for (uint8_t i = 0; i < tx_length[PORT_PI]; i++) {
//        usart_write(PORT_PI, tx_buffer[PORT_PI][i]);
//    }
//}
//
//// Handle Pi request to write Controller?s holding registers
//static void handle_pi_write_multiple(uint16_t reg_addr, uint16_t reg_count, uint8_t *data) {
//    if (reg_addr + reg_count > holding_reg_count) return;
//    uint16_t new_values[reg_count];
//    for (uint16_t i = 0; i < reg_count; i++) {
//        new_values[i] = (data[i * 2] << 8) | data[i * 2 + 1];
//    }
//    controller_update_holding_registers(new_values, reg_addr, reg_count);
//    uint8_t *buffer = tx_buffer[PORT_PI];
//    buffer[0] = CONTROLLER_SLAVE_ID;
//    buffer[1] = 0x10;
//    buffer[2] = reg_addr >> 8;
//    buffer[3] = reg_addr & 0xFF;
//    buffer[4] = reg_count >> 8;
//    buffer[5] = reg_count & 0xFF;
//    uint16_t crc = calculate_crc16(buffer, 6);
//    buffer[6] = crc & 0xFF;
//    buffer[7] = crc >> 8;
//    tx_length[PORT_PI] = 8;
//    tx_index[PORT_PI] = 0;
//    tx_in_progress[PORT_PI] = true;
//    tx_silence_complete[PORT_PI] = false;
//    set_de_high(PORT_PI);
//    for (uint8_t i = 0; i < tx_length[PORT_PI]; i++) {
//        usart_write(PORT_PI, tx_buffer[PORT_PI][i]);
//    }
//}
//
//// Process incoming Pi requests
//void controller_handle_pi_request(void) {
//    uint8_t port = PORT_PI;
//    if (!frame_complete[port] || rx_index[port] < 8) return;
//    uint8_t *buffer = rx_buffer[port];
//    if (buffer[0] != CONTROLLER_SLAVE_ID || calculate_crc16(buffer, rx_index[port] - 2) != ((buffer[rx_index[port] - 1] << 8) | buffer[rx_index[port] - 2])) {
//        rx_index[port] = 0;
//        frame_complete[port] = false;
//        return;
//    }
//    uint16_t reg_addr = (buffer[2] << 8) | buffer[3];
//    uint16_t reg_count = (buffer[4] << 8) | buffer[5];
//    if (buffer[1] == 0x03) {
//        handle_pi_read_holding(reg_addr, reg_count);
//    } else if (buffer[1] == 0x10 && buffer[6] == reg_count * 2) {
//        handle_pi_write_multiple(reg_addr, reg_count, &buffer[7]);
//    }
//    rx_index[port] = 0;
//    frame_complete[port] = false;
//}
//
//// Initialize Modbus and timers
//void controller_modbus_init(uint16_t *holding_registers_ptr, uint16_t reg_count) {
//    holding_registers = holding_registers_ptr ? holding_registers_ptr : dummy_holding_registers;
//    holding_reg_count = reg_count ? reg_count : HOLDING_REG_COUNT;
//    DE_PI_PORT.DIRSET = (1 << DE_PI_PIN); // PA7 output
//    DE_SENSOR_PORT.DIRSET = (1 << DE_SENSOR_PIN); // PB3 output
//    DE_PI_PORT.OUTCLR = (1 << DE_PI_PIN); // PA7 low (RX mode)
//    DE_SENSOR_PORT.OUTCLR = (1 << DE_SENSOR_PIN); // PB3 low (RX mode)
//    init_timers();
//}
//
//// Read sensor holding registers
//bool controller_read_sensor_holding(uint8_t sensor_slave_id, uint16_t reg_addr, uint16_t reg_count, uint16_t *data) {
//    return send_sensor_request(sensor_slave_id, 0x03, reg_addr, reg_count, data, false);
//}
//
//// Write multiple sensor holding registers
//bool controller_write_sensor_multiple(uint8_t sensor_slave_id, uint16_t reg_addr, uint16_t reg_count, const uint16_t *data) {
//    return send_sensor_request(sensor_slave_id, 0x10, reg_addr, reg_count, (uint16_t *)data, true);
//}
//
//// Application callback: Handle Pi writing to registers
//void controller_update_holding_registers(const uint16_t *registers, uint16_t addr, uint16_t count) {
//    for (uint16_t i = 0; i < count && (addr + i) < holding_reg_count; i++) {
//        holding_registers[addr + i] = registers[i];
//    }
//}
//
//// RX callback for Pi (USART0)
//void controller_pi_rx_callback(void) {
//    if (rx_index[PORT_PI] < MAX_FRAME_SIZE && usart_is_rx_ready(PORT_PI)) {
//        rx_buffer[PORT_PI][rx_index[PORT_PI]++] = usart_read(PORT_PI);
//        TCB0_CounterSet(0);
//        TCB0_Start();
//        frame_complete[PORT_PI] = false;
//    }
//}
//
//// RX callback for sensor (USART3)
//void controller_sensor_rx_callback(void) {
//    if (rx_index[PORT_SENSOR] < MAX_FRAME_SIZE && usart_is_rx_ready(PORT_SENSOR)) {
//        rx_buffer[PORT_SENSOR][rx_index[PORT_SENSOR]++] = usart_read(PORT_SENSOR);
//        TCB1_CounterSet(0);
//        TCB1_Start();
//        frame_complete[PORT_SENSOR] = false;
//    }
//}
//
//// TX complete callback for Pi (USART0)
//void controller_pi_tx_complete_callback(void) {
//    if (tx_index[PORT_PI] < tx_length[PORT_PI]) {
//        usart_write(PORT_PI, tx_buffer[PORT_PI][tx_index[PORT_PI]++]);
//    } else {
//        TCB0_CounterSet(0);
//        TCB0_Start();
//        USART0.CTRLA &= ~USART_TXCIE_bm;
//    }
//}
//
//// TX complete callback for sensor (USART3)
//void controller_sensor_tx_complete_callback(void) {
//    if (tx_index[PORT_SENSOR] < tx_length[PORT_SENSOR]) {
//        usart_write(PORT_SENSOR, tx_buffer[PORT_SENSOR][tx_index[PORT_SENSOR]++]);
//    } else {
//        TCB1_CounterSet(0);
//        TCB1_Start();
//        USART3.CTRLA &= ~USART_TXCIE_bm;
//    }
//}
//
////// Dummy holding registers for easy access (modify as needed)
////#define HOLDING_REG_COUNT 10
////static uint16_t dummy_holding_registers[HOLDING_REG_COUNT] = {
////    0, 0, 0, 0, // 40001?40004: Sensor temperatures (e.g., 0.1°C per unit)
////    0, 0,       // 40005?40006: Configurations (e.g., relay settings)
////    0, 0, 0, 0  // 40007?40010: Status or reserved
////};

