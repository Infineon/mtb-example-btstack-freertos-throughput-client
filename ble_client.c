/*******************************************************************************
 * File Name: ble_client.c
 *
 * Description: This is the source code for the FreeRTOS: BLE Throughput Client
 *              Example for ModusToolbox.
 *
 * Related Document: See README.md
 *
 *
 *******************************************************************************
 * Copyright 2021-2024, Cypress Semiconductor Corporation (an Infineon company) or
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


/*******************************************************************************
*        Header Files
*******************************************************************************/
#include "cyhal.h"
#include <FreeRTOS.h>
#include <task.h>
#include "wiced_memory.h"
#include "cycfg_gap.h"
#include "app_bt_utils.h"
#include "wiced_bt_stack.h"
#include "ble_client.h"
#include "wiced_bt_l2c.h"

/*******************************************************************************
*         Macros
*******************************************************************************/
#define GET_THROUGHPUT_TIMER_PERIOD (9999u)
#define APP_MILLISEC_TIMER_PERIOD (9u)
#define WRITE_DATA_SIZE (244)
#define TASK_NOTIFY_1MS_TIMER (1u)
#define TASK_NOTIFY_NO_GATT_CONGESTION (2u)

/*******************************************************************************
*        Variable Definitions
*******************************************************************************/
/* Variables to hold GATT notification bytes sent and GATT Write bytes received
 * successfully*/
static unsigned long gatt_notif_rx_bytes = 0;
static unsigned long gatt_write_tx_bytes = 0;
/*Variable that stores the data which will be sent as GATT write alternatively*/
uint8_t write_data_seq1[WRITE_DATA_SIZE];
uint8_t write_data_seq2[WRITE_DATA_SIZE];
/* Variable to store packet size decided based on MTU exchanged */
static uint16_t packet_size = 0;
/* PWM object used for Advertising Led*/
static cyhal_pwm_t scan_led_pwm;
/* Variable to store ble advertising state*/
static app_bt_scan_conn_mode_t app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_OFF;
static bool tput_service_found = false;
/* Variable to store connection state information*/
static conn_state_info_t conn_state_info;
/* Enable or Disable notification from server */
static bool enable_cccd = true;
/* Flag to enable or disable GATT write */
static bool gatt_write_tx = false;
/* Flag to used to Scan only for first button press */
static bool scan_flag = true;
/* Variable to switch between different data transfer modes */
static tput_mode_t mode_flag = GATT_NOTIFANDWRITE;
static const uint8_t tput_service_uuid[LEN_UUID_128] = TPUT_SERVICE_UUID;
static uint16_t tput_service_handle = 0;
static wiced_bt_gatt_status_t status;
static wiced_bt_gatt_write_hdr_t tput_write_cmd = {0};
/* variables for app buffer + handling */
uint8_t *tput_buffer_ptr;
uint8_t  data_flag = 0;
uint8_t  value_initialize = 243;
/* For 1 second timer*/
static cyhal_timer_t get_throughput_timer_obj,app_millisec_timer_obj;
const cyhal_timer_cfg_t get_throughput_timer_cfg =
{
    .compare_value = 0,                   /* Timer compare value, not used */
    .period = GET_THROUGHPUT_TIMER_PERIOD,/* Defines the timer period */
    .direction = CYHAL_TIMER_DIR_UP,      /* Timer counts up */
    .is_compare = false,                  /* Don't use compare mode */
    .is_continuous = true,                /* Run timer indefinitely */
    .value = 0                            /* Initial value of counter */
};
/* For 1 millisecond timer*/
const cyhal_timer_cfg_t app_millisec_timer_cfg =
{
    .compare_value = 0,                  /* Timer compare value, not used */
    .period = APP_MILLISEC_TIMER_PERIOD, /* Defines the timer period */
    .direction = CYHAL_TIMER_DIR_UP,     /* Timer counts up */
    .is_compare = false,                 /* Don't use compare mode */
    .is_continuous = true,               /* Run timer indefinitely */
    .value = 0                           /* Initial value of counter */
};

/*******************************************************************************
*        Function Prototypes
*******************************************************************************/
static void tput_scan_led_update            (void);
static void tput_ble_app_init               (void);
static void tput_button_interrupt_handler   (void *handler_arg,
                                            cyhal_gpio_event_t event);
static uint16_t tput_get_write_cmd_pkt_size (uint16_t att_mtu_size);
static wiced_bt_gatt_status_t tput_enable_disable_gatt_notification(bool notify);
static void tput_scan_result_cback  (wiced_bt_ble_scan_results_t *p_scan_result,
                                    uint8_t *p_adv_data);
void tput_app_throughput_timer_callb        (void *callback_arg,
                                            cyhal_timer_event_t event);
