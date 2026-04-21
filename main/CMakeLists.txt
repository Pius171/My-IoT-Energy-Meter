idf_component_register(SRCS "modbus_handler.cpp" "config_server.cpp" "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver modbus_parser esp_wifi nvs_flash esp_http_server spi_flash esp_partition myPPP esp_netif my_mqtt)

littlefs_create_partition_image(littlefs ../flash_data FLASH_IN_PROJECT)