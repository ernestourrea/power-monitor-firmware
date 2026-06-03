// components/board/include/pin_config.h

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "driver/gpio.h"
#include "hal/adc_types.h"

/* Board pins configuration. */
#define BOARD_GPIO_RELAY              GPIO_NUM_16
#define BOARD_GPIO_RELAY_FEEDBACK     GPIO_NUM_11
#define BOARD_GPIO_STATUS_LED         GPIO_NUM_8
#define BOARD_GPIO_PROV_BUTTON        GPIO_NUM_15
#define BOARD_GPIO_ZC_VOLTAGE         GPIO_NUM_7
#define BOARD_GPIO_ZC_CURRENT         GPIO_NUM_6

#define BOARD_ADC_UNIT                ADC_UNIT_1
#define BOARD_ADC_ATTEN               ADC_ATTEN_DB_12
#define BOARD_ADC_VOLTAGE_CHANNEL     ADC_CHANNEL_4
#define BOARD_ADC_CURRENT_CHANNEL     ADC_CHANNEL_3

#endif // PIN_CONFIG_H
