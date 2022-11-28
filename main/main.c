/* USB Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tusb_audio.h"
#include "tusb_cdc_acm.h"
#include "sig_gen.h"

#define SAMPLES_BYTES_NUM CFG_TUD_AUDIO_EP_SZ_IN
#define SAMPLES_NUM CFG_TUD_AUDIO_EP_SZ_IN / 2

static const char *TAG = "USB audio example";

static uint8_t cdc_buf[CONFIG_ESP_TINYUSB_CDC_RX_BUFSIZE + 1];
uint16_t audio_buffer[SAMPLES_BYTES_NUM];
sig_gen_t sine_ch1;
sig_gen_t sine_ch2;

/* ----------------- AUDIO ------------------- */
void tinyusb_audio_tx_callback(uint8_t *buffer, uint16_t *bytes)
{
    uint16_t ch1_samples[48];
    uint16_t ch2_samples[48];
    // Get samples from signal generator
    sig_gen_output(&sine_ch1, (uint8_t *)ch1_samples, 48);
    sig_gen_output(&sine_ch2, (uint8_t *)ch2_samples, 48);

    // Arrange samples in buffer
    uint16_t *p_buff = audio_buffer;
    for (int samples_num = 0; samples_num < 48; samples_num++) {
        *p_buff++ = ch1_samples[samples_num];
        *p_buff++ = ch2_samples[samples_num];
    }

    memcpy(buffer, audio_buffer, SAMPLES_BYTES_NUM);
    *bytes = SAMPLES_BYTES_NUM;
}

/* ----------------- SERIAL ------------------- */
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(itf, cdc_buf, CONFIG_ESP_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        cdc_buf[rx_size] = '\0';
        ESP_LOGI(TAG, "Got data (%d bytes): %s", rx_size, cdc_buf);
    }
    else {
        ESP_LOGE(TAG, "Read error");
    }

    /* write back */
    tinyusb_cdcacm_write_queue(itf, cdc_buf, rx_size);
    tinyusb_cdcacm_write_flush(itf, 0);
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rst = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "Line state changed! dtr:%d, rst:%d", dtr, rst);
}

/* -------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = {
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    sig_gen_config_t sig_gen_cfg = {
        .gen_source = SINE_LUT,
        .lut_freq = LUT_FREQ_440,
        .bytes_per_sample = CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
        .sample_rate = 48000,
        .endianess = SIG_GEN_LE,
        .enable_cb = SIG_GEN_NO_CB};
    sig_gen_init(&sine_ch1, &sig_gen_cfg);
    sig_gen_cfg.lut_freq = LUT_FREQ_552;
    sig_gen_init(&sine_ch2, &sig_gen_cfg);

    tinyusb_config_audio_t audio_cfg = {
        .audio_tx_callback = &tinyusb_audio_tx_callback};
    tusb_audio_init(&audio_cfg);

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
        .callback_line_coding_changed = NULL
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");

    while(1)
    {
        uint8_t msg_buf[] = "Hello from ESP32 via USB CDC\r\n";
        tinyusb_cdcacm_write_queue(0, msg_buf, sizeof(msg_buf));
        tinyusb_cdcacm_write_flush(0, 0);
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}
