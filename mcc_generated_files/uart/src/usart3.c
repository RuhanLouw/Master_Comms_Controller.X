/**
 * USART3 Generated Driver API Header File
 * 
 * @file usart3.c
 * 
 * @ingroup usart3
 * 
 * @brief This is the generated driver implementation file for the USART3 driver using the  Universal Synchronous and Asynchronous serial Receiver and Transmitter (USART) module. 
 *
 * @version USART3 Driver Version 2.1.2
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

/**
  Section: Included Files
*/

#include "../usart3.h"

/**
  Section: Macro Declarations
*/

#define USART3_TX_BUFFER_SIZE (1024U) //buffer size should be 2^n
#define USART3_TX_BUFFER_MASK (USART3_TX_BUFFER_SIZE - 1U) 

#define USART3_RX_BUFFER_SIZE (8U) //buffer size should be 2^n
#define USART3_RX_BUFFER_MASK (USART3_RX_BUFFER_SIZE - 1U)



/**
  Section: Driver Interface
 */

const uart_drv_interface_t UART3 = {
    .Initialize = &USART3_Initialize,
    .Deinitialize = &USART3_Deinitialize,
    .Read = &USART3_Read,
    .Write = &USART3_Write,
    .IsRxReady = &USART3_IsRxReady,
    .IsTxReady = &USART3_IsTxReady,
    .IsTxDone = &USART3_IsTxDone,
    .TransmitEnable = &USART3_TransmitEnable,
    .TransmitDisable = &USART3_TransmitDisable,
    .AutoBaudSet = &USART3_AutoBaudSet,
    .AutoBaudQuery = &USART3_AutoBaudQuery,
    .BRGCountSet = NULL,
    .BRGCountGet = NULL,
    .BaudRateSet = NULL,
    .BaudRateGet = NULL,
    .AutoBaudEventEnableGet = NULL,
    .ErrorGet = &USART3_ErrorGet,
    .TxCompleteCallbackRegister = &USART3_TxCompleteCallbackRegister,
    .RxCompleteCallbackRegister = &USART3_RxCompleteCallbackRegister,
    .TxCollisionCallbackRegister = NULL,
    .FramingErrorCallbackRegister = &USART3_FramingErrorCallbackRegister,
    .OverrunErrorCallbackRegister = &USART3_OverrunErrorCallbackRegister,
    .ParityErrorCallbackRegister = &USART3_ParityErrorCallbackRegister,
    .EventCallbackRegister = NULL,
};

/**
  Section: USART3 variables
*/
static volatile uint16_t usart3TxHead = 0;
static volatile uint16_t usart3TxTail = 0;
static volatile uint16_t usart3TxBufferRemaining;
static volatile uint8_t usart3TxBuffer[USART3_TX_BUFFER_SIZE];
static volatile bool usart3IsTxComplete;
static volatile uint8_t usart3RxHead = 0;
static volatile uint8_t usart3RxTail = 0;
static volatile uint8_t usart3RxCount;
static volatile uint8_t usart3RxBuffer[USART3_RX_BUFFER_SIZE];
/**
 * @misradeviation{@advisory,19.2}
 * The UART error status necessitates checking the bit field and accessing the status within the group byte therefore the use of a union is essential.
 */
  /* cppcheck-suppress misra-c2012-19.2 */
static volatile usart3_status_t usart3RxStatusBuffer[USART3_RX_BUFFER_SIZE];
 /* cppcheck-suppress misra-c2012-19.2 */
static volatile usart3_status_t usart3RxLastError;

/**
  Section: USART3 APIs
*/
static void (*USART3_FramingErrorHandler)(void);
static void (*USART3_OverrunErrorHandler)(void);
static void (*USART3_ParityErrorHandler)(void);
void (*USART3_TxInterruptHandler)(void);
/* cppcheck-suppress misra-c2012-8.9 */
static void (*USART3_TxCompleteInterruptHandler)(void) = NULL;
void (*USART3_RxInterruptHandler)(void);
static void (*USART3_RxCompleteInterruptHandler)(void) = NULL;

