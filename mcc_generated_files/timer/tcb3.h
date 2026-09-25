/**
 * TCB3 Generated Driver API Header File
 *
 * @file tcb3.h
 *
 * @defgroup tcb3 TCB3
 *
 * @brief  This file contains the API prototypes and other data types for TCB3.
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

#ifndef TCB3_H_INCLUDED
#define TCB3_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include "../system/utils/compiler.h"
#include "tcb3_deprecated.h"

/**
 * @misradeviation{@advisory,2.5}
 * MCC Melody drivers provide macros that can be added to an application. 
 * It depends on the application whether a macro is used or not. 
 */

/**
 * @ingroup tcb3
 * @brief Defines the TCB3 maximum count value
 */
#define TCB3_MAX_COUNT (65535U)
/**
 * @ingroup tcb3
 * @brief Defines the TCB3 frequency in hertz.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_CLOCK_FREQ (8000000UL)
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_MAX_COUNT.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_MAX_COUNT TCB3_MAX_COUNT
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CLOCK_FREQ.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CLOCK_FREQ TCB3_CLOCK_FREQ
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_Initialize API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_Initialize TCB3_Initialize
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_Deinitialize API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_Deinitialize TCB3_Deinitialize
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_Start API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_Start TCB3_Start
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_Stop API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_Stop TCB3_Stop
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_CounterGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CounterGet TCB3_CounterGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_CounterSet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CounterSet TCB3_CounterSet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_PeriodSet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_PeriodSet TCB3_PeriodSet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_PeriodGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_PeriodGet TCB3_PeriodGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_MaxCountGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_MaxCountGet TCB3_MaxCountGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_CAPTInterruptEnable API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CAPTInterruptEnable TCB3_CAPTInterruptEnable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_EnableOvfInterrupt API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_OVFInterruptEnable TCB3_OVFInterruptEnable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_CAPTInterruptDisable API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CAPTInterruptDisable TCB3_CAPTInterruptDisable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_OVFInterruptDisable API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_OVFInterruptDisable TCB3_OVFInterruptDisable
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_IsOvfInterruptFlag API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_OverflowStatusGet TCB3_OverflowStatusGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_CaptureStatusGet API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CaptureStatusGet TCB3_CaptureStatusGet
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_ClearCaptInterruptFlag API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CaptureStatusClear TCB3_CaptureStatusClear
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_OverflowStatusClear API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_OverflowStatusClear TCB3_OverflowStatusClear
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_OverflowCallbackRegister API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_OverflowCallbackRegister TCB3_OverflowCallbackRegister
/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCB3_CaptureCallbackRegister API
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_CaptureCallbackRegister TCB3_CaptureCallbackRegister
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_IsCaptInterruptEnabled API.
 *        This macro will be deprecated in the future release.
 */
 /* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_IsCaptInterruptEnabled TCB3_IsCaptInterruptEnabled
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_IsOvfInterruptEnabled API.
 *        This macro will be deprecated in the future release.
 */
 /* cppcheck-suppress misra-c2012-2.5 */ 
#define SENSOR_REQUEST_TIMEOUT_IsOvfInterruptEnabled TCB3_IsOvfInterruptEnabled

/**
 * @ingroup tcb3
 * @brief Registers a callback function to be called during a capture event.
 * @param void (* CallbackHandler)(void) - Pointer to the custom callback
 * @return None.
 */
void TCB3_CaptureCallbackRegister(void (* CallbackHandler)(void));

/**
 * @ingroup tcb3
 * @brief Registers a callback function to be called during an overflow event.
 * @param void (* CallbackHandler)(void) - Pointer to the custom callback
 * @return None.
 */
 void TCB3_OverflowCallbackRegister(void (* CallbackHandler)(void));

/**
 * @ingroup tcb3
 * @brief Initializes the TCB3 module
 * @param None.
 * @return None.
 */
void TCB3_Initialize(void);

/**
 * @ingroup tcb3
 * @brief Deinitializes the TCB3 module.
 * @param None.
 * @return None.
 */
void TCB3_Deinitialize(void);

/**
 * @ingroup tcb3
 * @brief Starts the TCB3 counter.
 * @param None.
 * @return None.
 */
void TCB3_Start(void);

/**
 * @ingroup tcb3
 * @brief Stops the TCB3 counter.
 * @param None.
 * @return None.
 */
void TCB3_Stop(void);

/**
 * @ingroup tcb3
 * @brief Writes the counter value to the CNT register.
 * @pre Initialize TCB3 with TCB3_Initialize() before calling this API.
 * @param counterValue - Counter value to be written to the CNT register
 * @return None.
 */
void TCB3_CounterSet(uint16_t counterValue);

/**
 * @ingroup tcb3
 * @brief Reads the counter value from the CNT register.
 * @pre Initialize TCB3 with TCB3_Initialize() before calling this API.
 * @param None.
 * @return Counter value from the CNT register
 */
uint16_t TCB3_CounterGet(void);

/**
 * @ingroup tcb3
 * @brief Loads the period count value to the CCMP register.
 * @pre Initialize TCB3 with TCB3_Initialize() before calling this API.
 * @param periodVal - Count value written to the CCMP register
 * @return None.
 */
void TCB3_PeriodSet(uint16_t periodVal);

/**
 * @ingroup tcb3
 * @brief Gets the current period value from the CCMP register.
 * @pre Initialize TCB3 with TCB3_Initialize() before calling this API.
 * @param None.
 * @return Period value from the CCMP register
 */
uint16_t TCB3_PeriodGet(void);

/**
 * @ingroup tcb3
 * @brief Gets the maximum timer count value.
 * @param None.
 * @return Maximum count value
 */
uint16_t TCB3_MaxCountGet(void);

/**
 * @ingroup tcb3
 * @brief Enables the capture interrupt for the TCB3.
 * @param None.
 * @return None.
 */
void TCB3_CAPTInterruptEnable(void);

/**
 * @ingroup tcb3
 * @brief Disables the capture interrupt for the TCB3.
 * @param None.
 * @return None.
 */
void TCB3_CAPTInterruptDisable(void);

/**
 * @ingroup tcb3
 * @brief Enables the overflow interrupt for the TCB3.
 * @param None.
 * @return None.
 */
void TCB3_OVFInterruptEnable(void);

/**
 * @ingroup tcb3
 * @brief Disables the overflow interrupt for the TCB3.
 * @param None.
 * @return None.
 */
void TCB3_OVFInterruptDisable(void);


/**
 * @ingroup tcb3
 * @brief Checks if the capture interrupt is enabled.
 * @param None.
 * @return None.
 */
bool TCB3_IsCaptInterruptEnabled(void) __attribute__((deprecated("This function will be deprecated in future")));

/**
 * @ingroup tcb3
 * @brief Checks if the overflow interrupt is enabled.
 * @param None.
 * @return None.
 */
bool TCB3_IsOvfInterruptEnabled(void) __attribute__((deprecated("This function will be deprecated in future")));




#endif /* TCB3_H_INCLUDED */