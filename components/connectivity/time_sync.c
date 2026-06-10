// components/connectivity/time_sync.c

#include "time_sync.h"

#include "esp_netif_sntp.h"

esp_err_t time_sync_init(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    return esp_netif_sntp_init(&config);
}

esp_err_t time_sync_start(void)
{
    return esp_netif_sntp_start();
}