void tput_app_millisec_timer_callb          (void *callback_arg,
                                            cyhal_timer_event_t event);

/* GATT Event Callback Functions */
static wiced_bt_gatt_status_t ble_app_connect_callback
                            (wiced_bt_gatt_connection_status_t *p_conn_status);
static wiced_bt_gatt_status_t ble_app_gatt_event_handler(wiced_bt_gatt_evt_t event,
                                     wiced_bt_gatt_event_data_t *p_event_data);

static cyhal_gpio_callback_data_t cyhal_gpio_callback_data =
{
        .callback = tput_button_interrupt_handler,
        .pin = CYBSP_USER_BTN,
};

/******************************************************************************
 * Function Definitions
 ******************************************************************************/

/*******************************************************************************
* Function Name: app_bt_free_buffer()
********************************************************************************
* Summary:
*   This function frees up the memory buffer
*
* Parameters:
*   uint8_t *p_data: Pointer to the buffer to be free
*
* Return:
*   None
*
*******************************************************************************/
void app_bt_free_buffer(uint8_t *p_buf)
{
    vPortFree(p_buf);
}


/*******************************************************************************
* Function Name: app_bt_alloc_buffer()
********************************************************************************
* Summary:
*   This function allocates a memory buffer
*
* Parameters:
*   int len : Length to allocate
*
* Return:
*   None
*
*******************************************************************************/
void* app_bt_alloc_buffer(int len)
{
    return pvPortMalloc(len);
}



/*******************************************************************************
* Function Name: app_bt_management_callback()
********************************************************************************
* Summary:
*   This is a Bluetooth stack event handler function to receive management events
*   from the BLE stack and process as per the application.
*
* Parameters:
*   wiced_bt_management_evt_t      : BLE event code of one byte length
*   wiced_bt_management_evt_data_t : Pointer to BLE management event structures
*
* Return:
*  wiced_result_t: Error code from WICED_RESULT_LIST or BT_RESULT_LIST
*
*******************************************************************************/
wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event,
                                    wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t status = WICED_BT_SUCCESS;
    wiced_bt_device_address_t bda = {0};
    wiced_bt_ble_scan_type_t p_scan_type ;

    switch (event)
    {
    case BTM_ENABLED_EVT:
        /* Bluetooth Controller and Host Stack Enabled */
        if(WICED_BT_SUCCESS == p_event_data->enabled.status)
        {
            /* Bluetooth is enabled */
            wiced_bt_dev_read_local_addr(bda);
            printf("Local Bluetooth Address: ");
            print_bd_address(bda);
            /* Perform application-specific initialization */
            tput_ble_app_init();
        }
        else
        {
            printf("Bluetooth Disabled \n");
        }
        break;

    case BTM_BLE_SCAN_STATE_CHANGED_EVT:
        /* Scan State Changed */
        p_scan_type = p_event_data->ble_scan_state_changed;

        if(BTM_BLE_SCAN_TYPE_NONE == p_scan_type)
        {
            /* Scan Stopped */
            printf("Scanning stopped\n");
            /* Check connection status after scanning stops */
            if (conn_state_info.conn_id == 0)
            {
                app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_OFF;
            }
            else
            {
                app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_ON;
            }
        }
        else
        {
            /* Scan Started */
            printf("Scanning.....\n");
            app_bt_scan_conn_state = APP_BT_SCAN_ON_CONN_OFF;
        }
        /* Update Scan LED to reflect the updated state */
        tput_scan_led_update();
        break;

    case BTM_BLE_PHY_UPDATE_EVT:
        conn_state_info.rx_phy = p_event_data->ble_phy_update_event.rx_phy;
        conn_state_info.tx_phy = p_event_data->ble_phy_update_event.tx_phy;
        printf("Selected RX PHY - %dM\nSelected TX PHY - %dM\nPeer address = ",
                                conn_state_info.rx_phy,conn_state_info.tx_phy);
        break;

    case BTM_BLE_CONNECTION_PARAM_UPDATE:
        /* Connection parameters updated */
        if(WICED_BT_SUCCESS == p_event_data->ble_connection_param_update.status)
        {
            conn_state_info.conn_interval = (double)
                    ((p_event_data->ble_connection_param_update.conn_interval)
                                            * CONN_INTERVAL_MULTIPLIER);

            printf("New connection interval: %f ms\n",
                                            conn_state_info.conn_interval);
        }
        else
        {
            printf("Connection parameters update failed: %d\n",
                            p_event_data->ble_connection_param_update.status);
        }
        break;

    default:
        printf("Unhandled Bluetooth Management Event: 0x%x %s\n",
                                            event, get_bt_event_name(event));
        break;
    }

    return status;
}

