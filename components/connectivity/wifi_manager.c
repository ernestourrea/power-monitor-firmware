// components/connectivity/wifi_manager.c

#include "wifi_manager.h"

#include "ble_provisioning.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_log.h"

static const char *TAG = "wifi_mgr";

static EventGroupHandle_t s_wifi_events;
static TimerHandle_t s_reconnect_timer;
static int s_retry;
static bool s_reconnect_enabled = true;

static void reconnect_timer_cb(TimerHandle_t timer)
{
    (void)timer;

    if (!s_reconnect_enabled) {
        ESP_LOGI(TAG, "Wi-Fi reconnect disabled; skipping reconnect");
        return;
    }

    esp_wifi_connect();
}

/*
static void post_app_event(app_event_id_t id)
{
    app_event_t event = { .id = id };
    (void)app_core_post_event(&event, 0);
}
*/

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        //post_app_event(APP_EVENT_WIFI_DISCONNECTED);
        const int delay_ms = (s_retry < 6 ? (1 << s_retry) : 60) * 1000;
        s_retry++;
        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(delay_ms), 0);
        xTimerStart(s_reconnect_timer, 0);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry = 0;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        //post_app_event(APP_EVENT_WIFI_CONNECTED);
    }
}

esp_err_t wifi_manager_init(void)
{
    s_wifi_events = xEventGroupCreate();
    if (!s_wifi_events) {
        return ESP_ERR_NO_MEM;
    }
    s_reconnect_timer = xTimerCreate("wifi_reconnect", pdMS_TO_TICKS(1000), pdFALSE, NULL, reconnect_timer_cb);
    if (!s_reconnect_timer) {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    return esp_wifi_init(&cfg);
}

esp_err_t wifi_manager_start(void)
{
    wifi_config_t wifi_config = {0};

    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        return err;
    }

    if (wifi_config.sta.ssid[0] == '\0') {
        ESP_LOGW(TAG, "no WiFi credentials; starting provisioning");
        // app_event_t event = { .id = APP_EVENT_WIFI_DISCONNECTED };
        // app_core_post_event(&event, 0);
        return ble_provisioning_start();
    }

    ESP_LOGI(TAG, "using stored WiFi credentials for SSID: %s", (const char *)wifi_config.sta.ssid);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    return esp_wifi_start();
}

esp_err_t wifi_manager_stop(void)
{
    // TODO: handle errors
    (void)esp_wifi_disconnect();
    return esp_wifi_stop();
}

void wifi_manager_get_mac(uint8_t mac[6])
{
    esp_wifi_get_mac(WIFI_IF_STA, mac);
}

void wifi_manager_set_reconnect_enabled(bool enabled)
{
    s_reconnect_enabled = enabled;
}
