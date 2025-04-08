#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/lib/iso14229/uds.h>

#define SLEEP_TIME_MS 1000
#define LED0_NODE     DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static struct k_work_delayable reboot_work;

static void do_reboot(struct k_work *work)
{
	sys_reboot(SYS_REBOOT_COLD);
}

UDSErr_t ecu_reset_handler(UDSServer_t *srv, UDSECUResetArgs_t *args, void *dummy)
{
	k_work_schedule(&reboot_work, K_MSEC(1000));
	return UDS_PositiveResponse;
}

int main(void)
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

	k_work_init_delayable(&reboot_work, do_reboot);

	ret = uds_register_ecureset_handler(UDS_EVT_EcuReset, ecu_reset_handler, NULL);
	if (ret < 0) {
		return 0;
	}

	for (;;) {
		k_sleep(K_MSEC(1000));
		led_state = !led_state;
	}
}