/*******************************************************************************
* Function Name: tput_ble_app_init()
********************************************************************************
* Summary:
*   This function handles application level initialization tasks and is called
*   from the BT management callback once the BLE stack enabled event
*   (BTM_ENABLED_EVT) is triggered. This function is executed in the
*   BTM_ENABLED_EVT management callback.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
static void tput_ble_app_init(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    /* Initialize the PWM used for Scanning LED */
    rslt = cyhal_pwm_init(&scan_led_pwm, SCAN_LED_GPIO, NULL);

    /* PWM init failed. Stop program execution */
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Scan LED PWM Initialization has failed! \n");
        CY_ASSERT(0);
    }
    app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_OFF;
    tput_scan_led_update();

    /*Initialize the data packet to be sent as GATT notification to the peer
      device */
    for(uint8_t index = 0; index < WRITE_DATA_SIZE; index++)
    {
        write_data_seq1[index] = index;
    }
    for(uint8_t index = 0; index < WRITE_DATA_SIZE; index++)
    {
        write_data_seq2[index] = value_initialize;
        value_initialize--;
    }

    /* 1 second Timer initialization */
    rslt = cyhal_timer_init(&get_throughput_timer_obj, NC, NULL);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Get throughput timer init failed !\n");
        CY_ASSERT(0);
    }
    cyhal_timer_configure(&get_throughput_timer_obj, &get_throughput_timer_cfg);
    rslt = cyhal_timer_set_frequency(&get_throughput_timer_obj,FREQUENCY);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Get throughput timer set freq failed !\n");
        CY_ASSERT(0);
    }

    cyhal_timer_register_callback(&get_throughput_timer_obj,
                                    tput_app_throughput_timer_callb,
                                    NULL);
    cyhal_timer_enable_event(&get_throughput_timer_obj,
                            CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                            TIMER_INTERRUPT_PRIORITY,
                            true);

    /* 1 millisecond Timer initialization */
    rslt = cyhal_timer_init(&app_millisec_timer_obj, NC, NULL);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Get throughput timer init failed !\n");
        CY_ASSERT(0);
    }
    cyhal_timer_configure(&app_millisec_timer_obj, &app_millisec_timer_cfg);
    rslt = cyhal_timer_set_frequency(&app_millisec_timer_obj,FREQUENCY);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Get throughput timer set freq failed !\n");
        CY_ASSERT(0);
    }

    cyhal_timer_register_callback(&app_millisec_timer_obj,
                                    tput_app_millisec_timer_callb,
                                    NULL);
    cyhal_timer_enable_event(&app_millisec_timer_obj,
                            CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                            TIMER_INTERRUPT_PRIORITY,
                            true);

    /* Initialize GPIO for button interrupt*/
    rslt = cyhal_gpio_init(CYBSP_USER_BTN,
                            CYHAL_GPIO_DIR_INPUT,
                            CYHAL_GPIO_DRIVE_PULLUP,
                            CYBSP_BTN_OFF);
    /* GPIO init failed. Stop program execution */
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Button GPIO init failed! \n");
        CY_ASSERT(0);
    }

    /* Configure GPIO interrupt. */
    cyhal_gpio_register_callback(CYBSP_USER_BTN,
                                &cyhal_gpio_callback_data);
    cyhal_gpio_enable_event(CYBSP_USER_BTN,
                            CYHAL_GPIO_IRQ_FALL,
                            GPIO_INTERRUPT_PRIORITY,
                            true);

    /* Disable pairing for this application */
    wiced_bt_set_pairable_mode(WICED_FALSE, 0);

    /* Register with BT stack to receive GATT callback */
    status = wiced_bt_gatt_register(ble_app_gatt_event_handler);
    printf("GATT event Handler registration status: %s \n",
                                get_bt_gatt_status_name(status));

    /* Initialize GATT Database */
    status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    printf("GATT database initialization status: %s \n",
                                get_bt_gatt_status_name(status));
    printf("Press User Button on your kit to start scanning.....\n");

}

