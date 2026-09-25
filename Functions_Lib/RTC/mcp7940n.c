#include "mcp7940n.h"
#define F_CPU 16000000UL
#include <util/delay.h>

// --- Helpers for BCD ---
static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

static uint8_t bcd2dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

// --- Init ---
void RTC_Init(void) {
    TWI0_Initialize();
}

// --- Check if RTC already has valid time ---
bool RTC_HasValidTime(void) {
    uint8_t sec, wkday;
    uint8_t addr;

    addr = MCP7940N_REG_RTCSEC;
    if (!TWI0_WriteRead(MCP7940N_ADDR, &addr, 1, &sec, 1)) return false;

//    addr = MCP7940N_REG_RTCWKDAY;
//    if (!TWI0_WriteRead(MCP7940N_ADDR, &addr, 1, &wkday, 1)) return false;

    return (sec & MCP7940N_ST);
}

// () && (wkday & MCP7940N_OSCRUN )

// --- Set time ---
bool RTC_SetTime(const rtc_time_t *t) {
    if (!t) return false;

    uint8_t data[8];
    data[0] = MCP7940N_REG_RTCSEC;
    data[1] = dec2bcd(t->seconds) | MCP7940N_ST;
    data[2] = dec2bcd(t->minutes);
    data[3] = dec2bcd(t->hours); // 24-hour mode
    data[4] = dec2bcd(t->dayOfWeek) | MCP7940N_VBATEN;
    data[5] = dec2bcd(t->day);
    data[6] = dec2bcd(t->month);
    data[7] = dec2bcd(t->year % 100);

    return TWI0_Write(MCP7940N_ADDR, data, sizeof(data));
}

// --- Get time ---
bool RTC_GetTime(rtc_time_t *t) {
    if (!t) return false;
    uint8_t buf[7];
    uint8_t addr = MCP7940N_REG_RTCSEC;

    if (!TWI0_WriteRead(MCP7940N_ADDR, &addr, 1, buf, 7))
        return false;

    t->seconds   = bcd2dec(buf[0] & 0x7F);
    t->minutes   = bcd2dec(buf[1] & 0x7F);
    t->hours     = bcd2dec(buf[2] & 0x3F);
    t->dayOfWeek = bcd2dec(buf[3] & 0x07);
    t->day       = bcd2dec(buf[4] & 0x3F);
    t->month     = bcd2dec(buf[5] & 0x1F);
    t->year      = 2000 + bcd2dec(buf[6]);

    return true;
}
