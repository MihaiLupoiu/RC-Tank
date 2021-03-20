#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "pubsub.h"

// Modules
#include "mod_random.h"


void app_main() {
	ESP_LOGI("MAIN", "Starting PubSub.");
	ps_init();

	ESP_LOGI("MAIN", "Starting modules:");
	mod_random_init();

	ps_msg_t *msg = CALL_INT("mod_random.cmd.get.int", 0, 1000);
	printf(">>>> got: %lld\n", msg->int_val);
	ps_unref_msg(msg);

}