/*******************************************************************************
* Function Name: ble_app_gatt_event_handler()
********************************************************************************
* Summary:
*   This function handles GATT events from the BT stack.
*
* Parameters:
*   wiced_bt_gatt_evt_t event           :BLE GATT event code of one byte length
*   wiced_bt_gatt_event_data_t *p_event_data:Pointer to BLE GATT event structures
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*  in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_event_handler(wiced_bt_gatt_evt_t event,
                                    wiced_bt_gatt_event_data_t *p_event_data)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    /* Call the appropriate callback function based on the GATT event type, and
     * pass the relevant event parameters to the callback function */
    switch (event)
    {
    case GATT_CONNECTION_STATUS_EVT:
        status = ble_app_connect_callback(&p_event_data->connection_status);
        break;

    case GATT_DISCOVERY_RESULT_EVT:
        /* Check if it is throughput service uuid */
        if (!memcmp(&p_event_data->discovery_result.discovery_data.group_value.service_type.uu.uuid128,
                    &tput_service_uuid, TPUT_SERVICE_UUID_LEN))
        {
            /* Update the handle to TPUT service uuid, Throughput service
                GATT handle : 0x0009 */
            tput_service_handle =
            p_event_data->discovery_result.discovery_data.group_value.s_handle;
            tput_service_found = true;
        }
        break;

    case GATT_DISCOVERY_CPLT_EVT:
        if (tput_service_found)
        {
            printf("Custom throughput service found\n");
        }
        else
        {
            printf("Custom throughput service not found\n");
        }
        break;

    case GATT_OPERATION_CPLT_EVT:
        switch (p_event_data->operation_complete.op)
        {
        case GATTC_OPTYPE_WRITE_WITH_RSP:
        /*Check if GATT operation of enable/disable notification is success.*/
            if ((p_event_data->operation_complete.response_data.handle ==
                        (tput_service_handle + GATT_CCCD_HANDLE)) &&
            (WICED_BT_GATT_SUCCESS == p_event_data->operation_complete.status))
            {
                printf("Notifications %s\n",(enable_cccd)?"enabled":"disabled");
                /* Start msec timer only for GATT writes */
                if (gatt_write_tx)
                {
                    /* Clear GATT Tx packets */
                    gatt_notif_rx_bytes = 0;
                    if(CY_RSLT_SUCCESS != cyhal_timer_start(&app_millisec_timer_obj))
                    {
                       printf("Get millisec timer start failed !\n");
                       CY_ASSERT(0);
                    }
                }
            }
            else if (p_event_data->operation_complete.response_data.handle ==
                        (tput_service_handle + GATT_CCCD_HANDLE) &&
            (WICED_BT_GATT_SUCCESS != p_event_data->operation_complete.status))
            {
                printf("CCCD update failed. Error: %x\n",
                                    p_event_data->operation_complete.status);
            }
            break;

        case GATTC_OPTYPE_WRITE_NO_RSP:
            if ((p_event_data->operation_complete.response_data.handle ==
                    (tput_service_handle + GATT_WRITE_HANDLE)) &&
            (WICED_BT_GATT_SUCCESS == p_event_data->operation_complete.status))
            {
                gatt_write_tx_bytes += packet_size;
            }
            break;

        case GATTC_OPTYPE_NOTIFICATION:
            /* Receive GATT Notifications from server */
            gatt_notif_rx_bytes += p_event_data->operation_complete.response_data.att_value.len;
            break;

        case GATTC_OPTYPE_CONFIG_MTU:
            conn_state_info.mtu = p_event_data->operation_complete.response_data.mtu;
            printf("Negotiated MTU Size: %d\n", conn_state_info.mtu);
            packet_size = tput_get_write_cmd_pkt_size(conn_state_info.mtu);

            /* Send GATT service discovery request */
            wiced_bt_gatt_discovery_param_t gatt_discovery_setup = {0};
            gatt_discovery_setup.s_handle = 1;
            gatt_discovery_setup.e_handle = 0xFFFF;
            gatt_discovery_setup.uuid.len = TPUT_SERVICE_UUID_LEN;
            memcpy(gatt_discovery_setup.uuid.uu.uuid128,
                    tput_service_uuid,
                    TPUT_SERVICE_UUID_LEN);
            status = wiced_bt_gatt_client_send_discover(conn_state_info.conn_id,
                                                 GATT_DISCOVER_SERVICES_BY_UUID,
                                                        &gatt_discovery_setup);
            if (WICED_BT_GATT_SUCCESS != status)
            {
                printf("GATT Discovery request failed. Error code: %d,Conn id: %d\n",
                                             status, conn_state_info.conn_id);
            }
            break;
        }
        break;

    case GATT_CONGESTION_EVT:
        if(!p_event_data->congestion.congested)
        {
            xTaskNotifyGiveIndexed(send_gatt_write_task_handle,
                                    TASK_NOTIFY_NO_GATT_CONGESTION);
        }
        break;
    case GATT_GET_RESPONSE_BUFFER_EVT:
        if (p_event_data->buffer_request.len_requested != 0)
        {
            p_event_data->buffer_request.buffer.p_app_rsp_buffer =
                app_bt_alloc_buffer(p_event_data->buffer_request.len_requested);
            p_event_data->buffer_request.buffer.p_app_ctxt =
                                                    (void *)app_bt_free_buffer;
            status = WICED_BT_GATT_SUCCESS;
        }
        break;
    case GATT_APP_BUFFER_TRANSMITTED_EVT:
    {
        break;
    }
    default:
        status = WICED_BT_GATT_SUCCESS;
        break;
    }
    return status;
}

