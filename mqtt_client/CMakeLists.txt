set(srcs)
set(include_dirs "include")

if(CONFIG_OW_MQTT_CLIENT_ENABLE)
    list(APPEND srcs
        "mqtt_client.c")
endif()

idf_component_register(SRCS ${srcs}          
                       INCLUDE_DIRS "${include_dirs}" 
                       PRIV_REQUIRES esp_event freertos mqtt
                       )
