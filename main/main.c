
#include "esp_log.h"
#include "nvs_flash.h"


#include "wifi_connection.h"


static const char *TAG = "app_main";

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
        /* Optionally increase Wi-Fi log level for debugging */
    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG, "Starting Wi-Fi station initialization...");

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    

    wifi_connection_init_std();

}
