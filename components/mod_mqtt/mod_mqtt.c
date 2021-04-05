#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "pubsub.h"
#include "parseutils.h"

#include "mod_mqtt.h"

#define MOD_MQTT_NAME "mod_mqtt"
#define MOD_WIFI_NAME "mod_wifi"
#define MOD_MQTT_STACKSIZE 3072

typedef struct {
	uint8_t state;
	ps_subscriber_t *sub;
    esp_mqtt_client_handle_t client;
} mod_mqtt_ctx_t;

static mod_mqtt_ctx_t *ctx;

static void mqtt_start_client(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
// static void log_error_if_nonzero(const char *message, int error_code);

// MQTT COMMANDS HEADERS
static void mqtt_cmd_pub(const ps_msg_t *msg);
static void mqtt_to_local_bridge(const char *topic, int topic_len, const void *payload, int payloadlen);
static void local_to_mqtt_bridge(const ps_msg_t *msg);

static void mqtt_task() {
	ps_msg_t *msg = NULL;
    int mqtt_msg_id;
	while (true) {
		ESP_LOGD(MOD_MQTT_NAME, "checking msg");
		msg = ps_get(ctx->sub, 500);
		if (msg == NULL) {
			continue;
		}

		ESP_LOGI(MOD_MQTT_NAME, "request from %s received.", msg->topic);

		if (ps_has_topic_prefix(msg, MOD_WIFI_NAME ".evt.")) {
			if (ps_has_topic(msg, MOD_WIFI_NAME ".evt.wifi.stat") && IS_INT(msg)) {
				if (msg->int_val == 1) { // WiFi started
					ESP_LOGI(MOD_MQTT_NAME, "Wifi Started.");
					mqtt_start_client();
				} else {
					ESP_LOGI(MOD_MQTT_NAME, "Wifi disconected.");
				}
			}
		}

		if (ps_has_topic_prefix(msg, MOD_MQTT_NAME ".cmd.")) {
			if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.mqtt.sub") && IS_STR(msg)) {
                mqtt_msg_id = esp_mqtt_client_subscribe(ctx->client, msg->str_val, 0);
            	ESP_LOGI(MOD_MQTT_NAME, "Sent subscribe successful, msg_id=%d , topic=%s", mqtt_msg_id, msg->str_val);
            }
            //if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.mqtt.sub.qos1") && IS_STR(msg)) {}
            //if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.mqtt.sub.qos2") && IS_STR(msg)) {}

			// if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.local.sub") && IS_STR(msg)) {}
			if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.mqtt.usub") && IS_STR(msg)) {
                mqtt_msg_id = esp_mqtt_client_unsubscribe(ctx->client, msg->str_val);
                ESP_LOGI(MOD_MQTT_NAME, "Sent unsubscribe successful, msg_id=%d , topic=%s", mqtt_msg_id, msg->str_val);
            }
			// if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.local.usub") && IS_STR(msg)) {}

			if (ps_has_topic_prefix(msg, MOD_MQTT_NAME ".cmd.pub.")){
                mqtt_cmd_pub(msg);
            }
			// if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.disconnect")) {}
			// if (ps_has_topic(msg, MOD_MQTT_NAME ".cmd.connect")) {}

		}
		ps_unref_msg(msg);
	}
}

int mod_mqtt_init(void) {
	ctx = calloc(1, sizeof(mod_mqtt_ctx_t));
	ctx->state = 0;
	ctx->sub = ps_new_subscriber(10, STRLIST(MOD_MQTT_NAME ".cmd", MOD_WIFI_NAME ".evt.wifi.stat"));

	if (xTaskCreate(mqtt_task, MOD_MQTT_NAME, MOD_MQTT_STACKSIZE * 2, NULL, configMAX_PRIORITIES, NULL) != pdPASS) {
		ESP_LOG_LEVEL(ESP_LOG_ERROR, MOD_MQTT_NAME, "unable to start");
		return -1;
	}

	ctx->state = 1;
	ESP_LOG_LEVEL(ESP_LOG_INFO, MOD_MQTT_NAME, "started");

	return 0;
}

