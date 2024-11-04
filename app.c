// BLUETOOTH RELAY PROJECT:
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
// custom includes
#include "sl_emlib_gpio_init_RELAY_PIN_config.h"
#include "sl_emlib_gpio_init_BUTTON_config.h"
#include "stdio.h"
#include "printf.h"
#include "em_rtcc.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "gpiointerrupt.h"
// named constants for button and timer
#define BUTTON              (1 << 1)
#define MAX                 100
#define TIMER_ID            (1 << 1)
#define TIME                32768
// custom variable names
static uint8_t adv_set_handle = 0xff;
static customAdv_t advData;
static customScanResp_t scanData;
static uint8_t connection_handle, bonding, counter, *addr, status;
static uint32_t passkey;
static sl_status_t sc;
static char c;
static uint8_t count = 0;
static uint8_t bonded;
static uint32_t totalTime, click;
static bool buttonHeld;
// functions for get/set of passkey
static uint32_t setRandomPasskey(void);
static uint32_t getPasskey(void);
// function for blue tooth scanning
static void setAdvScanData(void);
// functions for button timing and GPIO & RTCC setup
static void button(const uint8_t pin);
static void GPIO_setup(void)
{
  GPIO_ExtIntConfig(SL_EMLIB_GPIO_INIT_BUTTON_PORT, SL_EMLIB_GPIO_INIT_BUTTON_PIN,
                    SL_EMLIB_GPIO_INIT_BUTTON_PIN, true, true, true);
  GPIOINT_Init();
  GPIOINT_CallbackRegister(SL_EMLIB_GPIO_INIT_BUTTON_PIN, button);
}
static void RTCC_setup(void)
{
  CMU_ClockEnable(cmuClock_RTCC, true);
  RTCC_Init_TypeDef initRTCC = RTCC_INIT_DEFAULT;
  RTCC_CCChConf_TypeDef initCC0 = RTCC_CH_INIT_COMPARE_DEFAULT;
  RTCC_CCChConf_TypeDef initCC1 = RTCC_CH_INIT_COMPARE_DEFAULT;
  initRTCC.enable = false;
  initRTCC.presc = rtccCntPresc_1;
  initRTCC.prescMode = rtccCntTickPresc;
  RTCC_ChannelCompareValueSet(0, 32768);
  RTCC_ChannelCompareValueSet(1, 50000);
  RTCC_ChannelInit(0, &initCC0);
  RTCC_ChannelInit(1, &initCC1);
  RTCC_Init(&initRTCC);
  RTCC_IntEnable(RTCC_IEN_CC0 + RTCC_IEN_CC1);
  NVIC_EnableIRQ(RTCC_IRQn);
}
/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  GPIO_setup();
  RTCC_setup();
}
/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void) {}
/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header))
  {
    case sl_bt_evt_system_boot_id:
      setAdvScanData();
      printf("Hold button to enable new bonding(3 seconds).\n");
      break;
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      connection_handle = evt->data.evt_connection_opened.connection;
      bonded = evt->data.evt_sm_bonded.bonding;
      status = evt->data.evt_connection_opened.bonding;
      if(status == 0)
      {
        GPIO_PinOutSet(SL_EMLIB_GPIO_INIT_RELAY_PIN_PORT,                       // switch on relay
                       SL_EMLIB_GPIO_INIT_RELAY_PIN_PIN);
      }
    break;
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      sl_bt_system_set_soft_timer(0, TIMER_ID, 0);                              // stop timer
      sl_bt_advertiser_start(adv_set_handle, sl_bt_advertiser_user_data,
                             sl_bt_advertiser_connectable_scannable);
      printf("connection closed, reason: 0x%2.2x\n\n",
             evt->data.evt_connection_closed.reason);
      GPIO_PinOutClear(SL_EMLIB_GPIO_INIT_RELAY_PIN_PORT,
                        SL_EMLIB_GPIO_INIT_RELAY_PIN_PIN);                      // switch off relay
      break;
    case sl_bt_evt_system_soft_timer_id:                                        // periodic timer event
      if (evt->data.evt_system_soft_timer.handle == TIMER_ID)
      {
        counter++;
        sl_bt_gatt_server_write_attribute_value(gattdb_counter_value, 0, 1,
                                                (uint8_t*)&counter);
      }
      break;
    /***************** BUTTON EVENTS ******************/                        // button events
    case sl_bt_evt_system_external_signal_id:
      // when holding button
      if (totalTime >= 3 && count % 4 == 2)
      {
        // new bondings are enabled
        printf("All connections are enabled.\n");
        buttonHeld = true;
        printf("Click button to erase all previous bondings.\n");
        printf("Hold button to keep previous bondings.\n");
      }
      if (totalTime < 3 && count % 4 == 2)
      {
        printf("Existing bondings:\n");
        sl_bt_sm_configure(0x07, sm_io_capability_keyboardonly);                // passkey displayed by this app (responder)
        sl_bt_sm_set_bondable_mode(1);                                          // enable new bondings
        passkey = setRandomPasskey();                                           // set passkey to be displayed
        sl_bt_sm_set_passkey(passkey);                                          // with displayonly I/O capability
        sl_bt_sm_list_all_bondings();                                           // list all existing bondings
      }
      if (buttonHeld == true  && click > 0 && click < 20)
      {
        sl_bt_sm_delete_bondings();                                             // erase all existng bondings
        printf("All bondings are erased.\n");
        printf("Existing bondings:\n");
        sl_bt_sm_configure(0x07, sm_io_capability_keyboardonly);                // passkey displayed by this app (responder)
        sl_bt_sm_set_bondable_mode(1);                                          // enable new bondings
        passkey = setRandomPasskey();                                           // set passkey to be displayed
        sl_bt_sm_set_passkey(passkey);                                          // with displayonly I/O capability
        sl_bt_sm_list_all_bondings();                                           // list all existing bondings
      }
      if (buttonHeld == true  && click >= 20 )
      {
        printf("Previous bondings are kept.\n");
        printf("Existing bondings:\n");
        sl_bt_sm_configure(0x07, sm_io_capability_keyboardonly);                // passkey displayed by this app (responder)
        sl_bt_sm_set_bondable_mode(1);                                          // enable new bondings
        passkey = setRandomPasskey();                                           // set passkey to be displayed
        sl_bt_sm_set_passkey(passkey);                                          // with displayonly I/O capability
        sl_bt_sm_list_all_bondings();                                           // list all existing bondings
      }
      break;
    /***************** Security related events ******************/
    case sl_bt_evt_sm_list_bonding_entry_id:                                    // next existing bonding record
      bonding = evt->data.evt_sm_list_bonding_entry.bonding;                    // bonding index
      addr = evt->data.evt_sm_list_bonding_entry.address.addr;                  // bonded address
      printf("%d: %02x:%02x:%02x:%02x:%02x:%02x \r\n", bonding,                 // print all the above
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
      break;

    case sl_bt_evt_sm_list_all_bondings_complete_id:                            // end of existing bondings list
      printf("End of list\n");
      printf("starting advertisements\n");                                      // just start advertisements
      sl_bt_advertiser_set_timing(adv_set_handle, 160, 160, 0, 0);              // set 100ms advert. period
      sl_bt_advertiser_start(adv_set_handle, sl_bt_advertiser_user_data,
                                  sl_bt_advertiser_connectable_scannable);
      break;

    case sl_bt_evt_sm_passkey_display_id:                                       // passkey for establishing new bonding
      printf("Type in this passkey on phone: %d\n",
             (int)evt->data.evt_sm_passkey_display.passkey);
      break;

    case sl_bt_evt_sm_passkey_request_id:                                       // enter passkey displayed on phone
      printf("Enter the passkey displayed on phone: ");
      passkey = getPasskey();                                                   // get user input (passkey)
      printf("\nEntered passkey: %d\n", (int)passkey);
      sl_bt_sm_enter_passkey(connection_handle, passkey);                       // set entered passkey
      break;

    case sl_bt_evt_sm_confirm_bonding_id:                                       // new bonding request is received
      sl_bt_sm_bonding_confirm(connection_handle, 1);                           // accept bonding request
      printf("bonding request accepted\n");
      break;

    case sl_bt_evt_sm_bonded_id:                                                // new bonding outcome (success)
      printf("bond success\n");
      break;

    case sl_bt_evt_sm_bonding_failed_id:                                        // new bonding outcome (failure)
      printf("bonding failed, reason 0x%2X\n",
              evt->data.evt_sm_bonding_failed.reason);
      sl_bt_connection_close(evt->data.evt_sm_bonding_failed.connection);
      break;

    case sl_bt_evt_sm_confirm_passkey_id:                                       // used in numeric comparison mode
      printf("Do you see this passkey on the other device: %06u? (y/n)  ",
             (int)evt->data.evt_sm_confirm_passkey.passkey);
      do {c = getchar();} while (c == 255);                                     // wait for key pressed
      printf("%c\n", c);                                                        // echo the user input
      if (c == 'y' || c == 'Y')
      {
        sl_bt_sm_passkey_confirm(connection_handle, 1);                         // confirm passkey match
        printf("Waiting for other device to confirm...\n");
      }
      else
      {
        sl_bt_sm_passkey_confirm(connection_handle, 0);                         // do not confirm passkey match
      }
      break;

    default:
      break;
  }
}

