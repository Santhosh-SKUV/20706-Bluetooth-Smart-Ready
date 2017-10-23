/*
This project demonstrates BLE Beacon with a fixed advertisement data
 */
 
#include "sparcommon.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_cfg.h"
#include "wiced_hal_gpio.h"
#include "wiced_bt_uuid.h"
#include "wiced_gki.h"
#include "wiced_bt_app_common.h"
#include "wiced_bt_app_hal_common.h"
#include "wiced_result.h"
#include "wiced_hal_platform.h"
#include "wiced_memory.h"
#include "wiced_transport.h"

#ifdef  WICED_BT_TRACE_ENABLE
#include "wiced_bt_trace.h"
#endif


/*  App Timer Timeout in seconds  */
#define CUSTOM_SENSOR_APP_TIMEOUT_IN_SECONDS                 20

/* App Fine Timer Timeout in milli seconds  */
#define CUSTOM_SENSOR_APP_FINE_TIMEOUT_IN_MS                 100

/* UUID value of the Custom Sensor Service */
#define UUID_CUSTOM_SERVICE                    0x23, 0x20, 0x56, 0x7c, 0x05, 0xcf, 0x6e, 0xb4, 0xc3, 0x41, 0x77, 0x28, 0x51, 0x82, 0x7e, 0x1b



extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;
extern const wiced_bt_cfg_buf_pool_t wiced_bt_cfg_buf_pools[];


uint8_t hello_sensor_device_name[]          = "BLE BEACON";												//GAP Service characteristic Device Name
uint8_t hello_sensor_appearance_name[2]     = { BIT16_TO_8(APPEARANCE_GENERIC_TAG) };

uint32_t  timer_count;              // timer count
uint32_t  fine_timer_count;         // fine timer count


/* transport configuration */
const wiced_transport_cfg_t  transport_cfg =
{
    WICED_TRANSPORT_UART,
    { WICED_TRANSPORT_UART_HCI_MODE, 115200 },
    { 0, 0},
    NULL,
    NULL,
    NULL
};

/*****************************    Function Prototypes   *******************/
static wiced_result_t           Beacon_management_cback( wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data );
static void                     Beacon_set_advertisement_data( void );
static void                     Connectable_set_advertisement_data( void );
static void                     Button_interrupt_handler( void* user_data, uint8_t pin );
static void                     Beacon_application_init( void );
static void                     custom_sensor_timeout( uint32_t count );
static void                     custom_sensor_fine_timeout( uint32_t finecount );

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/

/*
 *  Entry point to the application. Set device configuration and start BT
 *  stack initialization.  The actual application initialization will happen
 *  when stack reports that BT device is ready.
 */
APPLICATION_START( )
{
    wiced_transport_init( &transport_cfg );

#ifdef WICED_BT_TRACE_ENABLE
    //Set to PUART to see traces on peripheral uart(puart)
    wiced_set_debug_uart( WICED_ROUTE_DEBUG_TO_PUART );
    wiced_hal_puart_select_uart_pads( WICED_PUART_RXD, WICED_PUART_TXD, 0, 0);
#endif

    WICED_BT_TRACE( "Beacon Start \n" );

    // Register call back and configuration with stack
    wiced_bt_stack_init( Beacon_management_cback ,
                    &wiced_bt_cfg_settings, wiced_bt_cfg_buf_pools );
}

/*
 * This function is executed in the BTM_ENABLED_EVT management callback.
 */
void Beacon_application_init( void )
{
    wiced_result_t         result;

    /* Initialize wiced app */
    wiced_bt_app_init();

    /* Starting the app timers , seconds timer and the ms timer  */
    wiced_bt_app_start_timer( CUSTOM_SENSOR_APP_TIMEOUT_IN_SECONDS, CUSTOM_SENSOR_APP_FINE_TIMEOUT_IN_MS,
        custom_sensor_timeout, custom_sensor_fine_timeout );

    /* Set the advertising params and make the device discoverable */
    Beacon_set_advertisement_data();

    result =  wiced_bt_start_advertisements( BTM_BLE_ADVERT_NONCONN_HIGH, 0, NULL );

    WICED_BT_TRACE( "wiced_bt_start_advertisements %d\n", result );

}