/*******************************************************************************
 * Function Name: tput_button_interrupt_handler
 *******************************************************************************
 * Summary:
 *  GPIO interrupt service routine. This function detects button presses and
 *  changes the data transfer mode.
 *
 * Parameters:
 *  void *callback_arg : pointer to the variable passed to the ISR
 *  cyhal_gpio_event_t event : GPIO event type
 *
 * Return:
 *  None
 *
 ******************************************************************************/
static void tput_button_interrupt_handler(void *handler_arg,
                                        cyhal_gpio_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(ble_button_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*******************************************************************************
 * Function Name: ble_button_task
 *******************************************************************************
 * Summary:
 *  This function handles the button event.
 *
 * Parameters:
 *  void *pvParam : Unused
 *
 * Return:
 *  None
 *
 ******************************************************************************/
void ble_button_task(void *pvParam)
{
    wiced_result_t status = WICED_BT_SUCCESS;
    wiced_bt_gatt_status_t gatt_status;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!conn_state_info.conn_id)
        {
            if (scan_flag)
            {
                /* Start scan */
                status = wiced_bt_ble_scan(BTM_BLE_SCAN_TYPE_HIGH_DUTY, true,
                                           tput_scan_result_cback);
                if ((WICED_BT_PENDING != status) && (WICED_BT_BUSY != status))
                {
                    printf("Error: Starting scan failed. Error code: %d\n",status);
                    /* Switch off the scan LED */
                    app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_OFF;
                    tput_scan_led_update();
                }
            }
        }
        else
        {
            /* After connection pressing the user button will change the
            * throughput modes as follows :
            * GATT_Notif_StoC -> GATT_Write_CtoS -> GATT_NotifandWrite -> Roll
            * back to GATT_Notif_StoC
            */

            /* Stop ongoing GATT writes when enabling/disabling server
            * notification ,to prevent command failure due to GATT congestion
            * that may occur .The timer will be enabled on GATT event callback
            * based on the status of the GATT operation.
            */
            if (CY_RSLT_SUCCESS != cyhal_timer_stop(&app_millisec_timer_obj))
            {
                 printf("Get millisec timer stop failed !\n");
                 CY_ASSERT(0);
            }
            gatt_write_tx_bytes = 0;

            /* Change data transfer modes upon interrupt. Based on the current
            * mode,set flags to enable/disable notifications and set/clear GATT
            * write flag
            */
            mode_flag = (mode_flag == GATT_NOTIFANDWRITE) ?
                        GATT_NOTIF_STOC :
                        (tput_mode_t)(mode_flag + 1u);
            switch (mode_flag)
            {
            case GATT_NOTIF_STOC:
                enable_cccd = true;
                gatt_write_tx = false;
                break;

            case GATT_WRITE_CTOS:
                enable_cccd = false;
                gatt_write_tx = true;
                break;

            case GATT_NOTIFANDWRITE:
                enable_cccd = true;
                gatt_write_tx = true;
                break;

            default:
                printf("Invalid Data Transfer Mode\n");
                break;
            }
            /*Delay added to avoid the failure of notification enable packet*/
            vTaskDelay(2000);
            gatt_status = tput_enable_disable_gatt_notification(enable_cccd);
            if (WICED_BT_GATT_SUCCESS != gatt_status)
            {
                printf("Enable/Disable notification failed: %d\n\r",gatt_status);
            }
        }
    }
}

/*******************************************************************************
* Function Name: tput_scan_result_cback()
********************************************************************************
* Summary:
*   This function is registered as a callback to handle the scan results.
*   When the desired device is found, it will try to establish connection with
*   that device.
*
* Parameters:
*   wiced_bt_ble_scan_results_t *p_scan_result: Details of the new device found.
*   uint8_t                     *p_adv_data      : Advertisement data.
*
* Return:
*   None
*
*******************************************************************************/
static void tput_scan_result_cback(wiced_bt_ble_scan_results_t *p_scan_result,
                                    uint8_t *p_adv_data)
{
    wiced_result_t status = WICED_BT_SUCCESS;
    uint8_t length = 0u;
    uint8_t *p_data = NULL;
    uint8_t server_device_name[5] = {'T', 'P', 'U', 'T', '\0'};

    if (p_scan_result)
    {
        p_data = wiced_bt_ble_check_advertising_data(p_adv_data,
                                            BTM_BLE_ADVERT_TYPE_NAME_COMPLETE,
                                                    &length);

        if (p_data != NULL)
        {
            /* Check if the peer device's name is "TPUT" */
            if ((length = strlen((const char *)server_device_name)) &&
                (memcmp(p_data, (uint8_t *)server_device_name, length) == 0))
            {
                printf("Scan completed\n Found peer device with BDA:\n");
                print_bd_address(p_scan_result->remote_bd_addr);
                scan_flag = false;

                /* Device found. Stop scan. */
                if ((status = wiced_bt_ble_scan(BTM_BLE_SCAN_TYPE_NONE, true,
                                                tput_scan_result_cback)) != 0)
                {
                    printf("Scan off status %d\n", status);
                }

                /* Initiate the connection */
                if (wiced_bt_gatt_le_connect(p_scan_result->remote_bd_addr,
                                             p_scan_result->ble_addr_type,
                                             BLE_CONN_MODE_HIGH_DUTY,
                                             WICED_TRUE) != WICED_TRUE)
                {
                    printf("wiced_bt_gatt_connect failed\n");
                }
                else
                {
                    printf("gatt connect request sent\n");
                }
            }
        }
    }
}