uint32_t setRandomPasskey(void)
{
  uint32_t n;                                                                   // get 6-digit TRN
  sl_bt_system_get_random_data(4, 4, NULL, (uint8_t*)&n);
  n = (n % 900000) + 100000;
  return(n);
}

uint32_t getPasskey(void)                                                       // read 6-digit pass-key
{
  uint32_t n = 0;
  for (uint8_t i=0; i<6; i++)
  {
    do {c = getchar();} while (c == 255);                                       // wait for key pressed
    putchar(c);                                                                 // echo char to terminal
    n = n*10 + (c - '0');
  }
  return(n);
}

static void setAdvScanData(void)
{
  advData.len_flags = 0x02;
  advData.type_flags = 0x01;
  advData.val_flags = 0x06;                                                     // general discoverable mode

  advData.len_manuf = 15;
  advData.type_manuf = 0xFF;
  advData.company_LO = 0xFF;                                                    // Silabs company ID (0x02FF)
  advData.company_HI = 0x02;
  strncpy(advData.manuf_name, "Silicon Labs", 12);

  scanData.len_name = 14;
  scanData.type_name = 9;                                                       // full device name
  strncpy(scanData.name, "BTRelay", 7);

  // Create an advertising set.
  sc = sl_bt_advertiser_create_set(&adv_set_handle);
  app_assert(sc == SL_STATUS_OK,
             "[E: 0x%04x] Failed to create advertising set\n",
             (int)sc);

  // Set custom advertising data.
  sc = sl_bt_advertiser_set_data(adv_set_handle, 0, sizeof(advData),
                                 (uint8_t *)(&advData));
  app_assert(sc == SL_STATUS_OK,
             "[E: 0x%04x] Failed to set advertiser data\n",
             (int)sc);

  // Set custom scan response data.
  sc = sl_bt_advertiser_set_data(adv_set_handle, 1, sizeof(scanData),
                                 (uint8_t *)(&scanData));
  app_assert(sc == SL_STATUS_OK,
             "[E: 0x%04x] Failed to set advertiser data\n",
             (int)sc);
}

static void button(const uint8_t pin)
{
  if (pin == SL_EMLIB_GPIO_INIT_BUTTON_PIN)
  {
    sl_bt_external_signal(BUTTON);
    count ++;
    if (count % 4 == 1)
    {
      RTCC_CounterSet(0);
      RTCC_Start();
    }
    if (count % 4 == 2)
    {
      totalTime = (((int)RTCC_CounterGet()) / 1000) / 32;                       // time in seconds
    }
    if (count % 4 == 3)
    {
      RTCC_CounterSet(0);
      RTCC_Start();
    }
    if (count % 4 == 0)
    {
      click = (((int)RTCC_CounterGet()) / 10) / 32;                             // time in half-seconds
    }
  }
}
