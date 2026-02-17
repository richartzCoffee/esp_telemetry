#ifndef MQTT_SYSTEM_H
#define MQTT_SYSTEM_H

#include <stdbool.h>

typedef void (*mqtt_rx_callback_t)(const char *topic, const char *payload, int topic_len, int payload_len);

void mqtt_system_init(mqtt_rx_callback_t callback_for_mqtt_received_data);
bool mqtt_connection_is_connected(void);
esp_err_t mqtt_service_publish(const char *topic, const char *payload);


#endif /* MQTT_SYSTEM_H */

