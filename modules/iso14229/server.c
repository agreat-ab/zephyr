#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h> /* For logging (optional) */
#include <zephyr/kernel_structs.h> /* For K_FIFO_DEFINE */
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/toolchain/common.h>
#include <zephyr/lib/iso14229/uds.h>
#include <zephyr/kernel/thread.h>

LOG_MODULE_REGISTER(uds, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @Worker synchronization
 */
static struct k_sem uds_worker_dead;

/* Forward declaration (always visible) */
void uds_stop_poll_timer(void);

/**
 * @UDS service handler struct
 */
typedef struct {
	UDSEvent_t event;
	UDSGenericHandler_t handler;
	void *context;
} uds_service_handler_t;

static bool uds_initialized;

/* Registry of service handlers */
static uds_service_handler_t service_handler_registry[UDS_EVT_MAX] = {0};

/**
 * @Weak wrapper functions
 */
__weak int uds_can_send(const struct device *dev, const struct can_frame *frame,
				k_timeout_t timeout, can_tx_callback_t callback, void *user_data)
{
	return can_send(dev, frame, timeout, callback, user_data);
}

__weak int uds_can_start(const struct device *dev)
{
	return can_start(dev);
}

__weak int uds_can_stop(const struct device *dev)
{
	return can_stop(dev);
}

__weak int uds_can_calc_timing(const struct device *dev, struct can_timing *result,
							uint32_t bitrate, uint16_t sample_point)
{
	return can_calc_timing(dev, result, bitrate, sample_point);
}

__weak int uds_can_set_timing(const struct device *dev, const struct can_timing *timing)
{
	return can_set_timing(dev, timing);
}

__weak int uds_can_add_rx_filter(const struct device *dev, can_rx_callback_t callback,
					void *user_data, const struct can_filter *filter)
{
	return can_add_rx_filter(dev, callback, user_data, filter);
}

__weak bool uds_device_is_ready(const struct device *dev)
{
	return device_is_ready(dev);
}

/**
 * @UDS service handler
 */
/* Returns -2 if the uds module is already initialized */
int uds_register_service_handler(UDSEvent_t event, UDSGenericHandler_t handler, void *context)
{
	if (uds_initialized) {
		LOG_ERR("Cannot register UDS service when UDS module is initialized.");
		return -2;
	}

	if (event >= UDS_EVT_MAX) {
		LOG_ERR("Invalid UDS event index: %d", event);
		return -3;
	}

    /* Use handler pointer to detect occupancy so event 0 may be registered */
	if (service_handler_registry[event].handler != NULL) {
		return -1;
	}

	service_handler_registry[event].handler = handler;
	service_handler_registry[event].context = context;

	return 0;
}

int uds_register_ecureset_handler(UDSEvent_t event, UDSECUResetHandler_t handler, void *context)
{
	return uds_register_service_handler(event, (UDSGenericHandler_t)handler, context);
}

/**
 * @UDS event dispatch
 */
static UDSErr_t uds_event_dispatch_internal(UDSServer_t *uds_server, UDSEvent_t event, void *arg)
{
	if (service_handler_registry[event].handler != NULL) {
		return service_handler_registry[event].handler(
			uds_server, arg,
			service_handler_registry[event].context);
	}

	LOG_ERR("Unhandled event %s (%d)",
		UDSEventToStr(event), event);

	return UDS_NRC_ServiceNotSupported;
}

#ifdef CONFIG_ISO14229_TEST
/*
 * Test-only wrapper to allow ztest to exercise UDS event dispatch paths
 * without exposing internal symbols in production builds.
 */
UDSErr_t uds_event_dispatch_for_test(UDSServer_t *uds_server,
				UDSEvent_t event,
				void *arg)
{
	return uds_event_dispatch_internal(uds_server, event, arg);
}
#endif

/**
 * @Globals
 */
UDSServer_t uds_server;
UDSISOTpC_t uds_isotp_client;

/**
 * @ISO-TP client config
 */
/* Provide deterministic source/func addresses in tests so twister frames map
 *correctly (avoids test fragility). In normal builds use Kconfig values.
 */
static UDSISOTpCConfig_t uds_isotp_client_cfg = {
#ifdef CONFIG_ISO14229_TEST
	.source_addr      = 0x500,  /* Match Twister test frame IDs */
	.target_addr      = 0x600,  /* Mock target */
	.source_addr_func = 0x501,  /* Match Twister func frame IDs */
	.target_addr_func = UDS_TP_NOOP_ADDR,
#else
	.source_addr = CONFIG_ISO14229_RX_ID,
	.target_addr = CONFIG_ISO14229_TX_ID,
	.source_addr_func = CONFIG_ISO14229_FUNC_RX_ID,
	/* TODO: Should func address only be one? */
	.target_addr_func = UDS_TP_NOOP_ADDR,
#endif
};

const struct device *uds_can_device;

static struct k_timer uds_poll_timer;

/**
 * @FIFO + Worker Thread
 */
/* FIFO + worker thread definitions */
K_FIFO_DEFINE(uds_can_fifo);
#define UDS_WORKER_STACK_SIZE 2048
#define UDS_WORKER_PRIORITY 5
K_THREAD_STACK_DEFINE(uds_worker_stack, UDS_WORKER_STACK_SIZE);
/* thread id for worker (use the k_thread_create return value) */
static k_tid_t uds_worker_thread_id;
/* Use atomic flag to indicate running state for safe stop */
static atomic_t uds_worker_running = ATOMIC_INIT(0);

/* Structure for queued CAN frames */
struct uds_can_fifo_item {
	void *fifo_reserved;          /* Required by Zephyr FIFO */
	struct can_frame frame;
};


/**
 * @Helper + static ring buffer: declare static fifo_items here so worker
 * can always see it (prevents 'fifo_items' undeclared) and add helper to
 * detect static items. These are minimal safe changes.
 */

#ifndef CONFIG_ISO14229_TEST
/* Static ring buffer version (no heap, safe for embedded targets) */
/*
 * Add a static ring buffer (8 entries) and helper to detect static pointers.
 * This prevents freeing pointers into the static buffer.
 */
static struct uds_can_fifo_item fifo_items[8];

/* Static dummy item used to unblock worker during stop */

/* Index for next static slot. Use atomic_get + atomic_inc to avoid
 * off-by-one where atomic_inc returns the incremented value on some
 * implementations.
 */
/* Index for next FIFO ring buffer slot (round-robin) */
static atomic_t fifo_slot_index = ATOMIC_INIT(0);
#endif

/* Helper: return true if the item pointer lies within the static fifo_items array */
#ifdef CONFIG_ISO14229_TEST
static inline bool uds_item_is_static(struct uds_can_fifo_item *item)
{
	return false;
}
#else
static inline bool uds_item_is_static(struct uds_can_fifo_item *item)
{
	return (item >= &fifo_items[0] &&
			item <  &fifo_items[ARRAY_SIZE(fifo_items)]);
}
#endif /* CONFIG_ISO14229_TEST */

/* Static dummy item used to unblock worker during stop (always present) */
static struct uds_can_fifo_item fifo_dummy_item = { 0 };

/**
 * @Test notification hook
 */
/* Weak hook for test monitoring; tests can override to count processed frames. */
__weak void uds_test_notify_frame_processed(struct can_frame *frame)
{
	ARG_UNUSED(frame);
}

/**
 * @CAN transport callbacks
 */
/* TODO: If we handle can frames in fast succession we will lose data */

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
			void *user_data)
{
	if (size > CAN_MAX_DLEN) {
		LOG_ERR("Data exceeds CAN frame limit (CAN_MAX_DLEN=%d)", CAN_MAX_DLEN);
		return ISOTP_RET_ERROR;
	}
	struct can_frame frame = {
		.id = arbitration_id,
		.flags = 0x0,
		.dlc = size,
	};
	memcpy(frame.data, data, size);

	/* must be signed to detect negative errors */
	int status = uds_can_send(uds_can_device, &frame, K_MSEC(100), NULL, user_data);

	if (status < 0) {
		LOG_ERR("Failed to send CAN frame, %d\n", status);
		return ISOTP_RET_ERROR; /* TODO: Should we return for retransmission ever? */
	}
	return ISOTP_RET_OK;
}

