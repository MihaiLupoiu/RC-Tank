#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "pubsub.h"

// Modules
#include "mod_random.h"
#include "mod_motor_drv8833.h"

void app_main() {
	ESP_LOGI("MAIN", "Starting PubSub.");
	ps_init();

	ESP_LOGI("MAIN", "Starting modules:");
	mod_random_init();
	mod_motor_drv8833_init();

	ps_msg_t *msg = CALL_INT("mod_random.cmd.get.int", 0, 1000);
	printf(">>>> got: %lld\n", msg->int_val);
	ps_unref_msg(msg);

	for (size_t i = 0; i < 10000; i+=10)
	{
		printf(">>>> sending: %d\n", i%100);
		PUB_INT("mod_motor_drv8833.cmd.set.xChassis", i%100);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	

}
