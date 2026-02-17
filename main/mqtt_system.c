#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "mqtt_client.h"
#include "mqtt_system.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"


// Defines for MQTT connection parameters (to be set in Kconfig)
#define BROKER_MQTT_HOST                CONFIG_MQTT_BROKER_HOST
#define BROKER_MQTT_USERNAME            CONFIG_MQTT_USERNAME
#define BROKER_MQTT_PASSWORD            CONFIG_MQTT_PASSWORD
#define BROKER_MQTT_TOPICS_LIST         CONFIG_MQTT_TOPICS_LIST
#define BROKER_TRY_CONNECT_TIMEOUT_MS   CONFIG_MQTT_TRY_CONNECT_TIMEOUT_MS

// Define for event group bits
#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_DISCONNECTED_BIT   BIT1


// Tag for logging
static const char *TAG = "mqtt_system";


// Private static variable for MQTT client handle
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

// Event groups for MQTT events
static EventGroupHandle_t s_mqtt_event_group = NULL;


// Calback function for MQTT events
mqtt_rx_callback_t mqtt_rx_callback = NULL;


void subscribe_to_kconfig_topics(esp_mqtt_client_handle_t client) {
    if (BROKER_MQTT_TOPICS_LIST[0] == '\0') {
        ESP_LOGW(TAG, "MQTT_TOPICS_LIST is empty, skipping subscriptions");
        return;
    }

    // Make a mutable copy before tokenizing
    char topics_str[256];
    snprintf(topics_str, sizeof(topics_str), "%s", BROKER_MQTT_TOPICS_LIST);
    
    // Split topics by spaces
    char *topic = strtok(topics_str, ";");
    
    while (topic != NULL) {
        ESP_LOGI(TAG, "Subscribing to topic: %s", topic);
        esp_mqtt_client_subscribe(client, topic, 0);
        
        topic = strtok(NULL, ";");
    }
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)event_base;
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            subscribe_to_kconfig_topics(s_mqtt_client);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_DISCONNECTED_BIT);
            xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            if (mqtt_rx_callback != NULL) {
                mqtt_rx_callback(event->topic, event->data,event->topic_len, event->data_len);
            }
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            ESP_LOGD(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGD(TAG, "DATA=%.*s", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGE(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

void mqtt_system_init(mqtt_rx_callback_t callback_for_mqtt_received_data)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");

    // Set the callback for received MQTT data
    mqtt_rx_callback = callback_for_mqtt_received_data;

    s_mqtt_event_group = xEventGroupCreate();
    if (s_mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT event group");
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_MQTT_HOST,
        .credentials =  {
            .username = BROKER_MQTT_USERNAME,
            .authentication.password = BROKER_MQTT_PASSWORD,
        },
        .network.timeout_ms = BROKER_TRY_CONNECT_TIMEOUT_MS , // Optional: set network timeout
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

} 


bool mqtt_connection_is_connected(void)
{
    if (s_mqtt_event_group == NULL) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(s_mqtt_event_group);
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

esp_err_t mqtt_service_publish(const char *topic, const char *payload)
{
    if (!mqtt_connection_is_connected())
    {
        ESP_LOGE(TAG, "Cannot publish message, MQTT client is not connected");
        return ESP_FAIL;
    }
    int8_t msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;    
}
