/******************************************************************************
* File Name: main.c
*
* Description: This is the source code for the PSoC 6 MCU Watchdog Timer Example.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2019-2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* WDT demo options */
#define WDT_RESET_DEMO                      1
#define WDT_INTERRUPT_DEMO                  2

/* WDT Interrupt Number */
#define WDT_IRQ__INTC_NUMBER                srss_interrupt_IRQn
#define WDT_IRQ__INTC_CORTEXM4_PRIORITY     7

/* ILO Frequency in Hz */
#define ILO_FREQUENCY_HZ					32000

/* WDT interrupt period in milliseconds. Max limit is 2047ms. */
#define WDT_INTERRUPT_INTERVAL_MS           1000

/* Match count =  Desired interrupt interval in seconds x ILO Frequency in Hz */
#define WDT_MATCH_COUNT                     (WDT_INTERRUPT_INTERVAL_MS * ILO_FREQUENCY_HZ / 1000)

/* WDT time out for reset mode, in milliseconds. Max limit is given by CYHAL_WDT_MAX_TIMEOUT_MS */
#define WDT_TIME_OUT_MS                     4000

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void InitializeWDT(void);
void WdtInterruptHandler(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* WDT interrupt configuration structure */
const cy_stc_sysint_t WDT_IRQ_cfg = {
    .intrSrc = (IRQn_Type) WDT_IRQ__INTC_NUMBER,
    .intrPriority = WDT_IRQ__INTC_CORTEXM4_PRIORITY
};

/* WDT object */
cyhal_wdt_t wdt_obj;

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. It configures the WDT in either
* reset mode or periodic interrupt mode.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    
    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the User LED */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    
    /* Check the reason for device restart */
    if (CYHAL_SYSTEM_RESET_WDT == cyhal_system_get_reset_reason())
    {
        /* It's WDT reset event - blink LED twice */
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
        cyhal_system_delay_ms(100);
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
        cyhal_system_delay_ms(200);
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
        cyhal_system_delay_ms(100);
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
    }
    else
    {
        /* It's Power-On reset or XRES event - blink LED once */
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
        cyhal_system_delay_ms(100);
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);;
        cyhal_system_delay_ms(100);
    }

    /* Initialize WDT */
    InitializeWDT();

    /* Enable global interrupt */
    __enable_irq();

    for (;;)
    {
        #if (WDT_DEMO == WDT_RESET_DEMO)
            /* Reset WDT */
            cyhal_wdt_kick(&wdt_obj);

            #if defined(EXECUTION_BLOCK)
            while(1);
            #endif

            /* Constant delay of 1000ms */
            cyhal_system_delay_ms(1000);

            /* Invert the state of LED */
            cyhal_gpio_toggle(CYBSP_USER_LED);
        #endif

        #if (WDT_DEMO == WDT_INTERRUPT_DEMO)
            #if defined(DEEPSLEEP_ENABLE)
            /* Code to demonstrate wake up from deep sleep */
            cyhal_syspm_deepsleep();
            #endif
        #endif
    }
}

/*******************************************************************************
* Function Name: InitializeWDT
********************************************************************************
* Summary:
* This function initializes the WDT block
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void InitializeWDT()
{
    cy_rslt_t result;

    #if (WDT_DEMO == WDT_INTERRUPT_DEMO)

        /* Step 1- Unlock WDT */
        Cy_WDT_Unlock();

        /* Step 2- Write the ignore bits - operate with full 16 bits */
        Cy_WDT_SetIgnoreBits(0);

        /* Step 3- Write match value */
        Cy_WDT_SetMatch(WDT_MATCH_COUNT);

        /* Step 4- Clear match event interrupt, if any */
        Cy_WDT_ClearInterrupt();

        /* Step 5- Enable ILO */
        Cy_SysClk_IloEnable();

        /* Step 6 - Enable interrupt if periodic interrupt mode selected */
        Cy_SysInt_Init(&WDT_IRQ_cfg, WdtInterruptHandler);
        NVIC_EnableIRQ(WDT_IRQ_cfg.intrSrc);
        Cy_WDT_UnmaskInterrupt();

        /* Step 7- Enable WDT */
        Cy_WDT_Enable();

        /* Step 8- Lock WDT configuration */
        Cy_WDT_Lock();
    #else /* Reset Demo */

        /* Initialize the WDT */
        result = cyhal_wdt_init(&wdt_obj, WDT_TIME_OUT_MS);

        /* WDT initialization failed. Stop program execution */
        if (result != CY_RSLT_SUCCESS)
        {
            CY_ASSERT(0);
        }

    #endif
}

/*******************************************************************************
* Function Name: WdtInterruptHandler
********************************************************************************
* Summary:
* This function is the handler for the WDT interrupt
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void WdtInterruptHandler(void)
{
    /* Check if the interrupt is from WDT */
    if (SRSS_SRSS_INTR & SRSS_SRSS_INTR_WDT_MATCH_Msk)
    {
        /* Clear WDT Interrupt */
        Cy_WDT_ClearInterrupt();

        /* Update the match count */
        Cy_WDT_Unlock();

        /* Update the match count. Note that, if the ignore bits in the match is set
         * to a non-zero value, the below code requires a modification. */
        Cy_WDT_SetMatch((uint16)(Cy_WDT_GetMatch() + WDT_MATCH_COUNT));
        Cy_WDT_Lock();

        /* Invert the state of LED */
        cyhal_gpio_toggle(CYBSP_USER_LED);
    }
}


/* [] END OF FILE */