/*******************************************************************************
* Function Name: ble_app_connect_callback()
********************************************************************************
* Summary:
*   This callback function handles connection status changes.
*
* Parameters:
*   wiced_bt_gatt_connection_status_t *p_conn_status  : Pointer to data that has
*                                                       connection details
*
* Return:
*  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
*                          in wiced_bt_gatt.h
*
*******************************************************************************/
static wiced_bt_gatt_status_t ble_app_connect_callback(
                            wiced_bt_gatt_connection_status_t *p_conn_status)
{
    wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;

    if (NULL != p_conn_status)
    {
        if (p_conn_status->connected)
        {
            /* Device has connected */
            printf("Connected : BDA ");
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d'\n", p_conn_status->conn_id);

            /* Store the connection ID and remote BDA*/
            conn_state_info.conn_id = p_conn_status->conn_id;
            printf("connection_id = %d\n", conn_state_info.conn_id);
            memcpy(conn_state_info.remote_addr,
                        p_conn_status->bd_addr,
                        BD_ADDR_LEN);

            /* Update the scan/conn state */
            app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_ON;

            wiced_bt_l2cap_enable_update_ble_conn_params(conn_state_info.remote_addr,
                                                        true);

            /* Send MTU exchange request */
            status = wiced_bt_gatt_client_configure_mtu(conn_state_info.conn_id,
                                                        CY_BT_MTU_SIZE);
            if (status != WICED_BT_GATT_SUCCESS)
            {
                printf("GATT MTU configure failed %d\n", status);
            }

            if (CY_RSLT_SUCCESS != cyhal_timer_start(&get_throughput_timer_obj))
            {
                printf("Get throughput timer start failed !\n");
                CY_ASSERT(0);
            }
        }
        else
        {
            /* Device has disconnected */
            printf("Disconnected : BDA ");
            print_bd_address(p_conn_status->bd_addr);
            printf("Connection ID '%d', Reason '%s'\n",
                        p_conn_status->conn_id,
                        get_bt_gatt_disconn_reason_name(p_conn_status->reason));

            /* Fill the structure containing connection info with zero */
            memset(&conn_state_info, 0, sizeof(conn_state_info));
            /* Reset the flags */
            tput_service_found = false;
            mode_flag = GATT_NOTIFANDWRITE;
            enable_cccd = true;
            gatt_write_tx = false;
            scan_flag = true;
            /* Clear tx and rx packet count */
            gatt_notif_rx_bytes = 0;
            gatt_write_tx_bytes = 0;
            /* Stop the timers */
            if (CY_RSLT_SUCCESS != cyhal_timer_stop(&get_throughput_timer_obj))
            {
                printf("Get throughput timer stop failed !\n");
                CY_ASSERT(0);
            }

            if (CY_RSLT_SUCCESS != cyhal_timer_stop(&app_millisec_timer_obj))
            {
                printf("Get millisec timer stop failed !\n");
                CY_ASSERT(0);
            }
            /* Update the scan/conn state */
            app_bt_scan_conn_state = APP_BT_SCAN_OFF_CONN_OFF;
            printf("Press user button on your kit to start scanning.....\n");
        }

        /* Update Scan LED to reflect the updated state */
        tput_scan_led_update();
    }

    return status;
}