static void USART3_DefaultFramingErrorCallback(void);
static void USART3_DefaultOverrunErrorCallback(void);
static void USART3_DefaultParityErrorCallback(void);
void USART3_TransmitISR (void);
void USART3_ReceiveISR(void);



/**
  Section: USART3  APIs
*/

void USART3_Initialize(void)
{
    USART3_RxInterruptHandler = USART3_ReceiveISR;  
    USART3_TxInterruptHandler = USART3_TransmitISR;

    // Set the USART3 module to the options selected in the user interface.

    //BAUD 6666; 
    USART3.BAUD = (uint16_t)USART3_BAUD_RATE(9600UL);
	
    // ABEIE disabled; DREIE disabled; LBME disabled; RS485 DISABLE; RXCIE enabled; RXSIE enabled; TXCIE enabled; 
    USART3.CTRLA = 0xD0;
	
    // MPCM disabled; ODME disabled; RXEN enabled; RXMODE NORMAL; SFDEN disabled; TXEN enabled; 
    USART3.CTRLB = 0xC0;
	
    // CMODE Asynchronous Mode; UCPHA enabled; UDORD disabled; CHSIZE Character size: 8 bit; PMODE No Parity; SBMODE 1 stop bit; 
    USART3.CTRLC = 0x3;
	
    //DBGRUN disabled; 
    USART3.DBGCTRL = 0x0;
	
    //IREI disabled; 
    USART3.EVCTRL = 0x0;
	
    //RXPL 0x0; 
    USART3.RXPLCTRL = 0x0;
	
    //TXPL 0x0; 
    USART3.TXPLCTRL = 0x0;
	
    USART3_FramingErrorCallbackRegister(USART3_DefaultFramingErrorCallback);
    USART3_OverrunErrorCallbackRegister(USART3_DefaultOverrunErrorCallback);
    USART3_ParityErrorCallbackRegister(USART3_DefaultParityErrorCallback);
    usart3RxLastError.status = 0;  
    usart3TxHead = 0;
    usart3TxTail = 0;
    usart3TxBufferRemaining = sizeof(usart3TxBuffer);
    usart3IsTxComplete = true;
    usart3RxHead = 0;
    usart3RxTail = 0;
    usart3RxCount = 0;
    USART3.CTRLA |= USART_RXCIE_bm; 

}

void USART3_Deinitialize(void)
{
    USART3.CTRLA &= ~(USART_RXCIE_bm);    
    USART3.CTRLA &= ~(USART_DREIE_bm);  
    USART3.BAUD = 0x00;	
    USART3.CTRLA = 0x00;	
    USART3.CTRLB = 0x00;	
    USART3.CTRLC = 0x00;	
    USART3.DBGCTRL = 0x00;	
    USART3.EVCTRL = 0x00;	
    USART3.RXPLCTRL = 0x00;	
    USART3.TXPLCTRL = 0x00;	
}

void USART3_Enable(void)
{
    USART3.CTRLB |= USART_RXEN_bm | USART_TXEN_bm; 
}

void USART3_Disable(void)
{
    USART3.CTRLB &= ~(USART_RXEN_bm | USART_TXEN_bm); 
}

void USART3_TransmitEnable(void)
{
    USART3.CTRLB |= USART_TXEN_bm; 
}

void USART3_TransmitDisable(void)
{
    USART3.CTRLB &= ~(USART_TXEN_bm); 
}

void USART3_ReceiveEnable(void)
{
    USART3.CTRLB |= USART_RXEN_bm ; 
}

void USART3_ReceiveDisable(void)
{
    USART3.CTRLB &= ~(USART_RXEN_bm); 
}

void USART3_AutoBaudSet(bool enable)
{
    if(enable)
    {
        USART3.CTRLB |= USART_RXMODE_gm & (0x02 << USART_RXMODE_gp); 
        USART3.STATUS |= USART_WFB_bm ; 
    }
    else
    {
       USART3.CTRLB &= ~(USART_RXMODE_gm); 
       USART3.STATUS &= ~(USART_BDF_bm);  
    }
}

