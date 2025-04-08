#include "shim.h"
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/lib/iso14229/iso14229.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>

UDSServer_t srv;
UDSISOTpC_t tp;

static const UDSISOTpCConfig_t tp_cfg = {
    .source_addr=0x7E8,
    .target_addr=0x7E0,
    .source_addr_func=0x7DF,
    .target_addr_func=UDS_TP_NOOP_ADDR,
};

#define SLEEP_TIME_MS   1000
#define LED0_NODE DT_ALIAS(led0)

K_MSGQ_DEFINE(can_msgq, sizeof(struct can_frame), 10, 4);
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data,
                               const uint8_t size, void *user_data) {
    printf("Trying to send CAN frame from isotp layer (id: 0x%03lx)\n", arbitration_id);
    // if (0 != BSPSendCAN(arbitration_id, data, size)) {
        // return ISOTP_RET_ERROR;
        // } else {
        // return ISOTP_RET_OK;
        // }
}

void isotp_user_debug(const char *msg, ...) {
    printf("%s", msg);
}

static void isotp_debug(const char *msg, ...) {
    printf("%s", msg);
}

uint32_t UDSMillis(void) {
    // Return milliseconds from system, assumed relative time is sufficient
    return k_uptime_get_32();
}

uint32_t isotp_user_get_us() {
  return k_uptime_get_32();
}

static uint8_t fn(UDSServer_t *srv, UDSEvent_t ev, const void *arg) {
    return UDS_PositiveResponse;
}


int main() {
  struct can_timing timing;
  const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
  if (!device_is_ready(can_dev)) {
    printf("CAN: Device driver not ready.\n");
    return 0;
  }
  int ret;

  printf("Initializing CAN UDS sample\n");

  ret = can_calc_timing(can_dev, &timing, 500000, 875); // Sampling point relates to tradeoff between bandwidth and bus length
  if (ret > 0) {
    printf("Sample-Point error: %d\n", ret);
  }

  if (ret < 0) {
    printf("Failed to calc a valid timing\n");
    return;
  }

  ret = can_stop(can_dev);
  if (ret != 0) {
    printf("Failed to stop CAN controller\n");
  }

  ret = can_set_timing(can_dev, &timing);
  if (ret != 0) {
    printf("Failed to set timing\n");
  }

  struct can_filter filter;
  struct can_frame frame;

  filter.id = 0;
  filter.mask = 0; // TODO: Filter the messages here
  filter.flags = 0;

  ret = can_add_rx_filter_msgq(can_dev, &can_msgq, &filter);
  if (ret < 0) {
    printf("Failed to add filter %d\n", ret);
  }

  ret = can_start(can_dev);
  if (ret != 0) {
    printf("Failed to start CAN controller\n");
  } else {
    printf("CAN controller configuration sucessful!\n");
  }

	bool led_state = true;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

  UDSErr_t uds_ret;
  uds_ret = UDSServerInit(&srv);
  if (uds_ret != 0) {
    printf("Failed to initialize UDS server\n");
    return 0;
  }
  uds_ret = UDSISOTpCInit(&tp, &tp_cfg);
  if (uds_ret != 0) {
    printf("Failed to initialize UDS ISO-TP client\n");
    return 0;
  }
  srv.fn = fn;
  srv.tp = &tp.hdl;

  uint8_t my_data = 0x88;
  struct can_frame hb_frame = {
      .id = 0x123,
      .flags = 0x0,
      .dlc = 8,
  };
  memcpy(hb_frame.data, my_data, 8);

  for (;;) {
    if (k_msgq_get(&can_msgq, &frame, K_MSEC(100)) == 0) {
      if (frame.id == tp.phys_sa) {
        printf("Received phys can id 0x%03lx\n", frame.id);
        isotp_on_can_message(&tp.phys_link, frame.data, frame.dlc);
      } else if (frame.id == tp.func_sa) {
        printf("Received func can id 0x%03lx\n", frame.id);
        if (ISOTP_RECEIVE_STATUS_IDLE != tp.phys_link.receive_status) {
          printf("Func frame received but cannot process because link is not idle\n");
          continue;
        }
        isotp_on_can_message(&tp.func_link, frame.data, frame.dlc);
      } else {
        printf("Received unknown can id 0x%03lx\n", frame.id);
      }
    }

    UDSServerPoll(&srv);
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		// printf("LED state: %s\n", led_state ? "ON" : "OFF");
		// k_msleep(SLEEP_TIME_MS);
		// can_send(can_dev, &hb_frame, K_MSEC(100), NULL, NULL); // Heartbeat for debugging
  }

}
