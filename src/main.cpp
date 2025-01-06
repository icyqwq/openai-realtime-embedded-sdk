#include "main.h"

#include <esp_event.h>
#include <esp_log.h>
#include <peer.h>

#ifndef LINUX_BUILD
#include "nvs_flash.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>

#include <M5Unified.h>

constexpr const char* TAG = "main";

extern const uint8_t image_bg_start[] asm("_binary_m5_png_start");
extern const uint8_t image_bg_end[] asm("_binary_m5_png_end");

char wifi_ssid[64];
char wifi_password[64];
char openaikey[128];

#ifdef CONFIG_ENABLE_HEAP_MONITOR
static esp_timer_handle_t s_monitor_timer;
#endif // CONFIG_ENABLE_HEAP_MONITOR

bool load_user_data()
{
  nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open("config", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS handle: %s", esp_err_to_name(ret));
        return false;
    }

    size_t required_size = sizeof(wifi_ssid);
    ret = nvs_get_str(nvs_handle, "wifi_ssid", wifi_ssid, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI("NVS", "WiFi SSID: %s", wifi_ssid);
    } else {
        ESP_LOGE("NVS", "Error reading WiFi SSID: %s", esp_err_to_name(ret));
        return false;
    }

    required_size = sizeof(wifi_password);
    ret = nvs_get_str(nvs_handle, "wifi_password", wifi_password, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI("NVS", "WiFi Password: %s", wifi_password);
    } else {
        ESP_LOGE("NVS", "Error reading WiFi Password: %s", esp_err_to_name(ret));
        return false;
    }

    required_size = sizeof(openaikey);
    ret = nvs_get_str(nvs_handle, "openaikey", openaikey, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI("NVS", "OpenAI Key: %s", openaikey);
    } else {
        ESP_LOGE("NVS", "Error reading OpenAI Key: %s", esp_err_to_name(ret));
        return false;
    }

    nvs_close(nvs_handle);

    return true;
}

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);


#ifdef CONFIG_ENABLE_HEAP_MONITOR
  esp_timer_create_args_t timer_args = {
      .callback = [](void* arg) {
    ESP_LOGW(TAG, "current heap %7d | minimum ever %7d | largest free %7d ",
             xPortGetFreeHeapSize(),
             xPortGetMinimumEverFreeHeapSize(),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
      },
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "monitor_timer"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_monitor_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(s_monitor_timer, CONFIG_HEAP_MONITOR_INTERVAL_MS * 1000ULL));
#endif // CONFIG_ENABLE_HEAP_MONITOR

  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);

  M5.Lcd.fillScreen(TFT_WHITE);

  if (!load_user_data()) {
    M5.Lcd.fillScreen(TFT_RED);
    M5.Lcd.drawString("Failed to load config data", 0, 0);
    ESP_LOGE(TAG, "Failed to load user data");
    return;
  }

  bool res = M5.Lcd.drawPng(image_bg_start, image_bg_end - image_bg_start, 0, 0, 320, 240);

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_wifi();
  
  oai_init_audio_capture();
  oai_init_audio_decoder();
  
  oai_webrtc();
}
#else
int main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_webrtc();
}
#endif
