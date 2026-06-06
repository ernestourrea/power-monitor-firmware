// components/connectivity/wifi_manager.c

#include "wifi_manager.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "connectivity.h"

static const char *TAG = "wifi_mgr";

static EventGroupHandle_t s_wifi_events;

esp_err_t wifi_manager_attempt_reconnect(void)
{
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi reconnect failed: %s", esp_err_to_name(err));
        connectivity_post_event(CONN_EVT_WIFI_DISCONNECTED, err);
        return err;
    }

    return ESP_OK;
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
        connectivity_post_event(CONN_EVT_WIFI_STARTED, 0);
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)data;
        connectivity_post_event(CONN_EVT_WIFI_DISCONNECTED, disc ? disc->reason : 0);
        // TODO: move to connectivity component
        //post_app_event(APP_EVENT_WIFI_DISCONNECTED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) data;
        connectivity_post_event(CONN_EVT_WIFI_CONNECTED, 0);
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
        connectivity_post_event(CONN_EVT_NO_CREDENTIALS, 0);
        // app_event_t event = { .id = APP_EVENT_WIFI_DISCONNECTED };
        // app_core_post_event(&event, 0);
        return ESP_OK;
        // return ble_provisioning_start();
    }

    connectivity_post_event(CONN_EVT_CREDENTIALS_FOUND, 0);
    ESP_LOGI(TAG, "using stored WiFi credentials for SSID: %s", (const char *)wifi_config.sta.ssid);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    return esp_wifi_start();
}

esp_err_t wifi_manager_stop(void)
{
    // TODO: handle errors
    (void)esp_wifi_disconnect();
    (void)esp_wifi_stop();
    return ESP_OK;
}

void wifi_manager_get_mac(uint8_t mac[6])
{
    esp_wifi_get_mac(WIFI_IF_STA, mac);
}
