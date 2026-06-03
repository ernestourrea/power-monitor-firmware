idf_component_register(
    SRCS 
        "ble_provisioning.c"
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
)
