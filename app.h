#ifndef APP_H
#define APP_H

#include "stdint.h"
typedef struct
{
  uint8_t len_flags;
  uint8_t type_flags;
  uint8_t val_flags;

  uint8_t len_manuf;
  uint8_t type_manuf;
  uint8_t company_LO;
  uint8_t company_HI;
  char    manuf_name[12];
} customAdv_t;


typedef struct
{
  uint8_t len_name;
  uint8_t type_name;
  char    name[13];
} customScanResp_t;


/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void);

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void);

#endif // APP_H