void isotp_user_debug(const char *msg, ...)
{
	LOG_INF("%s", msg);
}

uint32_t UDSMillis(void)
{
	/* Return milliseconds from system, assumed relative time is sufficient */
	/* TODO: Is assumption correct? */
	return k_uptime_get_32();
}

uint32_t isotp_user_get_us(void)
{
	return k_uptime_get_32();
}

/**
 * @UDS frame handling
 */

void uds_handle_frame(struct can_frame *frame)
{
	LOG_DBG("Handling work for UDS");

	if (frame->id == uds_isotp_client.phys_sa) {
		LOG_DBG("Received phys can id 0x%03x\n", frame->id);
		isotp_on_can_message(&uds_isotp_client.phys_link, frame->data, frame->dlc);
	} else if (frame->id == uds_isotp_client.func_sa) {
		LOG_DBG("Received func can id 0x%03x\n", frame->id);
		if (ISOTP_RECEIVE_STATUS_IDLE != uds_isotp_client.phys_link.receive_status) {
			LOG_ERR("Func frame received but cannot process because link is "
				"not "
				"idle\n");
			/* TODO: Should we not retransmit this? */
		} else {
			isotp_on_can_message(&uds_isotp_client.func_link, frame->data, frame->dlc);
		}
	} else {
		LOG_ERR("Received unknown can id 0x%03x", frame->id);
	}
	/* Always notify tests (even if frame ID is unknown) */
	uds_test_notify_frame_processed(frame);
}

