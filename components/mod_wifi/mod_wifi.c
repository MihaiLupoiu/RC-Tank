/*  WiFi softAP Example
   
   	This example code is in the Public Domain (or CC0 licensed, at your option.)
   
   	Unless required by applicable law or agreed to in writing, this
   	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   	CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "pubsub.h"
#include "mod_wifi.h"

#define MOD_WIFI_NAME "mod_wifi"
#define MOD_WIFI_STACKSIZE 3072

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* The examples use WiFi configuration that you can set via project configuration menu.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      					CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      					CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_MAX_RECONNECTION_TIME   	CONFIG_ESP_WIFI_MAX_RECONNECTION_TIME

typedef struct {
	uint8_t state;
	ps_subscriber_t *sub;
	/* WiFi status */
	uint8_t wifi_state;
	/* Counter to reconnect WiFi */
	uint8_t reconnect_counter;
	/* WiFi event group */
	EventGroupHandle_t wifi_event_group;
} mod_wifi_ctx_t;

static mod_wifi_ctx_t *ctx;

void wifi_init();

static void wifi_task() {
	ps_msg_t *msg = NULL;

	//Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	wifi_init();

	while (true) {
		ESP_LOGD(MOD_WIFI_NAME, "checking msg");
		msg = ps_get(ctx->sub, 500);
		if (msg == NULL) {
			continue;
		}

		if (ps_has_topic_prefix(msg, MOD_WIFI_NAME ".cmd.")) {
			if (ps_has_topic(msg, MOD_WIFI_NAME ".cmd.get.stat")) {
				uint32_t val = esp_random();
				ESP_LOGD(MOD_WIFI_NAME, "request from %s for random int. Value: %u", msg->topic, val);
				PUB_INT(msg->rtopic, val);
			}
		}
		ps_unref_msg(msg);
	}
}

int mod_wifi_init(void) {
	ctx = calloc(1, sizeof(mod_wifi_ctx_t));
	ctx->state = 0;
	ctx->sub =  ps_new_subscriber(10, STRLIST(MOD_WIFI_NAME ".cmd"));

	if (xTaskCreate(wifi_task, MOD_WIFI_NAME, MOD_WIFI_STACKSIZE * 2, NULL, configMAX_PRIORITIES, NULL) != pdPASS) {
		ESP_LOG_LEVEL(ESP_LOG_ERROR, MOD_WIFI_NAME, "unable to start");
		return -1;
	}

	ctx->state = 1;
	ESP_LOG_LEVEL(ESP_LOG_INFO, MOD_WIFI_NAME, "started");

	return 0;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (ctx->reconnect_counter < EXAMPLE_ESP_WIFI_MAX_RECONNECTION_TIME) {
            esp_wifi_connect();
            ctx->reconnect_counter++;
            ESP_LOGI(MOD_WIFI_NAME, "retry to connect to the AP");
			ctx->wifi_state = 0;
        } else {
            xEventGroupSetBits(ctx->wifi_event_group, WIFI_FAIL_BIT);
			ctx->wifi_state = 0;
        }
        ESP_LOGI(MOD_WIFI_NAME,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(MOD_WIFI_NAME, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ctx->reconnect_counter = 0;
        xEventGroupSetBits(ctx->wifi_event_group, WIFI_CONNECTED_BIT);
		ctx->wifi_state = 1;
    }
    PUB_INT_FL(MOD_WIFI_NAME ".evt.wifi.stat", ctx->wifi_state, FL_STICKY);
}


/**
 * @brief Initialize WiFi module, set station mode and start WiFi.
 *
 */
void wifi_init() {
	ctx->wifi_state = 0;
	ESP_LOGD(MOD_WIFI_NAME, "Initializing WiFi module in ESP_WIFI_MODE_STA");
	/* Create de EventGroup to control the WiFi events */
	ctx->wifi_event_group = xEventGroupCreate();

	/* Initialize tcp/ip */
	ESP_ERROR_CHECK(esp_netif_init());

	/* Initialize the event loop to collect the events created by the WiFi */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(MOD_WIFI_NAME, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(ctx->wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(MOD_WIFI_NAME, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(MOD_WIFI_NAME, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(MOD_WIFI_NAME, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(ctx->wifi_event_group);
}