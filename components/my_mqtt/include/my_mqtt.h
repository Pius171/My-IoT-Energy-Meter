#ifndef MY_MQTT_H
#define MY_MQTT_H

#include "mqtt_client.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

void my_mqtt_config();
void my_mqtt_publish(const char* payload, int qos);


#ifdef __cplusplus
}
#endif
#endif // MY_MQTT_H