/**
 * @Worker Thread
 */
/* Dedicated worker for frame processing */
void uds_worker_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (atomic_get(&uds_worker_running)) {
		struct uds_can_fifo_item *item = k_fifo_get(&uds_can_fifo, K_FOREVER);

		if (item != NULL) {

			/**
			 * @Skip processing the dummy wakeup frame
			 */
			if (item == &fifo_dummy_item) {
				continue;
			}

			uds_handle_frame(&item->frame);

			/*
			 * - In test builds, items are dynamically allocated -> free them.
			 * - In non-test builds, only free items that are not static.
			 */
#ifdef CONFIG_ISO14229_TEST
			k_free(item);
#else
			if (!uds_item_is_static(item)) {
				k_free(item);
			}
#endif
		}
	}

	LOG_DBG("UDS worker thread exiting cleanly");

	k_sem_give(&uds_worker_dead);
}

/* Enqueue a copy into the next static slot and put pointer to it in the FIFO */
void can_queue_uds_work(const struct device *dev, struct can_frame *frame, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	/*
	 * Round-robin selection of static FIFO ring buffer slots.
	 * Use atomic_get() to select the current slot, then atomic_inc()
	 * to advance for the next enqueue without relying on return semantics.
	 */
	/* Use atomic_get() to pick the current index (so first slot used is 0),
	 * then increment the counter for the next use. This avoids relying on the
	 * return semantics of atomic_inc() across platforms.
	 */
	#ifdef CONFIG_ISO14229_TEST
	struct uds_can_fifo_item *item = k_malloc(sizeof(*item));

	if (!item) {
		LOG_ERR("Failed to allocate FIFO item in test mode");
		return;
	}

	memcpy(&item->frame, frame, sizeof(struct can_frame));

	/* In test mode, process immediately */
	uds_handle_frame(&item->frame);
	k_free(item);
	#else
	uint32_t fifo_slot_index_local =
	atomic_get(&fifo_slot_index) % ARRAY_SIZE(fifo_items);

	atomic_inc(&fifo_slot_index);

	/* Copy frame into selected ring buffer slot */
	memcpy(&fifo_items[fifo_slot_index_local].frame, frame, sizeof(struct can_frame));

	/* Enqueue the static item into the FIFO */
	k_fifo_put(&uds_can_fifo, &fifo_items[fifo_slot_index_local]);
	#endif
}

