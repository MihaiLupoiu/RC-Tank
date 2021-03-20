#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm.h"

#include "pubsub.h"
#include "mod_motor_drv8833.h"

static mod_motor_drv8833_ctx_t *ctx;

static void motor_drv8833_task(void *arg);

int mod_motor_drv8833_init(void){
    ctx = calloc(1, sizeof(mod_motor_drv8833_ctx_t));
	ctx->state = 0;
	ctx->sub =  ps_new_subscriber(10, STRLIST( MOD_MOTOR_NAME".cmd"));

	if (xTaskCreate(motor_drv8833_task, MOD_MOTOR_NAME, MOD_MOTOR_STACKSIZE * 2, NULL, configMAX_PRIORITIES, NULL) != pdPASS) {
		ESP_LOG_LEVEL(ESP_LOG_ERROR, MOD_MOTOR_NAME, "unable to start");
		return -1;
	}

	ctx->state = 1;
	ESP_LOG_LEVEL(ESP_LOG_INFO, MOD_MOTOR_NAME, "started");

	return 0;
}

static void mcpwm_example_gpio_initialize(void)
{
    printf("initializing mcpwm gpio...\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, drv8833_ain1);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, drv8833_ain2);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, drv8833_bin1);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, drv8833_bin2);
}

void set_channel(bool bBrake, int32_t iSpeed, mcpwm_timer_t timer)
{
    printf("speed %d\n", iSpeed);

    if (bBrake)
    {
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_A, 100);
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_B, 100);
    }
    else if (iSpeed > 0)
    {
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_A, abs(iSpeed) * duty_cycle_max / 100);
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_B, 0);
    }
    else if (iSpeed < 0)
    {
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_A, 0);
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_B, abs(iSpeed) * duty_cycle_max / 100);
    }
    else
    {
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_A, 0);
        mcpwm_set_duty(drv8833_mcpwm_unit, timer, MCPWM_GEN_B, 0);
    }
}

/**
 * @brief Configure MCPWM module for brushed dc motor
 */
static void motor_drv8833_task(void *arg)
{
    //1. mcpwm gpio initialization
    mcpwm_example_gpio_initialize();

    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm...\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = drv8833_mcpwm_freq; // 1000;    //frequency = 1000Hz,
    pwm_config.cmpr_a = 0;                     //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;                     //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    mcpwm_init(drv8833_mcpwm_unit, MCPWM_TIMER_0, &pwm_config); //Configure PWM0A & PWM0B with above settings
    mcpwm_init(drv8833_mcpwm_unit, MCPWM_TIMER_1, &pwm_config); //Configure PWM1A & PWM1B with above settings

    ps_msg_t *msg = NULL;

    while (true) {
		msg = ps_get(ctx->sub, 100);
		if (msg == NULL) {
			continue;
		}

		ESP_LOGI(MOD_MOTOR_NAME, "request from %s ", msg->topic);

		if (ps_has_topic_prefix(msg, MOD_MOTOR_NAME ".cmd.")) {
			if (ps_has_topic(msg, MOD_MOTOR_NAME ".cmd.set.xChassis") && IS_INT(msg)) {
				ESP_LOGI(MOD_MOTOR_NAME, "request from %s Value: %lld", msg->topic, msg->int_val);
                // set commands
                // set_channel(xMotorCommands.bBrake, xMotorCommands.iMotorA, MCPWM_TIMER_0);
                // set_channel(xMotorCommands.bBrake, xMotorCommands.iMotorB, MCPWM_TIMER_1);
    
                // testing
                set_channel(false, msg->int_val, MCPWM_TIMER_0);
                set_channel(false, msg->int_val, MCPWM_TIMER_1);
                // vTaskDelay(5000 / portTICK_RATE_MS);

			}
		}
		ps_unref_msg(msg);
	}
}