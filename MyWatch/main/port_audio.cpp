#include "port_audio.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <math.h>

static const char *TAG = "AUDIO";

static esp_codec_dev_handle_t speaker_handle = NULL;
static esp_codec_dev_handle_t microphone_handle = NULL;

esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "Initializing audio codecs...");

    speaker_handle = bsp_audio_codec_speaker_init();
    if (speaker_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize speaker codec");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Speaker codec (ES8311) initialized");

    microphone_handle = bsp_audio_codec_microphone_init();
    if (microphone_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize microphone codec");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Microphone codec (ES7210) initialized");

    return ESP_OK;
}

esp_err_t audio_deinit(void)
{
    if (speaker_handle) {
        esp_codec_dev_delete(speaker_handle);
        speaker_handle = NULL;
    }
    if (microphone_handle) {
        esp_codec_dev_delete(microphone_handle);
        microphone_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t audio_play(const uint8_t *data, size_t len)
{
    if (speaker_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_codec_dev_write(speaker_handle, (void *)data, len);
}

esp_err_t audio_record(uint8_t *data, size_t len)
{
    if (microphone_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_codec_dev_read(microphone_handle, data, len);
}

esp_err_t audio_beep(int frequency_hz, int duration_ms)
{
    if (speaker_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const int sample_rate = 44100;
    const int num_samples = sample_rate * duration_ms / 1000;
    int16_t *beep_data = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (beep_data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float envelope = 1.0f;
        float fade_time = 0.01f;
        if (t < fade_time) {
            envelope = t / fade_time;
        } else if (t > (duration_ms / 1000.0f - fade_time)) {
            envelope = (duration_ms / 1000.0f - t) / fade_time;
        }
        beep_data[i] = (int16_t)(16384 * envelope * sinf(2 * 3.14159f * frequency_hz * t));
    }

    esp_err_t ret = esp_codec_dev_write(speaker_handle, beep_data, num_samples * sizeof(int16_t));
    free(beep_data);

    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
    }

    return ret;
}