static void mqtt_start_client(void) {
	esp_mqtt_client_config_t mqtt_cfg = {
	.uri = CONFIG_BROKER_URL,
	};
	ctx->client = esp_mqtt_client_init(&mqtt_cfg);
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	esp_mqtt_client_register_event(ctx->client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(ctx->client);
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	ESP_LOGD(MOD_MQTT_NAME, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	esp_mqtt_event_handle_t event = event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	switch ((esp_mqtt_event_id_t) event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_CONNECTED");
        PUB_INT_FL(MOD_MQTT_NAME ".evt.mqtt.stat", 1, FL_STICKY);

		msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
		ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);

		// msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
		// ESP_LOGI(MOD_MQTT_NAME, "sent subscribe successful, msg_id=%d", msg_id);
        PUB_STR(MOD_MQTT_NAME ".cmd.mqtt.sub", "/topic/qos0");

		// msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
		// ESP_LOGI(MOD_MQTT_NAME, "sent subscribe successful, msg_id=%d", msg_id);
        PUB_STR(MOD_MQTT_NAME ".cmd.mqtt.sub", "/topic/qos1");

		// msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
		// ESP_LOGI(MOD_MQTT_NAME, "sent unsubscribe successful, msg_id=%d", msg_id);
        PUB_STR(MOD_MQTT_NAME ".cmd.mqtt.usub", "/topic/qos1");

		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_DISCONNECTED");
        PUB_INT_FL(MOD_MQTT_NAME ".evt.mqtt.stat", 0, FL_STICKY);
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

		// msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
		// ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);
        PUB_STR(MOD_MQTT_NAME ".cmd.pub./topic/qos0", "data");

		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_DATA");
		printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		printf("DATA=%.*s\r\n", event->data_len, event->data);

        mqtt_to_local_bridge(event->topic,event->topic_len, event->data, event->data_len);

		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(MOD_MQTT_NAME, "MQTT_EVENT_ERROR");

		/*
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
		    log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
		    log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
		    log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
		    ESP_LOGI(MOD_MQTT_NAME, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

		}
		*/
		break;
	default:
		ESP_LOGI(MOD_MQTT_NAME, "Other event id:%d", event->event_id);
		break;
	}
}

/*
static void log_error_if_nonzero(const char *message, int error_code){
    if (error_code != 0) {
        ESP_LOGE(MOD_MQTT_NAME, "Last error %s: 0x%x", message, error_code);
    }
}

*/




static void mqtt_cmd_pub(const ps_msg_t *msg) {
    // MOD_MQTT_NAME.cmd.pub.sys/cmd/asdf:msg_data

	str_list *sl = str_split(msg->topic, ".");
	if (str_list_len(sl) != 4) {
        if( msg->rtopic != NULL){
            ESP_LOGE(MOD_MQTT_NAME, "Error. Invalid mqtt topic. Message cannot contain \".\"");
            PUB_STR_FL(msg->rtopic, "Error. Invalid mqtt topic. Message cannot contain \".\"",ERR_TYP); // Response
        }
		return;
	}

	char mqtt_topic[256];
	char *ptr = sl->next->next->next->s;
	if (strlen(ptr) > 255) {
        ESP_LOGE(MOD_MQTT_NAME, "Error. Invalid mqtt topic too long.");
        PUB_STR_FL(msg->rtopic, "Error. Invalid mqtt topic too long.",ERR_TYP); // Response
		return;
	}

	strncpy(mqtt_topic, ptr, sizeof(mqtt_topic));
	str_list_free(sl);

    ESP_LOGI(MOD_MQTT_NAME, "Topic: %s",mqtt_topic);


	if (!IS_STR(msg)) {
        ESP_LOGE(MOD_MQTT_NAME, "Error. Message not of type sting.");
        PUB_STR_FL(msg->rtopic, "Error. Message not of type sting.",ERR_TYP); // Response
		return;
	}

    int msg_id = esp_mqtt_client_publish(ctx->client, mqtt_topic, msg->str_val, strlen(msg->str_val), 0, 0);
	if ( msg_id == -1) {
        ESP_LOGE(MOD_MQTT_NAME, "Error. Could not publish to mqtt server");
        PUB_STR_FL(msg->rtopic, "Error. Could not publish to mqtt server",ERR_TYP); // Response
		return;
	}

    ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);
    PUB_STR(msg->rtopic, "ok"); // Response
}

static void mqtt_to_local_bridge(const char *topic, int topic_len, const void *payload, int payloadlen){
    // MOD_MQTT_NAME.cmd.mqtt.sub:test/

    char *local_topic = NULL;
	char *tmp_topic = NULL;

	tmp_topic = strndup(topic, topic_len);
	local_topic = str_repl(tmp_topic, "/", ".");
	free(tmp_topic);
    tmp_topic = NULL;
    // ESP_LOGI(MOD_MQTT_NAME, "Message MQTT Topic: %s\n local Topic: %s \n", topic, local_topic);

	char *local_payload = NULL;
	local_payload = strndup(payload, payloadlen);
    PUB_STR(local_topic, local_payload);
}


static void local_to_mqtt_bridge(const ps_msg_t *msg){
    int msg_id = 0;
	char *buf = NULL;
	char *mqtt_topic = NULL;
	
	mqtt_topic = str_repl(msg->topic, ".", "/");

	// char *err = msg->flags & ERR_TYP ? "error" : "";
    // ESP_LOGI(MOD_MQTT_NAME, "Message Topic: %s\n MQTT Topic: %s \n", msg->topic, mqtt_topic);

    if (IS_STR(msg)) {
        ESP_LOGI(MOD_MQTT_NAME, "Message String: %s\n", msg->str_val);
        msg_id = esp_mqtt_client_publish(ctx->client, mqtt_topic, msg->str_val, strlen(msg->str_val), 0, 0);
		ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);
    } else if (IS_INT(msg)) {
		asprintf(&buf, "%lld", msg->int_val);
		ESP_LOGI(MOD_MQTT_NAME, "Message int: %s\n", buf);
        msg_id = esp_mqtt_client_publish(ctx->client, mqtt_topic, (void *) buf, strlen(buf), 0, 0);
		ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);
	} else if (IS_DBL(msg)) {
		asprintf(&buf, "%f", msg->dbl_val);
		ESP_LOGI(MOD_MQTT_NAME, "Message int: %s\n", buf);
        msg_id = esp_mqtt_client_publish(ctx->client, mqtt_topic, (void *) buf, strlen(buf), 0, 0);
		ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);
    } else if (IS_BUF(msg)) {
        ESP_LOGI(MOD_MQTT_NAME, "Message buffer size: %d\n", msg->buf_val.sz);
        msg_id = esp_mqtt_client_publish(ctx->client, mqtt_topic, (void *) msg->buf_val.ptr, msg->buf_val.sz, 0, 0);
		ESP_LOGI(MOD_MQTT_NAME, "sent publish successful, msg_id=%d", msg_id);
    } else{
        ESP_LOGI(MOD_MQTT_NAME, "Unrecognized Message Type\n");
    }

    free(buf);
    buf = NULL;

	if (msg_id != 0) {
        PUB_STR_FL(msg->rtopic, "Error. Could not publish to mqtt server.",ERR_TYP); // Response
		return;
	}

    PUB_STR(msg->rtopic, "ok"); // Response
}