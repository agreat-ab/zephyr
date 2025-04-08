#include "shim.h"
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/lib/iso14229/uds.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#define SLEEP_TIME_MS 1000
#define LED0_NODE     DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main()
{
	uint8_t ret = 0;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	bool led_state = true;

	for (;;) {
		k_sleep(K_MSEC(1000));
		led_state = !led_state;
	}
}
