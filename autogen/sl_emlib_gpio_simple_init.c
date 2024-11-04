#include "sl_emlib_gpio_simple_init.h"
#include "sl_emlib_gpio_init_BUTTON_config.h"
#include "sl_emlib_gpio_init_RELAY_PIN_config.h"
#include "em_gpio.h"
#include "em_cmu.h"

void sl_emlib_gpio_simple_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(SL_EMLIB_GPIO_INIT_BUTTON_PORT,
                  SL_EMLIB_GPIO_INIT_BUTTON_PIN,
                  SL_EMLIB_GPIO_INIT_BUTTON_MODE,
                  SL_EMLIB_GPIO_INIT_BUTTON_DOUT);

  GPIO_PinModeSet(SL_EMLIB_GPIO_INIT_RELAY_PIN_PORT,
                  SL_EMLIB_GPIO_INIT_RELAY_PIN_PIN,
                  SL_EMLIB_GPIO_INIT_RELAY_PIN_MODE,
                  SL_EMLIB_GPIO_INIT_RELAY_PIN_DOUT);
}