bool USART3_AutoBaudQuery(void)
{
     return (bool)(USART3.STATUS & USART_BDF_bm) ; 
}

bool USART3_IsAutoBaudDetectError(void)
{
     return (bool)(USART3.STATUS & USART_ISFIF_bm) ; 
}

void USART3_AutoBaudDetectErrorReset(void)
{
    USART3.STATUS |= USART_ISFIF_bm ;
	USART3_AutoBaudSet(false);
    USART3_ReceiveDisable();
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
    USART3_ReceiveEnable();
    USART3_AutoBaudSet(true);
}

void USART3_TransmitInterruptEnable(void)
{
    USART3.CTRLA |= USART_DREIE_bm ; 
}

void USART3_TransmitInterruptDisable(void)
{ 
    USART3.CTRLA &= ~(USART_DREIE_bm); 
}

void USART3_ReceiveInterruptEnable(void)
{
    USART3.CTRLA |= USART_RXCIE_bm ; 
}
void USART3_ReceiveInterruptDisable(void)
{
    USART3.CTRLA &= ~(USART_RXCIE_bm); 
}

bool USART3_IsRxReady(void)
{
    return (usart3RxCount ? true : false);
}

bool USART3_IsTxReady(void)
{
    return (usart3TxBufferRemaining ? true : false);
}

bool USART3_IsTxDone(void)
{
    bool usart3TxCompleteStatus = false;
    usart3TxCompleteStatus = usart3IsTxComplete;
    usart3IsTxComplete = false;
    return usart3TxCompleteStatus;
}

size_t USART3_ErrorGet(void)
{
    usart3RxLastError.status = usart3RxStatusBuffer[usart3RxTail & USART3_RX_BUFFER_MASK].status;
    return usart3RxLastError.status;
}

uint8_t USART3_Read(void)
{
    uint8_t readValue  = 0;
    uint8_t tempRxTail;
    
    readValue = usart3RxBuffer[usart3RxTail];
    tempRxTail = (usart3RxTail + 1U) & USART3_RX_BUFFER_MASK; // Buffer size of RX should be in the 2^n  
    usart3RxTail = tempRxTail;
    USART3.CTRLA &= ~(USART_RXCIE_bm); 
    if(0U != usart3RxCount)
    {
        usart3RxCount--;
    }
    USART3.CTRLA |= USART_RXCIE_bm; 


    return readValue;
}

/* Interrupt service routine for RX complete */
/* cppcheck-suppress misra-c2012-2.7 */
/* cppcheck-suppress misra-c2012-8.4 */
ISR(USART3_RXC_vect)
/* cppcheck-suppress misra-c2012-5.5 */
{
    USART3_RxInterruptHandler();
}

void USART3_ReceiveISR(void)
{
    uint8_t regValue;
    uint8_t tempRxHead;
    
    usart3RxStatusBuffer[usart3RxHead].status = 0;

    if(USART_FERR_bm == (USART3.RXDATAH & USART_FERR_bm))
    {
        usart3RxStatusBuffer[usart3RxHead].ferr = 1;
        if(NULL != USART3_FramingErrorHandler)
        {
            USART3_FramingErrorHandler();
        } 
    }
    if(USART_PERR_bm == (USART3.RXDATAH & USART_PERR_bm))
    {
        usart3RxLastError.perr = 1;
        if(NULL != USART3_ParityErrorHandler)
        {
            USART3_ParityErrorHandler();
        }  
    }
    if(USART_BUFOVF_bm == (USART3.RXDATAH & USART_BUFOVF_bm))
    {
        usart3RxStatusBuffer[usart3RxHead].oerr = 1;
        if(NULL != USART3_OverrunErrorHandler)
        {
            USART3_OverrunErrorHandler();
        }   
    }    
    
    regValue = USART3.RXDATAL;
    
    tempRxHead = (usart3RxHead + 1U) & USART3_RX_BUFFER_MASK;// Buffer size of RX should be in the 2^n
    if (tempRxHead == usart3RxTail) {
		// ERROR! Receive buffer overflow 
	} 
    else
    {
        // Store received data in buffer 
		usart3RxBuffer[usart3RxHead] = regValue;
		usart3RxHead = tempRxHead;

		usart3RxCount++;
	}
    if (NULL != USART3_RxCompleteInterruptHandler)
    {
        (*USART3_RxCompleteInterruptHandler)();
    }
    
    else {
        // Do Nothing. Added for MISRA C Compliant.
    }
}

