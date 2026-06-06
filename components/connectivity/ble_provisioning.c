// components/connectivity/ble_provisioning.c

#include "ble_provisioning.h"
#include "ble_prov_security.h"

#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include <network_provisioning/manager.h>
#include <network_provisioning/scheme_ble.h>

#include "connectivity.h"

static const char *TAG = "ble_prov";

static bool s_mgr_initialized = false;
static bool s_provisioning_active = false;
static bool s_is_provisioned = false;

static network_prov_security2_params_t s_sec2_params;

static esp_err_t ble_provisioning_start_service(void);
static esp_err_t ble_provisioning_ensure_mgr_initialized(void);

/* Event handler for catching system events */
static void ble_provisioning_event_handler(void *arg, 
                                           esp_event_base_t event_base,
                                           int32_t event_id, 
                                           void *event_data )
{
    (void)arg;

    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            connectivity_post_event(CONN_EVT_PROVISIONING_STARTED, 0);
            s_provisioning_active = true;
            break;
        case NETWORK_PROV_WIFI_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials"
                     "\n\tSSID     : %s\n\tPassword : %s",
                     (const char *) wifi_sta_cfg->ssid,
                     (const char *) wifi_sta_cfg->password);
            break;
        }
        case NETWORK_PROV_WIFI_CRED_FAIL: {
            network_prov_wifi_sta_fail_reason_t *reason = (network_prov_wifi_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                     "\n\tPlease check SSID/password and retry provisioning",
                     (*reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR) ?
                     "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
            connectivity_post_event(CONN_EVT_PROVISIONING_FAILED, *reason);
            // Reset the state machine on provisioning failure.
            network_prov_mgr_reset_wifi_sm_state_on_failure();
            break;
        }
        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            connectivity_post_event(CONN_EVT_PROVISIONING_SUCCESS, 0);
            s_is_provisioned = true;
            break;
        case NETWORK_PROV_END:
            ESP_LOGI(TAG, "Provisioning ended");
            connectivity_post_event(CONN_EVT_PROVISIONING_STOPPED, 0);
            s_provisioning_active = false;
            esp_err_t err = network_prov_mgr_deinit();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to de-initialize provisioning manager: %s", esp_err_to_name(err));
            }
            s_mgr_initialized = false;
            break;
        default:
            break;
        }
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "BLE transport: Connected!");
            break;
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "BLE transport: Disconnected!");
            break;
        default:
            break;
        }
    } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            ESP_LOGI(TAG, "Secured session established!");
            break;
        case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!");
            break;
        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing secure session!");
            break;
        default:
            break;
        }
    }
}

/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 * Note that memory for the response buffer must be allocated using heap as this buffer
 * gets freed by the protocomm layer once it has been sent by the transport layer.
 */
static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    (void)session_id;
    (void)priv_data;

    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

static esp_err_t ble_provisioning_ensure_mgr_initialized(void)
{
    if (s_mgr_initialized) {
        return ESP_OK;
    }

    /* Configuration for the provisioning manager */
    network_prov_mgr_config_t config = {
        .network_prov_wifi_conn_cfg = {
            .wifi_conn_attempts = 5,
        },
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
    };

    esp_err_t err = network_prov_mgr_init(config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize provisioning manager: %s", esp_err_to_name(err));
        return err;
    }

    s_mgr_initialized = true;
    return ESP_OK;
}

esp_err_t ble_provisioning_init(void)
{
    /* Register our event handler for provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, &ble_provisioning_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &ble_provisioning_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &ble_provisioning_event_handler, NULL));

    return ble_provisioning_ensure_mgr_initialized();
}

esp_err_t ble_provisioning_start(void)
{
    ESP_RETURN_ON_ERROR(ble_provisioning_ensure_mgr_initialized(), TAG, "Provisioning manager init failed");

    bool provisioned = false;

    /* Let's find out if the device is provisioned */
    ESP_RETURN_ON_ERROR(network_prov_mgr_is_wifi_provisioned(&provisioned), TAG, "Failed to read provisioned state");

    ESP_LOGI(TAG, "Wi-Fi provisioned state: %s", provisioned ? "true" : "false");

    /* Set the global variable to indicate provisioning status */
    s_is_provisioned = provisioned;

    if (provisioned) {
        ESP_LOGI(TAG, "Device is already provisioned; BLE provisioning will not be started");
        return ESP_OK;
    }

    /* If device is not yet provisioned start provisioning service */
    return ble_provisioning_start_service();
}