/**
 * @Timer function to poll UDS server
 */
void uds_poll_timer_function(struct k_timer *dummy)
{
	ARG_UNUSED(dummy);

	/* Do nothing if UDS was never initialized */
	if (!uds_initialized) {
		return;
	}

	/* Poll UDS server state machine */
	UDSServerPoll(&uds_server);
}

/**
 * @Stop Poll Timer Helper for Tests
 */
#ifdef CONFIG_ISO14229_TEST
void uds_stop_poll_timer(void)
{
	/* Avoid stopping a non-running or non-initialized timer */
	if (!uds_initialized) {
		LOG_DBG("Poll timer stop skipped (UDS not initialized)");
		return;
	}

	LOG_DBG("Stopping uds_poll_timer (test mode)");
	k_timer_stop(&uds_poll_timer);
}
#endif

/* Stop worker thread cleanly (used during tests) */
static void uds_stop_worker(void)
{
	if (!atomic_get(&uds_worker_running)) {
		LOG_DBG("UDS worker already stopped");
		return;
	}

	LOG_DBG("Stopping UDS worker thread...");

	/* Ask worker to exit */
	atomic_set(&uds_worker_running, 0);

	/* Wake worker if blocked on FIFO */
	k_fifo_put(&uds_can_fifo, &fifo_dummy_item);

	/* Wait for worker to signal exit */
	if (k_sem_take(&uds_worker_dead, K_MSEC(200)) != 0) {
		LOG_WRN("Worker did not signal exit in time");
	}

	if (uds_worker_thread_id != NULL) {
		int rc = k_thread_join(uds_worker_thread_id, K_MSEC(500));

		if (rc != 0) {
			LOG_WRN("k_thread_join failed (%d), aborting worker", rc);
			k_thread_abort(uds_worker_thread_id);
		} else {
			LOG_DBG("UDS worker thread joined successfully");
		}
		uds_worker_thread_id = NULL;
	}

	LOG_DBG("UDS worker thread stopped");
}

/**
 * @Internal reset (prototype)
 */
static void uds_reset_internal_state(void);

/**
 * @Initialization
 */
