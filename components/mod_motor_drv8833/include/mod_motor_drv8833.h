#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm.h"

#define MOD_MOTOR_NAME "mod_motor_drv8833"
#define MOD_MOTOR_STACKSIZE 3072

typedef struct {
	uint8_t state;
	ps_subscriber_t *sub;
} mod_motor_drv8833_ctx_t;

static const gpio_num_t drv8833_ain1 = GPIO_NUM_23;
static const gpio_num_t drv8833_ain2 = GPIO_NUM_19;
static const gpio_num_t drv8833_bin1 = GPIO_NUM_18;
static const gpio_num_t drv8833_bin2 = GPIO_NUM_5;

// MCPWM peripheral components to use
static const mcpwm_unit_t drv8833_mcpwm_unit = MCPWM_UNIT_0;
static const uint32_t drv8833_mcpwm_freq = 50000;

// Duty cycle limit: restrict duty cycle to be no greater than this value.
// Necessary when the power supply exceeds voltage rating of motor.
// TT gearmotors are typically quoted for 3-6V operation.
// Fully charged 2S LiPo is 8.4V. 6/8.4 ~= 72%.
// Feeling adventurous? Nominal 2S LiPo is 7.4V. 7.4/8.4 ~= 88%
static const uint32_t duty_cycle_max = 88;

typedef struct xChassisMessage
{
  TickType_t  timeStamp;
  int32_t     iMotorA;
  int32_t     iMotorB;
  bool        bBrake;
} Chassis_t;

int mod_motor_drv8833_init(void);
