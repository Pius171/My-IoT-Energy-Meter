idf_component_register(SRCS "myPPP.c"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos esp_netif esp_modem esp_event)
