#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "pubsub.h"

// Modules
#include "mod_random.h"
#include "mod_motor_drv8833.h"
#include "mod_wifi.h"
#include "mod_mqtt.h"

static const char *TAG = "MAIN";


void app_main() {

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

/*
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
*/

	ESP_LOGI(TAG, "Starting PubSub.");
	ps_init();

	ESP_LOGI(TAG, "Starting modules:");
	mod_random_init();
	mod_wifi_init();
	mod_mqtt_init();
	mod_motor_drv8833_init();

	ps_msg_t *msg = CALL_INT("mod_random.cmd.get.int", 0, 1000);
	printf(">>>> got: %lld\n", msg->int_val);
	ps_unref_msg(msg);

/* 
	for (size_t i = 0; i < 10000; i+=10)
	{
		printf(">>>> sending: %d\n", i%100);
		PUB_INT("mod_motor_drv8833.cmd.set.xChassis", i%100);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
*/

}
