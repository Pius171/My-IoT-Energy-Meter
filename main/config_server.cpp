#include "config_server.hpp"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"

#include "esp_err.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include "esp_idf_version.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "spi_flash_mmap.h"

#include "esp_littlefs.h"
#include <string>
#include <fstream>

#define EXAMPLE_ESP_WIFI_SSID "MG001"
#define EXAMPLE_ESP_WIFI_PASS ""
#define EXAMPLE_ESP_WIFI_CHANNEL 1
#define EXAMPLE_MAX_STA_CONN 4

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define EXAMPLE_GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define EXAMPLE_GTK_REKEY_INTERVAL 0
#endif

#define CONFIG_FILE_PATH "/littlefs/meter_config.json"
static const char *TAG = "wifi softAP";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS, // Moved up to match struct order
            .ssid_len = (uint8_t)strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .authmode = (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .pmf_cfg = {
                .required = true,
            },
            .gtk_rekey_interval = EXAMPLE_GTK_REKEY_INTERVAL,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // print ip address
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(TAG, "AP IP Address: " IPSTR, IP2STR(&ip_info.ip));

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

static esp_err_t init_littlefs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = false,
        .dont_mount = false,
        .grow_on_mount = false

    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LittleFS");
        return ret;
    }
    return ESP_OK;
}

static esp_err_t hello_get_handler(httpd_req_t *req)
{
    FILE *f = fopen("/littlefs/index.html", "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char chunk[1024];
    size_t chunksize;
    while ((chunksize = fread(chunk, 1, sizeof(chunk), f)) > 0)
    {
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
        {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Reset requested via Web Portal...");
    httpd_resp_sendstr(req, "Restarting ESP32... Please wait.");

    // Give the webserver a moment to finish sending the response
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_restart();
    return ESP_OK; // Technically never reached, but satisfies compiler
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[] = CONFIG_FILE_PATH;
    FILE *f = fopen(filepath, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry if timeout occurred */
                continue;
            }
            fclose(f);
            ESP_LOGE(TAG, "File receive failed");
            return ESP_FAIL;
        }

        /* Write received data to file */
        size_t written = fwrite(buf, 1, ret, f);
        if (written != ret)
        {
            fclose(f);
            ESP_LOGE(TAG, "File write failed");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        remaining -= ret;
    }

    fclose(f);
    ESP_LOGI(TAG, "File received successfully: %s", filepath);

    /* Respond to the user */
    httpd_resp_sendstr(req, "File uploaded successfully. MITA system updating...");

    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_uri_t hello = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = hello_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &hello);

        // for file upload
        httpd_uri_t upload_uri = {
            .uri = "/upload",
            .method = HTTP_POST,
            .handler = upload_post_handler,
            .user_ctx = NULL};

        httpd_register_uri_handler(server, &upload_uri);

        httpd_uri_t reset_uri = {
            .uri = "/reset",
            .method = HTTP_POST,
            .handler = reset_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &reset_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void run_config_server()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    init_littlefs();
    wifi_init_softap();
    start_webserver();
}

bool is_config_file_present()
{
    struct stat st;
    return (stat(CONFIG_FILE_PATH, &st) == 0);
}

std::string load_file_to_string()
{
    struct stat st;
    // 1. Check if file exists and get size
    if (stat(CONFIG_FILE_PATH, &st) != 0)
    {
        ESP_LOGE(TAG, "File %s not found", CONFIG_FILE_PATH);
        return "";
    }

    // 2. Open the file
    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return "";
    }

    // 3. Prepare the string (allocate memory once)
    std::string contents;
    contents.resize(st.st_size); // RESIZE THE STRING TO THE SIZE OF THE FILE. st.st_size= file size

    // 4. Read the data into the string buffer
    size_t read_size = fread(&contents[0], 1, st.st_size, f);
    fclose(f);

    if (read_size != (size_t)st.st_size)
    {
        ESP_LOGW(TAG, "Read size mismatch: expected %ld, got %d", st.st_size, read_size);
    }

    return contents;
}