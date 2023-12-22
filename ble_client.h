/*******************************************************************************
 * File Name: ble_client.h
 *
 * Description: This file consists of the utility functions that will help
*              debugging and developing the applications easier with much
*              more meaningful information.
 *
 * Related Document: See README.md
 *
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
 ******************************************************************************/

#ifndef __BLE_CLIENT_H__
#define __BLE_CLIENT_H__

/******************************************************************************
 *                                INCLUDES
 ******************************************************************************/
#include <FreeRTOS.h>
#include <task.h>
#include "wiced_bt_stack.h"
#include "wiced_bt_ble.h"
#include "cybsp.h"

/******************************************************************************
 *                                Macros
 ******************************************************************************/
/* LED pin assignment for scan event */
#define SCAN_LED_GPIO                   CYBSP_USER_LED1
/* PWM frequency of LED's in Hz when blinking */
#define SCAN_LED_PWM_FREQUENCY          (1)
/* CTS Service UUID length */
#define TPUT_SERVICE_UUID_LEN           (16)
/* Current Time characteristic descriptor length-used to subscribe for notification */
#define CCCD_LENGTH                     (2)
/* Value used to derive handle of CCCD of current time characteristic */
#define GATT_CCCD_HANDLE                (3)
/* Custom Throughput Service UUID - used for service discovery */
#define TPUT_SERVICE_UUID               {0xCCu, 0x7Bu, 0xCBu, 0x32u, 0x07u,\
                                        0x08u, 0x17u, 0xAFu,0xD3u, 0x43u,\
                                        0x1Eu, 0x5Du, 0x20u, 0x0Du, 0xECu, 0x1Au}

#define GATT_WRITE_BYTES_MAX_LEN        (495)
#define CONN_INTERVAL_MULTIPLIER        (1.25f)
#define GPIO_INTERRUPT_PRIORITY         (7)
#define TIMER_INTERRUPT_PRIORITY        (3)
#define FREQUENCY                   (10000)


/* Data packet sizes when 247 <= ATT MTU <= 498 */
#define DATA_PACKET_SIZE_1               (244u)
#define DATA_PACKET_SIZE_2               (495u)
#define ATT_HEADER                       (3u)

/* Handle to write field in GATT DB */
#define GATT_WRITE_HANDLE               (5)

/*******************************************************************************
*        Structures and Enumerations
*******************************************************************************/
typedef struct
{
    wiced_bt_device_address_t  remote_addr; // remote peer device address
    uint16_t                   conn_id; // connection ID referenced by the stack
    uint16_t                   mtu;     // MTU exchanged after connection
    double                     conn_interval; // connection interval negotiated
    wiced_bt_ble_host_phy_preferences_t   rx_phy; // RX PHY selected
    wiced_bt_ble_host_phy_preferences_t   tx_phy; // TX PHY selected

} conn_state_info_t;

typedef enum
{
    GATT_NOTIF_STOC,         /* Server to Client Notifications */
    GATT_WRITE_CTOS,         /* Client to Server GATT writes   */
    GATT_NOTIFANDWRITE       /* Both Notifications and GATT writes */
}tput_mode_t;

/* PWM Duty Cycle of LED's for different states */
typedef enum
{
    LED_ON_DUTY_CYCLE = 0,
    LED_BLINKING_DUTY_CYCLE= 50,
    LED_OFF_DUTY_CYCLE = 100
} led_duty_cycles;

/* This enumeration combines the scan and connection states from two different
 * callbacks to maintain the status in a single state variable */
typedef enum
{
    APP_BT_SCAN_OFF_CONN_OFF,
    APP_BT_SCAN_ON_CONN_OFF,
    APP_BT_SCAN_OFF_CONN_ON
} app_bt_scan_conn_mode_t;

extern TaskHandle_t ble_button_task_handle;
extern TaskHandle_t get_throughput_task_handle;
extern TaskHandle_t send_gatt_write_task_handle;
/****************************************************************************
 *                              FUNCTION DECLARATIONS
 ***************************************************************************/
/* Callback function for Bluetooth stack management events */
wiced_bt_dev_status_t  app_bt_management_callback(wiced_bt_management_evt_t event,
                                    wiced_bt_management_evt_data_t *p_event_data);
void ble_button_task(void *pvParam);
void get_throughput_task(void *pvParam);
void send_gatt_write_task(void *pvParam);

#endif      /*__BLE_CLIENT_H__ */
/* [] END OF FILE */
