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
#include "sig_gen.h"

#define SAMPLES_BYTES_NUM CFG_TUD_AUDIO_EP_SZ_IN
#define SAMPLES_NUM CFG_TUD_AUDIO_EP_SZ_IN / 2

static const char *TAG = "USB audio example";

uint16_t audio_buffer[SAMPLES_BYTES_NUM];
sig_gen_t sine_ch1;
sig_gen_t sine_ch2;

/* ----------------- AUDIO ------------------- */
bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
   (void)rhport;
   (void)itf;
   (void)ep_in;
   (void)cur_alt_setting;

   tud_audio_write(audio_buffer, CFG_TUD_AUDIO_EP_SZ_IN);

   return true;
}

bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
   (void)rhport;
   (void)n_bytes_copied;
   (void)itf;
   (void)ep_in;
   (void)cur_alt_setting;

   uint16_t ch1_samples[48];
   sig_gen_output(&sine_ch1, (uint8_t *)ch1_samples, 48);
   uint16_t ch2_samples[48];
   sig_gen_output(&sine_ch2, (uint8_t *)ch2_samples, 48);

   uint16_t *p_buff = audio_buffer;
   for (int samples_num = 0; samples_num < 48; samples_num++)
   {
      *p_buff++ = ch1_samples[samples_num];
      *p_buff++ = ch2_samples[samples_num];
   }

   return true;
}

/* ----------------- SERIAL ------------------- */
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        buf[rx_size] = '\0';
        ESP_LOGI(TAG, "Got data (%d bytes): %s", rx_size, buf);
    } else {
        ESP_LOGE(TAG, "Read error");
    }

    /* write back */
    tinyusb_cdcacm_write_queue(itf, buf, rx_size);
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

   tusb_audio_init();

   tinyusb_config_t tusb_cfg = {
      .external_phy = false,
   };
   ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

   tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    /* the second way to register a callback */
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
                        TINYUSB_CDC_ACM_0,
                        CDC_EVENT_LINE_STATE_CHANGED,
                        &tinyusb_cdc_line_state_changed_callback));
    ESP_LOGI(TAG, "USB initialization DONE");
}