/*******************************************************************************
* Function Name: tput_app_throughput_timer_callb()
********************************************************************************
*
* Summary:
*   One second timer callback.
*
* Parameters:
*   void *callback_arg  : The argument parameter is not used in this callback.
*   cyhal_timer_event_t event : The argument parameter is not used in this
*                               callback.
*
* Return:
*   None
*
*******************************************************************************/
void tput_app_throughput_timer_callb(void *callback_arg,cyhal_timer_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(get_throughput_task_handle,&xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*******************************************************************************
* Function Name: get_throughput_task()
********************************************************************************
* Summary:
*   Send Throughput Values every second .
*
* Parameters:
*   void *pvParam : The argument parameter is not used.
*
* Return:
*   None
*
*******************************************************************************/
void get_throughput_task(void *pvParam)
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (conn_state_info.conn_id && gatt_notif_rx_bytes)
        {
            gatt_notif_rx_bytes = (gatt_notif_rx_bytes * 8) / 1000;
            printf("GATT NOTIFICATION : Client Throughput (RX) = %lu kbps\n",
                                                         gatt_notif_rx_bytes);
            gatt_notif_rx_bytes = 0; //Reset the byte counter
        }

        if ((conn_state_info.conn_id) && gatt_write_tx_bytes)
        {
            gatt_write_tx_bytes = (gatt_write_tx_bytes * 8) / 1000;
            printf("GATT WRITE        : Client Throughput (TX) = %lu kbps\n",
                                                        gatt_write_tx_bytes);
            gatt_write_tx_bytes = 0; //Reset the byte counter
        }
    }
}

/*******************************************************************************
* Function Name: tput_app_millisec_timer_cb()
********************************************************************************
*
* Summary:
*   One millisecond timer callback.
*
* Parameters:
*   void *callback_arg  : The argument parameter is not used in this callback.
*   cyhal_timer_event_t event : The argument parameter is not used in this
*                               callback.
*
* Return:
*   None
*
*******************************************************************************/
void tput_app_millisec_timer_callb(void *callback_arg,cyhal_timer_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(send_gatt_write_task_handle,
                                TASK_NOTIFY_1MS_TIMER,
                                &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*******************************************************************************
* Function Name: send_gatt_write_task()
********************************************************************************
*
* Summary:
*   Send GATT Write if enabled by the GATT Client.
*
* Parameters:
*   void *pvParam : The argument parameter is not used.
*
* Return:
*   None
*
*******************************************************************************/
void send_gatt_write_task(void *pvParam)
{
    while(true)
    {
    ulTaskNotifyTakeIndexed(TASK_NOTIFY_1MS_TIMER,pdTRUE, portMAX_DELAY);

    /* Send GATT write(with no response) commands to the server only
     * when there is no GATT congestion and no GATT notifications are being
     * received. In data transfer mode 3(Both TX and RX), the GATT write
     * commands will be sent irrespective of GATT notifications being received
     * or not and when it is connected .
     */
    if ((conn_state_info.conn_id) && (gatt_write_tx == true))
    {
            tput_write_cmd.auth_req = GATT_AUTH_REQ_NONE;
            tput_write_cmd.handle = (tput_service_handle) + GATT_WRITE_HANDLE;
            tput_write_cmd.len = packet_size;
            tput_write_cmd.offset = 0;
            /*Packets are sending alternatively*/
            if(data_flag == 0)
            {
            status = wiced_bt_gatt_client_send_write(conn_state_info.conn_id,
                                                                GATT_CMD_WRITE,
                                                                &tput_write_cmd,
                                                                write_data_seq1,
                                                                (void *)app_bt_free_buffer);
            }
            else
            {
            status = wiced_bt_gatt_client_send_write(conn_state_info.conn_id,
                                                                GATT_CMD_WRITE,
                                                                &tput_write_cmd,
                                                                write_data_seq2,
                                                                (void *)app_bt_free_buffer);
            }

            if(WICED_BT_GATT_CONGESTED == status)
            {
            app_bt_free_buffer((wiced_bt_buffer_t *)tput_buffer_ptr);
            ulTaskNotifyTakeIndexed(TASK_NOTIFY_NO_GATT_CONGESTION,
                                                            pdTRUE,
                                                            portMAX_DELAY);
            }
            else if (WICED_BT_GATT_SUCCESS == status)
            {
             data_flag = data_flag == 0 ? 1 : 0 ;
            }
    }
    }
}

/*******************************************************************************
* Function Name: tput_scan_led_update()
********************************************************************************
*
* Summary:
*   This function updates the scan LED state based on BLE scanning/
*   connection state
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
static void tput_scan_led_update(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    /* Stop the scan led pwm */
    rslt = cyhal_pwm_stop(&scan_led_pwm);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Failed to stop PWM !!\n");
        CY_ASSERT(0);
    }

    /* Update LED state based on BLE scan/connection state.
     * LED OFF for no scan/connection, LED blinking for scan state,
     * LED ON for connected state  */
    switch (app_bt_scan_conn_state)
    {
    case APP_BT_SCAN_OFF_CONN_OFF:
        rslt = cyhal_pwm_set_duty_cycle(&scan_led_pwm,
                                        LED_OFF_DUTY_CYCLE,
                                        SCAN_LED_PWM_FREQUENCY);
        break;

    case APP_BT_SCAN_ON_CONN_OFF:
        rslt = cyhal_pwm_set_duty_cycle(&scan_led_pwm,
                                        LED_BLINKING_DUTY_CYCLE,
                                        SCAN_LED_PWM_FREQUENCY);
        break;

    case APP_BT_SCAN_OFF_CONN_ON:
        rslt = cyhal_pwm_set_duty_cycle(&scan_led_pwm,
                                        LED_ON_DUTY_CYCLE,
                                        SCAN_LED_PWM_FREQUENCY);
        break;

    default:
        /* LED OFF for unexpected states */
        rslt = cyhal_pwm_set_duty_cycle(&scan_led_pwm,
                                        LED_OFF_DUTY_CYCLE,
                                        SCAN_LED_PWM_FREQUENCY);
        break;
    }
    /* Check if update to PWM parameters is successful*/
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Failed to set duty cycle parameters!!\n");
    }

    /* Start the scan led pwm */
    rslt = cyhal_pwm_start(&scan_led_pwm);

    /* Check if PWM started successfully */
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("Failed to start PWM !!\n");
        CY_ASSERT(0);
    }
}

