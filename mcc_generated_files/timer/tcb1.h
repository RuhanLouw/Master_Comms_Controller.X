/**
 * TCB1 Generated Driver API Header File
 *
 * @file tcb1.h
 *
 * @defgroup tcb1 TCB1
 *
 * @brief  This file contains the API prototypes and other data types for TCB1.
 *
 * @version TCB1 Driver Version 2.0.0
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

#ifndef TCB1_H_INCLUDED
#define TCB1_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include "../system/utils/compiler.h"
#include "tcb1_deprecated.h"

/**
 * @misradeviation{@advisory,2.5}
 * MCC Melody drivers provide macros that can be added to an application. 
 * It depends on the application whether a macro is used or not. 
 */

/**
 * @ingroup tcb1
 * @brief Defines the TCB1 maximum count value
 */
#define TCB1_MAX_COUNT (65535U)
/**
 * @ingroup tcb1
 * @brief Defines the TCB1 frequency in hertz.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB1_CLOCK_FREQ (8000000UL)
/**
 * @ingroup tcb1
 * @brief Defines the Custom Name for the \ref TCB1_MAX_COUNT.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_MAX_COUNT TCB1_MAX_COUNT
/**
 * @ingroup tcb1
 * @brief Defines the Custom Name for the \ref TCB1_CLOCK_FREQ.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CLOCK_FREQ TCB1_CLOCK_FREQ
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_Initialize API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_Initialize TCB1_Initialize
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_Deinitialize API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_Deinitialize TCB1_Deinitialize
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_Start API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_Start TCB1_Start
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_Stop API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_Stop TCB1_Stop
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_CounterGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CounterGet TCB1_CounterGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_CounterSet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CounterSet TCB1_CounterSet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_PeriodSet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_PeriodSet TCB1_PeriodSet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_PeriodGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_PeriodGet TCB1_PeriodGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_MaxCountGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_MaxCountGet TCB1_MaxCountGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_CAPTInterruptEnable API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CAPTInterruptEnable TCB1_CAPTInterruptEnable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_EnableOvfInterrupt API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_OVFInterruptEnable TCB1_OVFInterruptEnable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_CAPTInterruptDisable API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CAPTInterruptDisable TCB1_CAPTInterruptDisable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_OVFInterruptDisable API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_OVFInterruptDisable TCB1_OVFInterruptDisable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_IsOvfInterruptFlag API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_OverflowStatusGet TCB1_OverflowStatusGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_CaptureStatusGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CaptureStatusGet TCB1_CaptureStatusGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_ClearCaptInterruptFlag API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CaptureStatusClear TCB1_CaptureStatusClear
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_OverflowStatusClear API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_OverflowStatusClear TCB1_OverflowStatusClear
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_OverflowCallbackRegister API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_OverflowCallbackRegister TCB1_OverflowCallbackRegister
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB1_CaptureCallbackRegister API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_CaptureCallbackRegister TCB1_CaptureCallbackRegister
/**
 * @ingroup tcb1
 * @brief Defines the Custom Name for the \ref TCB1_IsCaptInterruptEnabled API.
 *        This macro will be deprecated in the future release.
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_RX_TIMEOUT_IsCaptInterruptEnabled TCB1_IsCaptInterruptEnabled
/**
 * @ingroup tcb1
 * @brief Defines the Custom Name for the \ref TCB1_IsOvfInterruptEnabled API.
 *        This macro will be deprecated in the future release.
 */
 /* cppcheck-suppress misra-c2012-2.5 */ 
#define SENSOR_RX_TIMEOUT_IsOvfInterruptEnabled TCB1_IsOvfInterruptEnabled

/**
 * @ingroup tcb1
 * @brief Registers a callback function to be called during a capture event.
 * @param void (* CallbackHandler)(void) - Pointer to the custom callback
 * @return None.
 */
void TCB1_CaptureCallbackRegister(void (* CallbackHandler)(void));

/**
 * @ingroup tcb1
 * @brief Registers a callback function to be called during an overflow event.
 * @param void (* CallbackHandler)(void) - Pointer to the custom callback
 * @return None.
 */
 void TCB1_OverflowCallbackRegister(void (* CallbackHandler)(void));

/**
 * @ingroup tcb1
 * @brief Initializes the TCB1 module
 * @param None.
 * @return None.
 */
void TCB1_Initialize(void);

/**
 * @ingroup tcb1
 * @brief Deinitializes the TCB1 module.
 * @param None.
 * @return None.
 */
void TCB1_Deinitialize(void);

/**
 * @ingroup tcb1
 * @brief Starts the TCB1 counter.
 * @param None.
 * @return None.
 */
void TCB1_Start(void);

/**
 * @ingroup tcb1
 * @brief Stops the TCB1 counter.
 * @param None.
 * @return None.
 */
void TCB1_Stop(void);

/**
 * @ingroup tcb1
 * @brief Writes the counter value to the CNT register.
 * @pre Initialize TCB1 with TCB1_Initialize() before calling this API.
 * @param counterValue - Counter value to be written to the CNT register
 * @return None.
 */
void TCB1_CounterSet(uint16_t counterValue);

/**
 * @ingroup tcb1
 * @brief Reads the counter value from the CNT register.
 * @pre Initialize TCB1 with TCB1_Initialize() before calling this API.
 * @param None.
 * @return Counter value from the CNT register
 */
uint16_t TCB1_CounterGet(void);

/**
 * @ingroup tcb1
 * @brief Loads the period count value to the CCMP register.
 * @pre Initialize TCB1 with TCB1_Initialize() before calling this API.
 * @param periodVal - Count value written to the CCMP register
 * @return None.
 */
void TCB1_PeriodSet(uint16_t periodVal);

/**
 * @ingroup tcb1
 * @brief Gets the current period value from the CCMP register.
 * @pre Initialize TCB1 with TCB1_Initialize() before calling this API.
 * @param None.
 * @return Period value from the CCMP register
 */
uint16_t TCB1_PeriodGet(void);

/**
 * @ingroup tcb1
 * @brief Gets the maximum timer count value.
 * @param None.
 * @return Maximum count value
 */
uint16_t TCB1_MaxCountGet(void);

/**
 * @ingroup tcb1
 * @brief Enables the capture interrupt for the TCB1.
 * @param None.
 * @return None.
 */
void TCB1_CAPTInterruptEnable(void);

/**
 * @ingroup tcb1
 * @brief Disables the capture interrupt for the TCB1.
 * @param None.
 * @return None.
 */
void TCB1_CAPTInterruptDisable(void);

/**
 * @ingroup tcb1
 * @brief Enables the overflow interrupt for the TCB1.
 * @param None.
 * @return None.
 */
void TCB1_OVFInterruptEnable(void);

/**
 * @ingroup tcb1
 * @brief Disables the overflow interrupt for the TCB1.
 * @param None.
 * @return None.
 */
void TCB1_OVFInterruptDisable(void);


/**
 * @ingroup tcb1
 * @brief Checks if the capture interrupt is enabled.
 * @param None.
 * @return None.
 */
bool TCB1_IsCaptInterruptEnabled(void) __attribute__((deprecated("This function will be deprecated in future")));

/**
 * @ingroup tcb1
 * @brief Checks if the overflow interrupt is enabled.
 * @param None.
 * @return None.
 */
bool TCB1_IsOvfInterruptEnabled(void) __attribute__((deprecated("This function will be deprecated in future")));




#endif /* TCB1_H_INCLUDED */