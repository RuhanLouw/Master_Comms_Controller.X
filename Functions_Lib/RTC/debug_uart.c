#include "../../mcc_generated_files/uart/usart0.h"
#include "../../pinDef.h"
#include <stdint.h>
#include <stdio.h>

// Debug UART 1


// Call this whenever you are about to send a Modbus frame
void modbus_print_frame(const uint8_t *frame, uint16_t length)
{
    char hexbuf[3 * 64 + 1] = {0};  // worst case: 64 bytes -> 192 chars + null
                                           // (3 chars per byte: "XX ")

    // Safety: don't overflow if someone sends a huge frame
    if (length > 64) length = 64;

    for (uint16_t i = 0; i < length; i++) {
        sprintf(hexbuf + 3*i, "%02X ", frame[i]);   // "A1 00 01 ..." format
    }

    printf("[MB TX] %s\r\n", hexbuf);   // you will see this on the RPi terminal
}




void debug_uart1_init(void){
    UART1_Initialize();
}

void debug_uart1_send_string(const char *str){
    while(*str){
        while(!UART1_IsTxReady());
        UART1_Write(*str++);
    }
}

void debug_RS485_2_send_string(const char *str){

    while(*str){
        while(!UART3_IsTxReady());
        UART3_Write(*str++);
        
    }

}

void debug_RS485_1_send_string(const char *str){

    while(*str){
        while(!UART0_IsTxReady());
        UART0_Write(*str++);
        
    }
}
    
   