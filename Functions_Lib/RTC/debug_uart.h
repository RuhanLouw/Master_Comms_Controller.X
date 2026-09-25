/* 
 * File:   debug_uart.h
 * Author: Ruhan Louw
 *
 * Created on September 5, 2025, 1:28 PM
 */

#ifndef DEBUG_UART_H
#define	DEBUG_UART_H

#ifdef	__cplusplus
extern "C" {
#endif

// Debug UART 1
void debug_uart1_init(void);
void debug_RS485_1_send_string(const char *str);
void debug_RS485_2_send_string(const char *str);
void debug_uart1_send_string(const char *str);
void modbus_print_frame(const uint8_t *frame, uint16_t length);



#ifdef	__cplusplus
}
#endif

#endif	/* DEBUG_UART_H */

