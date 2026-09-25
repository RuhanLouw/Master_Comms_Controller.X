
/**
  * TCB3 Generated Driver File
  *
  * @file tcb3.c
  *
  * @ingroup tcb3
  *
  * @brief This file contains the API implementation for the TCB3 module driver.
  *
  * @version TCB3 Driver Version 2.0.0
  *
  * @version Package Version 6.0.0
*/
/*
© [2026] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/

#include "../tcb3.h"


/**
 * Section: Global Variables Definitions
*/
static void (*TCB3_OVF_isr_cb)(void);
static void TCB3_DefaultOverflowCallback(void);
static void (*TCB3_CAPT_isr_cb)(void);
static void TCB3_DefaultCaptureCallback(void);

void TCB3_Initialize(void)
{
    TCB3.CTRLA &= ~TCB_ENABLE_bm; /* Stop Timer */

    TCB3.CTRLB = (0 << TCB_ASYNC_bp)   // ASYNC disabled
        | (0 << TCB_CCMPEN_bp)   // CCMPEN disabled
        | (0 << TCB_CCMPINIT_bp)   // CCMPINIT disabled
        | (TCB_CNTMODE_INT_gc);  // CNTMODE INT

    TCB3.DBGCTRL = (0 << TCB_DBGRUN_bp);  // DBGRUN disabled

    TCB3.EVCTRL = (0 << TCB_CAPTEI_bp)   // CAPTEI disabled
        | (0 << TCB_EDGE_bp)   // EDGE disabled
        | (0 << TCB_FILTER_bp);  // FILTER disabled

    TCB3.CCMP = 0xFA00U;  // CCMP 0xFA00

    TCB3.CNT = 0x0;  // CNT 0xNAN

    TCB3.INTFLAGS = (0 << TCB_CAPT_bp)   // CAPT disabled
        | (0 << TCB_OVF_bp);  // OVF disabled
    
	/* cppcheck-suppress misra-c2012-8.7 */
    TCB3_OverflowCallbackRegister(TCB3_DefaultOverflowCallback);
	/* cppcheck-suppress misra-c2012-8.7 */
    TCB3_CaptureCallbackRegister(TCB3_DefaultCaptureCallback);

    TCB3.INTCTRL = (1 << TCB_CAPT_bp)   // CAPT enabled
        | (0 << TCB_OVF_bp);  // OVF disabled

    TCB3.CTRLA = (0 << TCB_CASCADE_bp)   // CASCADE disabled
        | (TCB_CLKSEL_DIV2_gc)   // CLKSEL DIV2
        | (1 << TCB_ENABLE_bp)   // ENABLE enabled
        | (0 << TCB_RUNSTDBY_bp)   // RUNSTDBY disabled
        | (0 << TCB_SYNCUPD_bp);  // SYNCUPD disabled
}

void TCB3_Deinitialize(void)
{
    TCB3.CTRLA &= ~TCB_ENABLE_bm; /* Stop Timer */
    
    TCB3.CCMP = 0x0;

    TCB3.CNT = 0x0;
    
    TCB3.CTRLB = 0x0;

    TCB3.DBGCTRL = 0x0;

    TCB3.EVCTRL = 0x0;

    TCB3.INTCTRL = 0x0;

    TCB3.TEMP = 0x0;

    TCB3.CTRLA = 0x0;
    
    TCB3.INTFLAGS = ~0x0;
}

void TCB3_Start(void)
{
    TCB3.CTRLA |= TCB_ENABLE_bm; /* Start Timer */
}

void TCB3_Stop(void)
{
    TCB3.CTRLA &= ~TCB_ENABLE_bm; /* Stop Timer */
}

void TCB3_CounterSet(uint16_t timerVal)
{
    TCB3.CNT = timerVal;
}

uint16_t TCB3_CounterGet(void)
{
    uint16_t readVal;

    readVal = TCB3.CNT;

    return readVal;
}
void TCB3_PeriodSet(uint16_t periodVal)
{
    TCB3.CCMP = (uint16_t) periodVal;
}

uint16_t TCB3_PeriodGet(void)
{
    return (uint16_t)TCB3.CCMP;
}

uint16_t TCB3_MaxCountGet(void)
{
    return TCB3_MAX_COUNT;
}

void TCB3_CAPTInterruptEnable(void)
{
    TCB3.INTCTRL |= TCB_CAPT_bm; /* Capture or Timeout: enabled */
}

void TCB3_CAPTInterruptDisable(void)
{
    TCB3.INTCTRL &= ~TCB_CAPT_bm; /* Capture or Timeout: disabled */
}

void TCB3_OVFInterruptEnable(void)
{
	TCB3.INTCTRL |= TCB_OVF_bm; /* Overflow Interrupt: enabled */
}

void TCB3_OVFInterruptDisable(void)
{
	TCB3.INTCTRL &= ~TCB_OVF_bm; /* Overflow Interrupt: disabled */
}


bool TCB3_IsCaptInterruptEnabled(void)
{
    return ((TCB3.INTCTRL & TCB_CAPT_bm) > 0);
}

bool TCB3_IsOvfInterruptEnabled(void)
{
    return ((TCB3.INTCTRL & TCB_OVF_bm) > 0);
}


/* cppcheck-suppress misra-c2012-8.7 */
void TCB3_OverflowCallbackRegister(void (* CallbackHandler)(void))
{
	TCB3_OVF_isr_cb = CallbackHandler;
}

static void TCB3_DefaultOverflowCallback(void)
{
    //Add your interrupt code here or
    //Use TCB3_OverflowCallbackRegister function to use Custom ISR
}

/* cppcheck-suppress misra-c2012-8.7 */
void TCB3_CaptureCallbackRegister(void (* CallbackHandler)(void))
{
	TCB3_CAPT_isr_cb = CallbackHandler;
}

static void TCB3_DefaultCaptureCallback(void)
{
    //Add your Capture interrupt code here or
    //Use TCB3_CaptureCallbackRegister function to use Custom ISR
}

/* cppcheck-suppress misra-c2012-2.7 */
/* cppcheck-suppress misra-c2012-8.2 */
/* cppcheck-suppress misra-c2012-8.4 */
ISR(TCB3_INT_vect)
{
	if (0U != (TCB3.INTFLAGS & TCB_CAPT_bm))
    {
        if (TCB3_CAPT_isr_cb != NULL)
        {
            (*TCB3_CAPT_isr_cb)();
        }
        TCB3.INTFLAGS = TCB_CAPT_bm;
    }
    if (0U != (TCB3.INTFLAGS & TCB_OVF_bm))
    {
        if (TCB3_OVF_isr_cb != NULL)
        {
            (*TCB3_OVF_isr_cb)();
        }
        TCB3.INTFLAGS = TCB_OVF_bm;
    }
}