void USART3_Write(uint8_t txData)
{
    uint16_t tempTxHead;
    
    if(0U < usart3TxBufferRemaining) // check if at least one byte place is available in TX buffer
    {
       usart3TxBuffer[usart3TxHead] = txData;
       tempTxHead = (usart3TxHead + 1U) & USART3_TX_BUFFER_MASK;// Buffer size of TX should be in the 2^n
       
       usart3TxHead = tempTxHead;
       USART3.CTRLA &= ~(USART_DREIE_bm);  //Critical value decrement
       usart3TxBufferRemaining--;  // one less byte remaining in TX buffer
    }
    else
    {
        //overflow condition; TX buffer is full
    }

    USART3.CTRLA |= USART_DREIE_bm;  
}

/* Interrupt service routine for Data Register Empty */
/* cppcheck-suppress misra-c2012-2.7 */
/* cppcheck-suppress misra-c2012-8.4 */
ISR(USART3_DRE_vect)
/* cppcheck-suppress misra-c2012-5.5 */
{
    USART3_TxInterruptHandler();
}

/* Interrupt service routine for shift register and data register empty */
/* cppcheck-suppress misra-c2012-2.7 */
/* cppcheck-suppress misra-c2012-8.4 */
ISR(USART3_TXC_vect)
/* cppcheck-suppress misra-c2012-5.5 */
{
    usart3IsTxComplete = (bool)(USART3.STATUS & USART_TXCIF_bm);

    if (NULL != USART3_TxCompleteInterruptHandler)
    {
        (*USART3_TxCompleteInterruptHandler)();
    }

    USART3.STATUS |= USART_TXCIF_bm;
}

void USART3_TransmitISR(void)
{
    uint16_t tempTxTail;

    // use this default transmit interrupt handler code
    if(sizeof(usart3TxBuffer) > usart3TxBufferRemaining) // check if all data is transmitted
    {
       USART3.TXDATAL = usart3TxBuffer[usart3TxTail];

       tempTxTail = (usart3TxTail + 1U) & USART3_TX_BUFFER_MASK;// Buffer size of TX should be in the 2^n
       
       usart3TxTail = tempTxTail;

       usart3TxBufferRemaining++; // one byte sent, so 1 more byte place is available in TX buffer
    }
    else
    {
        USART3.CTRLA &= ~(USART_DREIE_bm); 
    }
}

static void USART3_DefaultFramingErrorCallback(void)
{
    
}

static void USART3_DefaultOverrunErrorCallback(void)
{
    
}

static void USART3_DefaultParityErrorCallback(void)
{
    
}

void USART3_FramingErrorCallbackRegister(void (* callbackHandler)(void))
{
    if(NULL != callbackHandler)
    {
        USART3_FramingErrorHandler = callbackHandler;
    }
}

void USART3_OverrunErrorCallbackRegister(void (* callbackHandler)(void))
{
    if(NULL != callbackHandler)
    {
        USART3_OverrunErrorHandler = callbackHandler;
    }    
}

void USART3_ParityErrorCallbackRegister(void (* callbackHandler)(void))
{
    if(NULL != callbackHandler)
    {
        USART3_ParityErrorHandler = callbackHandler;
    } 
}

void USART3_RxCompleteCallbackRegister(void (* callbackHandler)(void))
{
    if(NULL != callbackHandler)
    {
       USART3_RxCompleteInterruptHandler = callbackHandler; 
    }   
}

void USART3_TxCompleteCallbackRegister(void (* callbackHandler)(void))
{
    if(NULL != callbackHandler)
    {
       USART3_TxCompleteInterruptHandler = callbackHandler;
    }   
}


