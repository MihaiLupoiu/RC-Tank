#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pubsub.h"
#include "parseutils.h"
#include "mod_mobile_controller.h"

#define MOD_MOBILE_CONTROLLER_NAME "mod_mobile_controller"
#define MOD_MOBILE_CONTROLLER_STACKSIZE 3072

#define MOD_MQTT_NAME "mod_mqtt"

#define MOBILE_CONTROLLER_MQTT_TOPIC	CONFIG_ESP_MOBILE_CONTROLLER_MQTT_TOPIC

typedef struct {
	uint8_t state;
	ps_subscriber_t *sub;
} mod_mobile_controller_ctx_t;

static mod_mobile_controller_ctx_t *ctx;

// Headers
void mobile_controller_start_client();
void mobile_controller_process(ps_msg_t *msg);

// Internal helper funtions
static int process_speed(char *raw_string);
static void stop_motors();

static void mobile_controller_task() {
	ps_msg_t *msg = NULL;
	while (true) {
		ESP_LOGD(MOD_MOBILE_CONTROLLER_NAME, "checking msg");
		msg = ps_get(ctx->sub, 500);
		if (msg == NULL) {
			continue;
		}

		ESP_LOGD(MOD_MOBILE_CONTROLLER_NAME, "request from %s received.", msg->topic);

		if (ps_has_topic_prefix(msg, MOD_MQTT_NAME ".evt.")) {
			if (ps_has_topic(msg, MOD_MQTT_NAME ".evt.mqtt.stat") && IS_INT(msg)) {
				if (msg->int_val == 1) { // MQTT started
					ESP_LOGI(MOD_MOBILE_CONTROLLER_NAME, "MQTT Started.");
					mobile_controller_start_client();
				} else {
					ESP_LOGI(MOD_MOBILE_CONTROLLER_NAME, "MQTT disconected.");
					stop_motors();
				}
			}
		}

		if (ps_has_topic_prefix(msg, MOD_MOBILE_CONTROLLER_NAME ".cmd.")) {
			if (ps_has_topic(msg, MOD_MOBILE_CONTROLLER_NAME ".cmd.get.int")) {
				uint32_t val = esp_random();
				ESP_LOGD(MOD_MOBILE_CONTROLLER_NAME, "request from %s for mobile_controller int. Value: %u", msg->topic,
				         val);
				PUB_INT(msg->rtopic, val);
			}
		}

		if (ps_has_topic_prefix(msg, MOBILE_CONTROLLER_MQTT_TOPIC ".") && IS_STR(msg)) {
			mobile_controller_process(msg);
		}

		ps_unref_msg(msg);
	}
}

int mod_mobile_controller_init(void) {
	ctx = calloc(1, sizeof(mod_mobile_controller_ctx_t));
	ctx->state = 0;
	ctx->sub = ps_new_subscriber(10, STRLIST(MOD_MOBILE_CONTROLLER_NAME ".cmd", MOD_MQTT_NAME ".evt", "tank.1"));

	if (xTaskCreate(mobile_controller_task, MOD_MOBILE_CONTROLLER_NAME, MOD_MOBILE_CONTROLLER_STACKSIZE * 2, NULL,
	                configMAX_PRIORITIES, NULL) != pdPASS) {
		ESP_LOG_LEVEL(ESP_LOG_ERROR, MOD_MOBILE_CONTROLLER_NAME, "unable to start");
		return -1;
	}

	ctx->state = 1;
	ESP_LOG_LEVEL(ESP_LOG_INFO, MOD_MOBILE_CONTROLLER_NAME, "started");

	return 0;
}

void mobile_controller_start_client() {
	PUB_STR(MOD_MQTT_NAME ".cmd.mqtt.sub", "tank/1/#");
}

void mobile_controller_process(ps_msg_t *msg) {
	if (ps_has_topic(msg, "tank.1.right")) {
		int speed = process_speed(msg->str_val);
		ESP_LOGD(MOD_MOBILE_CONTROLLER_NAME, "request from %s for mobile_controller string. Value: %s -> speed: %d",
		         msg->topic, msg->str_val, speed);

		PUB_INT("mod_motor_drv8833.cmd.set.xChassis.B", process_speed(msg->str_val));

	} else if (ps_has_topic(msg, "tank.1.left")) {
		int speed = process_speed(msg->str_val);
		ESP_LOGD(MOD_MOBILE_CONTROLLER_NAME, "request from %s for mobile_controller string. Value: %s-> speed: %d",
		         msg->topic, msg->str_val, speed);
		PUB_INT("mod_motor_drv8833.cmd.set.xChassis.A", process_speed(msg->str_val));
	}
}

static int process_speed(char *raw_string) {
	str_list *sl = str_split(raw_string, " ");
	if (str_list_len(sl) != 2) {
		ESP_LOGE(MOD_MOBILE_CONTROLLER_NAME, "Error. Invalid mqtt message.");
		return 0;
	}

	char *angle_prt = sl->s;
	char *power_prt = sl->next->s;
	if (strlen(angle_prt) > 255 || strlen(power_prt) > 255) {
		ESP_LOGE(MOD_MQTT_NAME, "Error. Invalid mqtt message too long.");
		return 0;
	}
	int angle = atoi(angle_prt);
	int power = atoi(power_prt);
	str_list_free(sl);

	if (angle > 180) {
		return (power * -1);
	}
	return power;
}

static void stop_motors(){
	PUB_INT("mod_motor_drv8833.cmd.set.xChassis.A", 0);
	PUB_INT("mod_motor_drv8833.cmd.set.xChassis.B", 0);
}