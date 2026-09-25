#include "shift_register.h"
#include "../../system_registers.h"
#include "../../definitions.h"


static uint8_t sr_state = 0x00;  // Stores current state of all outputs



//TODO: check if delay is needed between logic
void SR_Clear(void) {
    SR_CLEAR_LOW();
    SR_CLEAR_HIGH();
    SR_LATCH_HIGH();
    SR_LATCH_LOW();
}

void SR_Write(uint8_t data) {
    // Write 8 bits MSBF
    for (int i = 7; i >= 0; i--) {
        if (data & (1 << i)) SR_DATA_HIGH();
        else SR_DATA_LOW(); // Pull low
        SR_CLOCK_HIGH(); // Clock high0
//        _delay_us(0.1);
        SR_CLOCK_LOW(); // Clock low
    }
    SR_LATCH_HIGH();
//    _delay_us(0.1);
    SR_LATCH_LOW();
}

void SR_Init(void) {
    PORTF.DIRSET = CLOCK_PIN_bm | LATCH_PIN_bm | CLEAR_PIN_bm; // Set direction of PORTF pins 
    DATA_PORT.DIRSET = DATA_PIN_bm; // Data pin on PORTE
    PORTF.OUTCLR = CLOCK_PIN_bm | LATCH_PIN_bm; // Pull low
    DATA_PORT.OUTCLR = DATA_PIN_bm; // Pull low
    PORTF.OUTSET = CLEAR_PIN_bm; // Pull high (active low)
    SR_Clear();
    SR_Write(0);
}

void SR_COIL_REG_update(void){
    if(REG(MREG_FAULT_FLAGS) & FAULT_COIL){
        SR_Write(0);
//        LED_ERR_ON();
        return;
    }; // TODO: error handling
    uint16_t coil_states = REG(MREG_ACTUATOR_STATES);
    sr_state = (uint8_t) coil_states; 
    SR_Write(sr_state);
}

