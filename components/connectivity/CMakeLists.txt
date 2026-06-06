idf_component_register(
    SRCS 
        "ble_provisioning.c"
        "ble_prov_security.c"
        "wifi_manager.c"
        "connectivity.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        esp_wifi 
        esp_netif 
        esp_event mqtt 
        json
        bt
        protocomm
        network_provisioning
        common
)
