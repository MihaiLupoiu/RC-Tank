#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "pubsub.h"

void app_main() {
	ESP_LOGI("MAIN", "Starting PubSub.");
	ps_init();
	ESP_LOGI("MAIN", "Starting modules:");
	// mod_random_init();
}
