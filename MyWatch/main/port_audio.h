#ifndef PORT_AUDIO_H
#define PORT_AUDIO_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES8311_I2C_ADDR  0x18
#define ES7210_I2C_ADDR  0x48

#define AUDIO_PA_PIN     GPIO_NUM_6
#define AUDIO_I2S_MCLK   GPIO_NUM_19
#define AUDIO_I2S_SCLK   GPIO_NUM_20
#define AUDIO_I2S_ASDOUT GPIO_NUM_21
#define AUDIO_I2S_LRCK   GPIO_NUM_22
#define AUDIO_I2S_DSDIN   GPIO_NUM_23

esp_err_t audio_init(void);
esp_err_t audio_deinit(void);
esp_err_t audio_play(const uint8_t *data, size_t len);
esp_err_t audio_record(uint8_t *data, size_t len);
esp_err_t audio_beep(int frequency_hz, int duration_ms);

#ifdef __cplusplus
}
#endif

#endif
