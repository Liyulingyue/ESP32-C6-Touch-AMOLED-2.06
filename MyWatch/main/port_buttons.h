#ifndef PORT_BUTTONS_H
#define PORT_BUTTONS_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_BOOT_PIN GPIO_NUM_9
#define BUTTON_PWR_PIN  GPIO_NUM_18

typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_BOOT_PRESS,
    BUTTON_EVENT_BOOT_LONG_PRESS,
    BUTTON_EVENT_PWR_PRESS,
    BUTTON_EVENT_PWR_LONG_PRESS,
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

esp_err_t buttons_init(void);
esp_err_t buttons_register_callback(button_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif
