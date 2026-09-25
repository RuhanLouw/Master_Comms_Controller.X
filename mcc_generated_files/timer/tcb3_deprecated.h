/**
 * TCB3 Generated Driver API Header File
 *
 * @file tcb3.h
 *
 * @defgroup tcb3 TCB3
 *
 * @brief This file contains the deprecated macros or APIs for the TCB3 driver.
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

#ifndef TCB3_DEPRECATED_H
#define TCB3_DEPRECATED_H

#warning "The tcb3_deprecated.h file contains the deprecated macros or functions. Replace the deprecated macro or functions with the recommended alternative."

/**
 * @misradeviation{@advisory,2.5}
 * MCC Melody drivers provide macros that can be added to an application. 
 * It depends on the application whether a macro is used or not. 
 */
 
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CounterGet API.
 *        The TCB3_Read will be deprecated in the future release. Use TCB3_CounterGet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_Read TCB3_CounterGet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CounterGet API.
 *        The SENSOR_REQUEST_TIMEOUT_Read will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CounterGet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_Read TCB3_CounterGet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CounterSet API.
 *        The TCB3_Write will be deprecated in the future release. Use TCB3_CounterSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_Write TCB3_CounterSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CounterSet API.
 *        The SENSOR_REQUEST_TIMEOUT_Write will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CounterSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_Write TCB3_CounterSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CAPTInterruptEnable API.
 *        The TCB3_EnableCaptInterrupt will be deprecated in the future release. Use TCB3_CAPTInterruptEnable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_EnableCaptInterrupt TCB3_CAPTInterruptEnable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CAPTInterruptEnable API.
 *        The SENSOR_REQUEST_TIMEOUT_EnableCaptInterrupt will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CAPTInterruptEnable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_EnableCaptInterrupt TCB3_CAPTInterruptEnable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CAPTInterruptDisable API.
 *        The TCB3_DisableCaptInterrupt will be deprecated in the future release. Use TCB3_CAPTInterruptDisable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_DisableCaptInterrupt TCB3_CAPTInterruptDisable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CAPTInterruptDisable API.
 *        The SENSOR_REQUEST_TIMEOUT_DisableCaptInterrupt will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CAPTInterruptDisable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_DisableCaptInterrupt TCB3_CAPTInterruptDisable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OVFInterruptEnable API.
 *        The TCB3_EnableOvfInterrupt will be deprecated in the future release. Use TCB3_OVFInterruptEnable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_EnableOvfInterrupt TCB3_OVFInterruptEnable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OVFInterruptEnable API.
 *        The SENSOR_REQUEST_TIMEOUT_EnableOvfInterrupt will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_OVFInterruptEnable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_EnableOvfInterrupt TCB3_OVFInterruptEnable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OVFInterruptDisable API.
 *        The TCB3_DisableOvfInterrupt will be deprecated in the future release. Use TCB3_OVFInterruptDisable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_DisableOvfInterrupt TCB3_OVFInterruptDisable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OVFInterruptDisable API.
 *        The SENSOR_REQUEST_TIMEOUT_DisableOvfInterrupt will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_OVFInterruptDisable instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_DisableOvfInterrupt TCB3_OVFInterruptDisable
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OverflowStatusClear API.
 *        The TCB3_ClearOvfInterruptFlag will be deprecated in the future release. Use TCB3_OverflowStatusClear instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_ClearOvfInterruptFlag TCB3_OverflowStatusClear
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OverflowStatusClear API.
 *        The SENSOR_REQUEST_TIMEOUT_ClearOvfInterruptFlag will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_OverflowStatusClear instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_ClearOvfInterruptFlag TCB3_OverflowStatusClear
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CaptureStatusClear API.
 *        The TCB3_ClearCaptInterruptFlag will be deprecated in the future release. Use TCB3_CaptureStatusClear instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_ClearCaptInterruptFlag TCB3_CaptureStatusClear
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CaptureStatusClear API.
 *        The SENSOR_REQUEST_TIMEOUT_ClearCaptInterruptFlag will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CaptureStatusClear instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_ClearCaptInterruptFlag TCB3_CaptureStatusClear
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OverflowStatusGet API.
 *        The TCB3_IsOvfInterruptFlag will be deprecated in the future release. Use TCB3_OverflowStatusGet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_IsOvfInterruptFlag TCB3_OverflowStatusGet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_OverflowStatusGet API.
 *        The SENSOR_REQUEST_TIMEOUT_IsOvfInterruptFlag will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_OverflowStatusGet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_IsOvfInterruptFlag TCB3_OverflowStatusGet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CaptureStatusGet API.
 *        The TCB3_IsCaptInterruptFlag will be deprecated in the future release. Use TCB3_CaptureStatusGet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_IsCaptInterruptFlag TCB3_CaptureStatusGet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CaptureStatusGet API.
 *        The SENSOR_REQUEST_TIMEOUT_IsCaptInterruptFlag will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CaptureStatusGet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_IsCaptInterruptFlag TCB3_CaptureStatusGet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_Start API.
 *        The TCB3_PWM_Enable will be deprecated in the future release. Use TCB3_Start instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_PWM_Enable TCB3_Start
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_Start API.
 *        The SENSOR_REQUEST_TIMEOUT_PWM_Enable will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_Start instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_PWM_Enable TCB3_Start
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_Stop API.
 *        The TCB3_PWM_Disable will be deprecated in the future release. Use TCB3_Stop instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_PWM_Disable TCB3_Stop
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_Stop API.
 *        The SENSOR_REQUEST_TIMEOUT_PWM_Disable will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_Stop instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_PWM_Disable TCB3_Stop
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CounterSet API.
 *        The TCB3_load_counter will be deprecated in the future release. Use TCB3_CounterSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_load_counter TCB3_CounterSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_CounterSet API.
 *        The SENSOR_REQUEST_TIMEOUT_load_counter will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_CounterSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_load_counter TCB3_CounterSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_PeriodSet API.
 *        The TCB3_load_top will be deprecated in the future release. Use TCB3_PeriodSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_load_top TCB3_PeriodSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_PeriodSet API.
 *        The SENSOR_REQUEST_TIMEOUT_load_top will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_PeriodSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_load_top TCB3_PeriodSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_DutyCycleSet API.
 *        The TCB3_load_duty_cycle will be deprecated in the future release. Use TCB3_DutyCycleSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_load_duty_cycle TCB3_DutyCycleSet
/**
 * @ingroup tcb3
 * @brief Defines the Custom Name for the \ref TCB3_DutyCycleSet API.
 *        The SENSOR_REQUEST_TIMEOUT_load_duty_cycle will be deprecated in the future release. Use SENSOR_REQUEST_TIMEOUT_DutyCycleSet instead.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define SENSOR_REQUEST_TIMEOUT_load_duty_cycle TCB3_DutyCycleSet
/**
 * @ingroup tcb3
 * @brief Defines a macro to handle deprecated custom data type.
 *        This macro will be deprecated in the future release.
 */
/* cppcheck-suppress misra-c2012-2.5 */
#define TCB3_pwm_register_t uint8_t

/**
 * @ingroup tcb3
 * @brief Defines a macro to handle deprecated custom data type.
 *        This macro will be deprecated in the future release.
 */
/* cppcheck-suppress misra-c2012-2.5 */ 
#define SENSOR_REQUEST_TIMEOUT_pwm_register_t uint8_t

#endif // TCB3_DEPRECATED_H