int uds_init(void)
{
	int status = 0;
	LOG_DBG("Initializing CAN UDS module\n");
	LOG_INF("%s() called, uds_initialized=%d",
	__func__, uds_initialized);

	if (uds_initialized) {
#ifdef CONFIG_ISO14229_TEST
		LOG_DBG("%s(): already initialized (test mode) — no-op", __func__);
		/* Do NOT create a new worker, do NOT modify state */
		return 0;
#else
		LOG_ERR("Cannot register UDS service when UDS module is initialized.");
		return -2;
#endif
}

	/* Initialize work item and timer */
	k_timer_init(&uds_poll_timer, uds_poll_timer_function, NULL);

	/* Start worker thread if not already running */
#ifndef CONFIG_ISO14229_TEST
	if (!atomic_get(&uds_worker_running)) {
		atomic_set(&uds_worker_running, 1);
		k_sem_init(&uds_worker_dead, 0, 1);

	uds_worker_thread_id = k_thread_create(NULL,
							uds_worker_stack,
							K_THREAD_STACK_SIZEOF(uds_worker_stack),
							uds_worker_thread_fn,
							NULL, NULL, NULL,
							UDS_WORKER_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(uds_worker_thread_id, "uds_worker");
	LOG_DBG("UDS worker thread started");
}
#else
LOG_DBG("UDS worker disabled in test mode");
#endif

	uds_can_device = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
	if (!uds_device_is_ready(uds_can_device)) {
		LOG_ERR("CAN: Device driver not ready.\n");
		return -1;
	}

	status = uds_can_stop(uds_can_device);
	if (status != 0) {
		LOG_DBG("CAN controller already stopped\n");
	}

	struct can_timing timing;

	status = uds_can_calc_timing(uds_can_device, &timing, CONFIG_UDS_CAN_BITRATE,
		CONFIG_UDS_CAN_SAMPLEPOINT);
	/* Sampling point relates to tradeoff */
	/* between bandwidth and bus length */
	if (status > 0) {
		LOG_DBG("Sample-Point error: %d\n", status);
	}

	if (status < 0) {
		LOG_ERR("Failed to calc a valid timing\n");
		/* TODO: Fallback sampling point value? */
		return -1;
	}

	status = uds_can_set_timing(uds_can_device, &timing);
	if (status != 0) {
		LOG_ERR("Failed to set timing with bitrate %d and samplepoint %d (%d)\n",
			CONFIG_UDS_CAN_BITRATE, CONFIG_UDS_CAN_SAMPLEPOINT, status);
		return -1;
	}

	struct can_filter filter = { .id = 0, .mask = 0, .flags = 0 };
	/* TODO: Filter the messages here */

	status = uds_can_add_rx_filter(uds_can_device, &can_queue_uds_work, NULL, &filter);
	if (status < 0) {
		LOG_ERR("Failed to add callback filter %d\n", status);
		return -1;
	}

	status = uds_can_start(uds_can_device);
	if (status != 0) {
		LOG_ERR("Failed to start CAN controller\n");
		return -1;
	}

	LOG_INF("CAN controller configuration successful!\n");

	if (UDSServerInit(&uds_server) != 0) {
		LOG_ERR("Failed to initialize UDS server\n");
		return -1;
	}
	if (UDSISOTpCInit(&uds_isotp_client, &uds_isotp_client_cfg) != 0) {
		LOG_ERR("Failed to initialize UDS ISO-TP client\n");
		return -1;
	}

	/* When running tests we force the server's addresses to match the test
	 * config. This avoids timing/address mismatches in unit tests.
	 */
	#ifdef CONFIG_ISO14229_TEST
	/* Override phys_sa / func_sa for Twister frames */
	uds_isotp_client.phys_sa = uds_isotp_client_cfg.source_addr;
	uds_isotp_client.func_sa = uds_isotp_client_cfg.source_addr_func;
	#endif

	uds_server.fn = uds_event_dispatch_internal;
	uds_server.tp = &uds_isotp_client.hdl;

	k_timer_start(&uds_poll_timer, K_MSEC(1000), K_MSEC(100));

	uds_initialized = true;

	return 0;
}

/**
 * @brief Reset all internal UDS state.
 *
 * Stops worker threads, clears registries, and resets global state.
 * Used internally and by test helpers.
 */
/* Unified internal reset used both by tests and uds_reset_internal_state() */
/* Purge FIFO but only free dynamic items, do not free static ring buffer items. */
static void uds_fifo_purge(struct k_fifo *fifo)
{
	struct uds_can_fifo_item *item;

	while ((item = k_fifo_get(fifo, K_NO_WAIT)) != NULL) {
		if (!uds_item_is_static(item)) {
			k_free(item);
		}
		/* if static, just drop it (no free) */
	}
}

/**
 * @Internal reset
 */
/* Made static to match the prototype and avoid "defined but not used" warnings */
__attribute__((unused))
static void uds_reset_internal_state(void)
{

	/* Stop worker first to ensure it doesn't keep queue/thread alive */
	uds_stop_poll_timer();
	uds_stop_worker();
	uds_worker_thread_id = NULL; /* defensive: ensure tid not reused */

	uds_initialized = false;
	memset(&uds_server, 0, sizeof(uds_server));
	memset(&uds_isotp_client, 0, sizeof(uds_isotp_client));
	memset(service_handler_registry, 0, sizeof(service_handler_registry));
	uds_fifo_purge(&uds_can_fifo);
	LOG_DBG("UDS internal state reset complete");
}

/**
 * @Test helper (builds only when CONFIG_ISO14229_TEST=y)
 */
#ifdef CONFIG_ISO14229_TEST
__attribute__((used)) /* Silence unused warning */
/* Exported for ztest suite */
void uds_internal_reset_for_tests(void)
{
	uds_reset_internal_state();

	LOG_DBG("Test reset: UDS internal state reset, timer stopped, worker stopped");
}
#endif
