#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"


#include "nvs_flash.h"

/* Configuration macros (from Kconfig) */
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


/* Event group bits */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static uint8_t s_retry_num = 0;

static const char *WIFI_CONNECTION = "wifi_connection";


/* esp_timer_start_once expects microseconds */
#define WIFI_RECONNECT_TIMER_PERIOD_US ((uint64_t)CONFIG_WIFI_RECONNECT_INTERVAL_SEC * 1000ULL * 1000ULL) /* 1 minute */


/* Private static variables */
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_event_handler_instance_t instance_any_id;

/* Handle for time*/
esp_timer_handle_t wifi_connection_timer_handle = NULL;


void trigger_timer()
{
    if(esp_timer_is_active(wifi_connection_timer_handle)) {
        esp_timer_stop(wifi_connection_timer_handle);
    }
    esp_timer_start_once(wifi_connection_timer_handle, WIFI_RECONNECT_TIMER_PERIOD_US);
}

void try_reconnect_wifi(void *arg)
{
    (void)arg;
    s_retry_num = 0;
    esp_wifi_connect();
    ESP_LOGI(WIFI_CONNECTION, "Trying to reconnect to the AP...");
}



static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* Driver started; begin connection attempt if not blocked */
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* Disconnected; retry logic */
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            /* Within retry limit: attempt reconnection */
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_CONNECTION, "retry to connect to the AP (%d)", s_retry_num);
        }
        else {
            trigger_timer();
            ESP_LOGI(WIFI_CONNECTION, "Waiting for the next reconnection attempt...");
        }
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED ) {
        /* Disconnected; exceeded retry limit */
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_retry_num = 0;
        ESP_LOGI(WIFI_CONNECTION, "connected to ap SSID:%s<---------------", EXAMPLE_ESP_WIFI_SSID);
    }
}

void wifi_connection_init_std(void)
{
    ESP_LOGI(WIFI_CONNECTION, "ESP_WIFI_MODE_STA");

    /*Create a timer handle for Wi-Fi reconnection */
    const esp_timer_create_args_t wifi_connection_timer_handle_anrgs = {
        .callback = &try_reconnect_wifi,
        .name = "one-shot-wifi-reconnect-timer"
    };

    esp_timer_create(&wifi_connection_timer_handle_anrgs, &wifi_connection_timer_handle);


    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(WIFI_CONNECTION, "Failed to create event group");
        return;
    }


    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    /* Initialize Wi-Fi driver */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configure Wi-Fi connection parameters */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };    

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &instance_any_id));
    

    
    /* Set mode, configuration, and start Wi-Fi */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(WIFI_CONNECTION, "wifi_init_sta started.");
    
}
