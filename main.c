/******************************************************************************
* File Name: main.c
*
* Description: This is the source code for the FreeRTOS
*              LE Throughput Example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
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
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
*        Header Files
*******************************************************************************/
#include "ble_client.h"
#include "wiced_bt_stack.h"
#include "cy_retarget_io.h"
#include <FreeRTOS.h>
#include <task.h>
#include "cycfg_bt_settings.h"
#include "cybsp_bt_config.h"

/*******************************************************************************
*        Macros
*******************************************************************************/
#define TASK_PRIORITY                   ( configMAX_PRIORITIES - 4 )
#define TASK_STACK_SIZE                 ( configMINIMAL_STACK_SIZE * 4 )
#define BUTTON_TASK_STRING                     "BLE button Task"
#define THROUGHPUT_TASK_STRING                 "Throughput Task"
#define MILLISEC_TASK_STRING                   "Millisec Task"

/*Handle for the task*/
TaskHandle_t ble_button_task_handle;
TaskHandle_t get_throughput_task_handle;
TaskHandle_t send_gatt_write_task_handle;

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/
/*
 *  Entry point to the application. Set device configuration and start BT
 *  stack initialization.  The actual application initialization will happen
 *  when stack reports that BT device is ready.
 */
int main()
{
    cy_rslt_t rslt;
    wiced_result_t result;
    BaseType_t rtos_result;

    /* Initialize the board support package */
    rslt = cybsp_init();
    if (CY_RSLT_SUCCESS != rslt)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX,
                        CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);
    cybt_platform_config_init(&cybsp_bt_platform_cfg);

    printf("**** BLE Throughput Measurement - Client Application Start ****\n\n");

    /* Register call back and configuration with stack */
    result = wiced_bt_stack_init(app_bt_management_callback,
                                &wiced_bt_cfg_settings);
    /* Check if stack initialization was successful */
    if( WICED_BT_SUCCESS == result)
    {
        printf("Bluetooth Stack Initialization Successful \n");
    }
    else
    {
        printf("Bluetooth Stack Initialization failed!! \n");
        CY_ASSERT(0);
    }

    rtos_result = xTaskCreate(ble_button_task,BUTTON_TASK_STRING,
                                TASK_STACK_SIZE,
                                NULL, TASK_PRIORITY,
                                &ble_button_task_handle);
    if(pdPASS != rtos_result)
    {
        CY_ASSERT(0) ;
    }

    rtos_result = xTaskCreate(get_throughput_task,THROUGHPUT_TASK_STRING,
                                TASK_STACK_SIZE,
                                NULL, TASK_PRIORITY,
                                &get_throughput_task_handle);
    if(pdPASS != rtos_result)
    {
        CY_ASSERT(0) ;
    }

    rtos_result = xTaskCreate(send_gatt_write_task,MILLISEC_TASK_STRING,
                                TASK_STACK_SIZE,
                                NULL,TASK_PRIORITY,
                                &send_gatt_write_task_handle);
    if(pdPASS != rtos_result)
    {
        CY_ASSERT(0) ;
    }

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler() ;

    /* The application should never reach here */
    CY_ASSERT(0) ;
}

/* [] END OF FILE */