static esp_err_t ble_provisioning_start_service(void)
{
    if (s_provisioning_active) {
        ESP_LOGW(TAG, "Provisioning is already active");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting BLE provisioning");

    char service_name[12];
    connectivity_get_device_service_name(service_name, sizeof(service_name));

    network_prov_security_t security = NETWORK_PROV_SECURITY_2;
    /* The username must be the same one, which has been used in the generation of salt and verifier */

    /* This pop field represents the password that will be used to generate salt and verifier.
     * The field is present here in order to generate the QR code containing password.
     * In production this password field shall not be stored on the device */
    const char *username  = EXAMPLE_PROV_SEC2_USERNAME;
    const char *pop = EXAMPLE_PROV_SEC2_PWD;
    const char *service_key = NULL;

    memset(&s_sec2_params, 0, sizeof(s_sec2_params));
    ESP_RETURN_ON_ERROR(example_get_sec2_salt(&s_sec2_params.salt, &s_sec2_params.salt_len), TAG, "Failed to get Sec2 salt");
    ESP_RETURN_ON_ERROR(example_get_sec2_verifier(&s_sec2_params.verifier, &s_sec2_params.verifier_len), TAG, "Failed to get Sec2 verifier");

    /* This step is only useful when scheme is network_prov_scheme_ble. This will
     * set a custom 128 bit UUID which will be included in the BLE advertisement
     * and will correspond to the primary GATT service that provides provisioning
     * endpoints as GATT characteristics. Each GATT characteristic will be
     * formed using the primary service UUID as base, with different auto assigned
     * 12th and 13th bytes (assume counting starts from 0th byte). The client side
     * applications must identify the endpoints by reading the User Characteristic
     * Description descriptor (0x2901) for each characteristic, which contains the
     * endpoint name of the characteristic */
    uint8_t custom_service_uuid[] = {
        /* LSB <---------------------------------------
         * ---------------------------------------> MSB */
        0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };
    network_prov_scheme_ble_set_service_uuid(custom_service_uuid);

    /* An optional endpoint that applications can create if they expect to
     * get some additional custom data during provisioning workflow.
     * The endpoint name can be anything of your choice.
     * This call must be made before starting the provisioning.
     */
    ESP_RETURN_ON_ERROR(network_prov_mgr_endpoint_create("custom-data"), TAG, "Failed to create custom endpoint");

    esp_err_t err = network_prov_mgr_start_provisioning(security, &s_sec2_params, service_name, service_key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning: %s", esp_err_to_name(err));
        return err;
    }

    ESP_RETURN_ON_ERROR(network_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL), TAG,
                        "Failed to register custom endpoint");

    wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_BLE);
    return ESP_OK;
}

esp_err_t ble_provisioning_stop(void)
{
    if (!s_provisioning_active) {
        return ESP_OK;
    }

    network_prov_mgr_stop_provisioning();
    return ESP_OK;
}

esp_err_t ble_provisioning_reprovision(void)
{
    ESP_LOGW(TAG, "Reprovisioning requested; clearing stored Wi-Fi credentials");

    ESP_RETURN_ON_ERROR(ble_provisioning_ensure_mgr_initialized(), TAG, "Provisioning manager init failed");

    ESP_RETURN_ON_ERROR(network_prov_mgr_reset_wifi_provisioning(), TAG, "Failed to reset Wi-Fi provisioning");
    s_is_provisioned = false;

    return ble_provisioning_start_service();
}

// TODO: Remove if not needed
bool ble_provisioning_is_active(void)
{
    return s_provisioning_active;
}

// TODO: Remove if not needed
bool ble_provisioning_is_provisioned(void)
{
    return s_is_provisioned;
}
