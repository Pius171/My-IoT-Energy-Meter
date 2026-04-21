#include <stdio.h>
#include "my_mqtt.h"

static const char *TAG = "my_mqtt";
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT Published, msg_id=%d", event->msg_id);
        break;
    default:
        break;
    }
}

void my_mqtt_config(){
        esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.username = CONFIG_MQTT_USERNAME,
    };

    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

// uint32_t random_value = esp_random(); // returns a uint32_t, so we can use it to generate random values in the desired ranges
// float voltage = 200.0 + (random_value % 50);
// float current = 10.0 +(random_value % 90);
// float power = voltage * current; // Simple calc for realism
// float frequency = 50.0 + (random_value % 10) / 100.0; // 50-60Hz with some decimal part
// float pf = 0.8 + (random_value % 20) / 100.0; // 0.8-1.0 with some decimal part
// float energy = 100.5 + (random_value % 10) / 10.0; // 100.5-110.5kWh with some decimal part

// char payload[256];
// snprintf(payload, sizeof(payload), 
//     "{\"V1\":%.2f, \"I1\":%.2f, \"power\":%.2f, \"frequency\":%.2f, \"pf\":%.2f, \"energy\":%.2f}", 
//     voltage, current, power, frequency, pf, energy);

// esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", payload, 0, 1, 0);
//         vTaskDelay(pdMS_TO_TICKS(5000));
    