/*
 * The function invoked on timeout of app seconds timer.
 */
void custom_sensor_timeout( uint32_t count )
{
    timer_count++;

    if (timer_count %2)
    {
        Beacon_set_advertisement_data();
        wiced_bt_start_advertisements( BTM_BLE_ADVERT_NONCONN_HIGH, 0, NULL );
        WICED_BT_TRACE( "Non Connectable Advertisement \n" );

    }
    else
    {
        Connectable_set_advertisement_data();
        wiced_bt_start_advertisements( BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL );
        WICED_BT_TRACE( "Connectable Advertisement \n" );
    }


}

/*
 * The function invoked on timeout of app milliseconds fine timer
 */
void custom_sensor_fine_timeout( uint32_t finecount )
{
    fine_timer_count++;
}

/*
 * Setup advertisement data with 16 byte UUID and device name
 */
void Beacon_set_advertisement_data(void)
{
    wiced_bt_ble_advert_elem_t adv_elem[4];
    uint8_t num_elem = 0;
    uint8_t flag = BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED;
    uint8_t mfg_data[2] = {0x00, 0x01};

    adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_FLAG;
    adv_elem[num_elem].len          = sizeof(uint8_t);
    adv_elem[num_elem].p_data       = &flag;
    num_elem++;

    adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
    adv_elem[num_elem].len          = strlen((char*)wiced_bt_cfg_settings.device_name);
    adv_elem[num_elem].p_data       = (uint8_t*)wiced_bt_cfg_settings.device_name;
    num_elem++;

    adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_MANUFACTURER;
    adv_elem[num_elem].len          = sizeof(mfg_data);
    adv_elem[num_elem].p_data       = (uint8_t*) mfg_data;
    num_elem++;

    wiced_bt_ble_set_raw_advertisement_data(num_elem, adv_elem);
}



/*
 * Setup advertisement data with 16 byte UUID and device name
 */
void Connectable_set_advertisement_data(void)
{
    wiced_bt_ble_advert_elem_t adv_elem[4];
    uint8_t num_elem = 0;
    uint8_t flag = BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED;

    uint8_t  hci_control_le_local_name[]      =   "Custom" ; //Alternate way to declare {'h', 'e', 'l', 'l', 'o', 0x00, 0x00};
    uint8_t custom_service_uuid[LEN_UUID_128] = { UUID_CUSTOM_SERVICE };

    adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_FLAG;
    adv_elem[num_elem].len          = sizeof(uint8_t);
    adv_elem[num_elem].p_data       = &flag;
    num_elem++;

    adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_128SRV_COMPLETE;
    adv_elem[num_elem].len          = sizeof(custom_service_uuid);
    adv_elem[num_elem].p_data       = ( uint8_t* )custom_service_uuid;
    num_elem++;

    adv_elem[num_elem].advert_type  = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
    adv_elem[num_elem].len          = strlen(hci_control_le_local_name);
    adv_elem[num_elem].p_data       = ( uint8_t* )hci_control_le_local_name;
    num_elem++;

    wiced_bt_ble_set_raw_advertisement_data(num_elem, adv_elem);
}


/*
 * Beacon BLE link management callback
 */
wiced_result_t Beacon_management_cback( wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data )
{
    wiced_result_t                    result = WICED_BT_SUCCESS;
    wiced_bt_dev_encryption_status_t *p_status;
    WICED_BT_TRACE("management_cback: %x\n", event );
    switch( event )
    {
    /* Bluetooth  stack enabled */
    case BTM_ENABLED_EVT:
        Beacon_application_init();
        break;

    default:
        break;
    }

    return result;
}
