#include <stdint.h>

#include <stdbool.h>
#include <zephyr/lib/iso14229/iso14229.h>
#include <zephyr/lib/iso14229/uds.h>
#include <zephyr/kernel.h>
#include <zephyr/version.h>
#include <zephyr/toolchain/common.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

UDSServer_t srv;
UDSISOTpC_t tp;

LOG_MODULE_REGISTER(uds, CONFIG_LOG_DEFAULT_LEVEL);

static const UDSISOTpCConfig_t tp_cfg = {
	.source_addr = CONFIG_ISO14229_TX_ID,
	.target_addr = CONFIG_ISO14229_RX_ID,
	.source_addr_func = CONFIG_ISO14229_FUNC_RX_ID,
	.target_addr_func = UDS_TP_NOOP_ADDR,
};

const struct device *can_dev;

static struct k_timer uds_timer;

K_MSGQ_DEFINE(can_msgq, sizeof(struct can_frame), CONFIG_ISO14229_CAN_MSGQ_SIZE, 4);

struct can_frame_work_info {
	struct k_work uds_work;
	struct can_frame frame;
} can_frame_work;

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
			void *user_data)
{
	// TODO: Add some parameter validation here
	struct can_frame frame = {
		.id = arbitration_id,
		.flags = 0x0,
		.dlc = size,
	};
	memcpy(frame.data, data, size);

	uint32_t ret = can_send(can_dev, &frame, K_MSEC(100), NULL, NULL);

	if (ret < 0) {
		LOG_ERR("Failed to send CAN frame (%d)\n", ret);
		return -1;
	} else {
		return size;
	}
}

void isotp_user_debug(const char *msg, ...)
{
	LOG_INF("%s", msg);
}

uint32_t UDSMillis(void)
{
	// Return milliseconds from system, assumed relative time is sufficient
	return k_uptime_get_32();
}

uint32_t isotp_user_get_us()
{
	return k_uptime_get_32();
}

static struct k_work_delayable reboot_work;

static void do_reboot(struct k_work *work)
{
	sys_reboot(SYS_REBOOT_COLD);
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg)
{
	switch (ev) {
	case UDS_EVT_EcuReset:
		LOG_INF("Scheduling reboot, from UDS_EVT_EcuReset\n");
		k_work_schedule(&reboot_work, K_MSEC(1000));
		break;
	default:
		LOG_ERR("Unhandled event %s (%d)\n", UDSEventToStr(ev), ev);
		return UDS_NRC_ServiceNotSupported;
	}
	return UDS_PositiveResponse;
}

void uds_handle_frame(struct can_frame *frame)
{
	LOG_INF("Handling work for UDS");

	if (frame->id == tp.phys_sa) {
		LOG_INF("Received phys can id 0x%03x\n", frame->id);
		isotp_on_can_message(&tp.phys_link, frame->data, frame->dlc);
	} else if (frame->id == tp.func_sa) {
		LOG_INF("Received func can id 0x%03x\n", frame->id);
		if (ISOTP_RECEIVE_STATUS_IDLE != tp.phys_link.receive_status) {
			LOG_ERR("Func frame received but cannot process because link is "
				"not "
				"idle\n");
		}
		isotp_on_can_message(&tp.func_link, frame->data, frame->dlc);
	} else {
		LOG_ERR("Received unknown can id 0x%03x\n", frame->id);
	}
}

void uds_poll_work_handler(struct k_work *frame_work)
{
	struct can_frame_work_info *info =
		CONTAINER_OF(frame_work, struct can_frame_work_info, uds_work);
	uds_handle_frame(&info->frame);
}

void can_queue_uds_work(const struct device *dev, struct can_frame *frame, void *user_data)
{
	uint8_t ret = 0;

	memcpy(&can_frame_work.frame, frame, sizeof(struct can_frame));

	ret = k_work_submit(&can_frame_work.uds_work);
	if (ret == 1) {
		LOG_INF("First submission to queue\n");
	} else if (ret == 0) {
		LOG_INF("Work already on queue\n");
	} else if (ret == 2) {
		LOG_INF("Work already running on queue\n");
	} else {
		LOG_ERR("Failed to queue\n");
	}
}

void uds_timer_function(struct k_timer *dummy)
{
	UDSServerPoll(&srv);
}

static int uds_init()
{
	uint8_t ret = 0;

	k_work_init_delayable(&reboot_work, do_reboot);

	LOG_INF("Initializing CAN UDS module\n");

	can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
	if (!device_is_ready(can_dev)) {
		LOG_INF("CAN: Device driver not ready.\n");
		return 0;
	}

	ret = can_stop(can_dev);
	if (ret != 0) {
		LOG_INF("CAN controller already stopped\n");
	}

	struct can_timing timing;
	ret = can_calc_timing(
		can_dev, &timing, 500000,
		875); // Sampling point relates to tradeoff between bandwidth and bus length
	if (ret > 0) {
		LOG_ERR("Sample-Point error: %d\n", ret);
	}

	if (ret < 0) {
		LOG_ERR("Failed to calc a valid timing\n");
		return 0;
	}

	ret = can_set_timing(can_dev, &timing);
	if (ret != 0) {
		LOG_ERR("Failed to set timing\n");
	}

	k_work_init(&can_frame_work.uds_work, uds_poll_work_handler);

	struct can_filter filter;

	filter.id = 0;
	filter.mask = 0; // TODO: Filter the messages here
	filter.flags = 0;

	ret = can_add_rx_filter(can_dev, &can_queue_uds_work, NULL, &filter);
	if (ret < 0) {
		LOG_ERR("Failed to add callback filter %d\n", ret);
	}

	// ret = can_add_rx_filter_msgq(can_dev, &can_msgq, &filter);
	// if (ret < 0) {
	// LOG_ERR("Failed to add msgq filter %d\n", ret);
	// }

	ret = can_start(can_dev);
	if (ret != 0) {
		LOG_ERR("Failed to start CAN controller\n");
		return 0;
	} else {
		LOG_INF("CAN controller configuration sucessful!\n");
	}

	UDSErr_t uds_ret;
	uds_ret = UDSServerInit(&srv);
	if (uds_ret != 0) {
		LOG_ERR("Failed to initialize UDS server\n");
		return 0;
	}
	uds_ret = UDSISOTpCInit(&tp, &tp_cfg);
	if (uds_ret != 0) {
		LOG_ERR("Failed to initialize UDS ISO-TP client\n");
		return 0;
	}

	srv.fn = fn;
	srv.tp = &tp.hdl;

	k_timer_init(&uds_timer, uds_timer_function, NULL);

	k_timer_start(&uds_timer, K_MSEC(1000), K_MSEC(100));

	return 0;
}

#define UDS_INIT_PRIO 32
SYS_INIT(uds_init, APPLICATION, UDS_INIT_PRIO);
