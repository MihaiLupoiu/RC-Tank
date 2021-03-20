#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pubsub.h"
#include "mod_random.h"

#define MOD_RANDOM_NAME "mod_random"
#define MOD_RANDOM_STACKSIZE 3072

typedef struct {
	uint8_t state;
	ps_subscriber_t *sub;
} mod_random_ctx_t;

static mod_random_ctx_t *ctx;

static void random_task() {
	ps_msg_t *msg = NULL;
	while (true) {
		ESP_LOGD(MOD_RANDOM_NAME, "checking msg");
		msg = ps_get(ctx->sub, 500);
		if (msg == NULL) {
			continue;
		}

		if (ps_has_topic_prefix(msg, MOD_RANDOM_NAME ".cmd.")) {
			if (ps_has_topic(msg, MOD_RANDOM_NAME ".cmd.get.int")) {
				uint32_t val = esp_random();
				ESP_LOGD(MOD_RANDOM_NAME, "request from %s for random int. Value: %u", msg->topic, val);
				PUB_INT(msg->rtopic, val);
			}
		}
		ps_unref_msg(msg);
	}
}

int mod_random_init(void) {
	ctx = calloc(1, sizeof(mod_random_ctx_t));
	ctx->state = 0;
	ctx->sub =  ps_new_subscriber(10, STRLIST(MOD_RANDOM_NAME ".cmd"));

	if (xTaskCreate(random_task, MOD_RANDOM_NAME, MOD_RANDOM_STACKSIZE * 2, NULL, configMAX_PRIORITIES, NULL) != pdPASS) {
		ESP_LOG_LEVEL(ESP_LOG_ERROR, MOD_RANDOM_NAME, "unable to start");
		return -1;
	}

	ctx->state = 1;
	ESP_LOG_LEVEL(ESP_LOG_INFO, MOD_RANDOM_NAME, "started");

	return 0;
}