/*******************************************************************************
* Function Name: tput_enable_disable_gatt_notification()
********************************************************************************
* Summary:
*   Enable or disable  GATT notification from the server.
*
* Parameters:
*   bool notify : Boolean variable to enable/disable notification.
*
* Return:
*   wiced_bt_gatt_status_t  : Status code from wiced_bt_gatt_status_e.
*
*******************************************************************************/
static wiced_bt_gatt_status_t tput_enable_disable_gatt_notification(bool notify)
{
    wiced_bt_gatt_write_hdr_t tput_write_notif = {0};
    wiced_bt_gatt_status_t status = WICED_BT_GATT_SUCCESS;
    uint8_t local_notif_enable[CCCD_LENGTH] = {0};
    uint8_t *notif_val = NULL;
    /*Allocate memory for data to be written on server DB and pass it to stack*/
    notif_val = app_bt_alloc_buffer(sizeof(uint16_t)); //CCCD is two bytes
    if (notif_val)
    {
        local_notif_enable[0] = notify;
        memcpy(notif_val, local_notif_enable, sizeof(uint16_t));
        tput_write_notif.auth_req = GATT_AUTH_REQ_NONE;
        tput_write_notif.handle = tput_service_handle + GATT_CCCD_HANDLE;
        tput_write_notif.len = CCCD_LENGTH;
        tput_write_notif.offset = 0;
        status = wiced_bt_gatt_client_send_write(conn_state_info.conn_id,
                                                GATT_REQ_WRITE,
                                                &tput_write_notif,
                                        notif_val,(void *)app_bt_free_buffer);
    }
    else
    {
        printf("malloc failed! write request not sent\n");
        status = WICED_BT_GATT_ERROR;
    }
    return status;
}

/*******************************************************************************
* Function Name: tput_get_write_cmd_pkt_size()
********************************************************************************
* Summary: This function decides size of notification packet based on the
*   attribute MTU exchanged. This is done to utilize the LL payload space
*   effectively.
*
* Parameters:
*   uint16_t att_mtu_size: MTU value exchaged after connection.
*
* Return:
*   uint16_t: Size of notification packet derived based on MTU.
*
*******************************************************************************/
static uint16_t tput_get_write_cmd_pkt_size(uint16_t att_mtu_size)
{
    if (att_mtu_size < DATA_PACKET_SIZE_1 + ATT_HEADER)
    {
        /* Packet Length = ATT_MTU_SIZE - ATT_HANDLE(2 bytes)-ATT_OPCODE(1 byte)
        * Reason: With DLE enabled, LL payload is 251 bytes. So if an MTU less
        * than 247 is exchanged, the data can be accommodated in a single LL
        * packet */
        packet_size = att_mtu_size - ATT_HEADER;
    }
    else if ((att_mtu_size >= DATA_PACKET_SIZE_1 + ATT_HEADER) &&
                    (att_mtu_size < DATA_PACKET_SIZE_2 + ATT_HEADER))
    {
        /* If MTU is between 247 and 498, if a packet size other than 244 bytes
         * is used, the data will be split and the LL payload space is not
         * utilized effectively. Refer README for details */
        packet_size = DATA_PACKET_SIZE_1;
    }
    else
    {
        /*For MTU value greater than 498, if a packet size other than 495(or244)
         * is used, the LL payload space is not utilized effectively.
         * 495 bytes will go as two LL packets: 244 bytes + 251 bytes */
        packet_size = DATA_PACKET_SIZE_2;
    }
    return packet_size;
}
/* [] END OF FILE */
