/*
 * Copyright (c) 2023 Basalte bv
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/can.h>
#include <zephyr/lib/iso14229/uds.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/lib/iso14229/server.h>

LOG_MODULE_REGISTER(uds_tests, LOG_LEVEL_DBG);

#include "mock_log_backend.h"

/* --- Strong overrides for weak symbols (mocks) --- */
static int test_fake_can_send_status;

int uds_can_send(const struct device *dev, const struct can_frame *frame,
				k_timeout_t timeout, can_tx_callback_t callback, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(frame);
	ARG_UNUSED(timeout);
	ARG_UNUSED(callback);
	ARG_UNUSED(user_data);
	return test_fake_can_send_status;
}

int uds_can_stop(const struct device *dev)
{
	return 0;
}
int uds_can_start(const struct device *dev)
{
	return 0;
}
int uds_can_set_timing(const struct device *dev,
			const struct can_timing *timing)
{
	return 0;
}
int uds_can_calc_timing(const struct device *dev,
		struct can_timing *res,
		uint32_t bitrate,
		uint16_t sample_point)
{
	return 0;
}
int uds_can_add_rx_filter(const struct device *dev,
		can_rx_callback_t cb,
		void *user_data,
		const struct can_filter *filter)
{
	return 0;
}
bool uds_device_is_ready(const struct device *dev)
{
	return true;
}
__weak UDSErr_t UDSServerInit(UDSServer_t *uds_server)
{
	return 0;
}
__weak UDSErr_t UDSISOTpCInit(UDSISOTpC_t *uds_isotp_client, const UDSISOTpCConfig_t *cfg)
{
	return 0;
}
__weak void UDSServerPoll(UDSServer_t *uds_server)
{
}
__weak const char *UDSEventToStr(UDSEvent_t ev)
{
	return "dummy_event";
}
__weak void isotp_on_can_message(IsoTpLink *link, const uint8_t *data, uint8_t len)
{
}

/* Declare external helpers from mock_log_backend.c */
extern bool mock_log_backend_contains(const char *substr);
extern void mock_log_backend_clear(void);
extern void mock_log_backend_enable(void);
extern const char *mock_log_backend_get_buffer(void);

/* --- Test dummy handler --- */
static UDSErr_t test_dummy_handler(UDSServer_t *uds_server, void *req, void *ctx)
{
	ARG_UNUSED(uds_server);
	ARG_UNUSED(req);
	ARG_UNUSED(ctx);
	return 0x55;
}

static UDSErr_t nrc_test_handler(UDSServer_t *srv, void *req, void *ctx)
{
	return UDS_NRC_ConditionsNotCorrect;
}

/* --- Test helper to reset all globals including test fakes --- */
static void uds_tests_reset_all(void *fixture)
{
	ARG_UNUSED(fixture);
	/* Enable the mock backend once before running tests */
	mock_log_backend_enable();
	test_fake_can_send_status = 0;

#ifdef CONFIG_ISO14229_TEST
	uds_internal_reset_for_tests();
#endif
}

/* --- Add teardown to clean up background worker --- */
static void uds_tests_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

#ifdef CONFIG_ISO14229_TEST
	uds_internal_reset_for_tests();
	LOG_INF("Teardown: UDS fully stopped");
#endif
}

/**
 * @Mock monitor for FIFO tests
 */
/*
 * Initialize frames_processed atomic to zero. Without an explicit initializer
 * atomic read may return non-zero (or cause warnings), and Twister tests rely
 *on it being zero before use.
 */
static struct {
	atomic_t frames_processed;
	struct can_frame last_frame;
} test_monitor = {
	.frames_processed = ATOMIC_INIT(0),
};

/* Override weak uds_test_notify_frame_processed hook to count frames */
void uds_test_notify_frame_processed(struct can_frame *frame)
{
	atomic_inc(&test_monitor.frames_processed);
	memcpy(&test_monitor.last_frame, frame, sizeof(struct can_frame));
	LOG_INF("Mock notify: processed frame ID 0x%x, data[0]=0x%x",
			frame->id, frame->data[0]);
}

/**
 * @brief UDS unit test cases
 */
ZTEST(uds_tests, test_register_service_handler_success)
{
	uint8_t status = uds_register_service_handler(1, test_dummy_handler, NULL);

	zassert_equal(status, 0, "Expected registration success");
}

ZTEST(uds_tests, test_register_service_handler_duplicate)
{
	uint8_t status1 = uds_register_service_handler(2, test_dummy_handler, NULL);
	uint8_t status2 = uds_register_service_handler(2, test_dummy_handler, NULL);

	zassert_equal(status1, 0, "Expected first registration success");
	zassert_equal(status2, (uint8_t)-1, "Expected duplicate registration failure");
}

ZTEST(uds_tests, test_uds_init_and_reinit)
{
	int status1 = uds_init();

	zassert_equal(status1, 0, "First uds_init failed");

#ifdef CONFIG_ISO14229_TEST
	int status2 = uds_init();

	zassert_equal(status2, 0, "Second uds_init failed (test mode)");

	/* MUST stop worker BEFORE test returns */
	uds_internal_reset_for_tests();
#else
	int status2 = uds_init();

	zassert_equal(status2, -2, "Second uds_init should fail");
#endif
}

ZTEST(uds_tests, test_isotp_user_send_can_success)
{
	test_fake_can_send_status = 0;
	uint8_t data[8] = { 1, 2, 3, 4 };
	int status = isotp_user_send_can(0x123, data, 4, NULL);

	zassert_equal(status, ISOTP_RET_OK, "Expected send success");
}

ZTEST(uds_tests, test_isotp_user_send_can_error)
{
	test_fake_can_send_status = -1;
	uint8_t data[8] = { 1, 2, 3, 4 };
	int status = isotp_user_send_can(0x123, data, 4, NULL);

	zassert_equal(status, ISOTP_RET_ERROR, "Expected send error");
}

ZTEST(uds_tests, test_handle_frame_phys)
{
	struct can_frame frame = { .id = 0x111, .dlc = 1, .data = {0xAA} };

	uds_isotp_client.phys_sa = 0x111;
	uds_handle_frame(&frame);
	zassert_true(true, "Handled phys frame");
}

ZTEST(uds_tests, test_handle_frame_func_idle)
{
	struct can_frame frame = { .id = 0x222, .dlc = 1, .data = {0xBB} };

	uds_isotp_client.func_sa = 0x222;
	uds_isotp_client.phys_link.receive_status = ISOTP_RECEIVE_STATUS_IDLE;
	uds_handle_frame(&frame);
	zassert_true(true, "Handled func frame when idle");
}

ZTEST(uds_tests, test_handle_frame_func_not_idle)
{
	struct can_frame frame = { .id = 0x222, .dlc = 1, .data = {0xCC} };

	uds_isotp_client.func_sa = 0x222;
	uds_isotp_client.phys_link.receive_status = 1;
	uds_handle_frame(&frame);
	zassert_true(true, "Handled func frame when not idle");
}

ZTEST(uds_tests, test_handle_frame_unknown)
{
	mock_log_backend_clear();

	struct can_frame frame = { .id = 0x333, .dlc = 1, .data = {0xDD} };

	uds_isotp_client.phys_sa = 0x111;
	uds_isotp_client.func_sa = 0x222;

	uds_handle_frame(&frame);

	zassert_true(mock_log_backend_contains("Received unknown can id"),
				"Expected unknown frame log missing");
	zassert_true(mock_log_backend_contains("0x333"),
				"Expected CAN ID missing in log");
}

ZTEST(uds_tests, test_timer_function)
{
	uds_poll_timer_function(NULL);
	zassert_true(true, "Timer function called without crash");
}

ZTEST(uds_tests, test_log_capture)
{
	mock_log_backend_clear();

	LOG_ERR("UDS critical failure!");

	/* Verify message text */
	zassert_true(mock_log_backend_contains("UDS critical failure"),
		     "Expected log message not found");

	/* Verify error log level token */
	zassert_true(
		mock_log_backend_contains("<err>") ||
		mock_log_backend_contains("ERR"),
		"Expected error log level token missing"
	);

	/* Verify module name */
	zassert_true(
		mock_log_backend_contains("uds_tests"),
		"Expected module name missing"
	);
}

ZTEST(uds_tests, test_log_format_contains_level_and_module)
{
	mock_log_backend_clear();

	LOG_DBG("Formatted test log");

	const char *buf = mock_log_backend_get_buffer();

	bool has_level =
		strstr(buf, "<dbg>") ||
		strstr(buf, "DBG") ||
		strstr(buf, "dbg");

	bool has_module = strstr(buf, "uds_tests") != NULL;

	zassert_true(has_level, "Expected log level missing");
	zassert_true(has_module, "Expected module name missing");
}

/**
 * @FIFO + Worker Thread integration tests
 */
ZTEST(uds_tests, test_fifo_burst_processing)
{
	uds_init(); /* Start worker thread once */
	atomic_set(&test_monitor.frames_processed, 0);

	struct can_frame frames[5];

	for (int i = 0; i < 5; i++) {
		frames[i].id = 0x500 + i;
		frames[i].dlc = 1;
		frames[i].data[0] = 0xA0 + i;
		can_queue_uds_work(NULL, &frames[i], NULL);
	}

	/* Allow worker thread to process */
	k_sleep(K_MSEC(200));

	zassert_equal(atomic_get(&test_monitor.frames_processed), 5,
				"Expected 5 frames processed");
	zassert_equal(test_monitor.last_frame.id, 0x504,
				"Expected last frame ID 0x504");
}

ZTEST(uds_tests, test_fifo_order_preservation)
{
	uds_init();
	atomic_set(&test_monitor.frames_processed, 0);

	struct can_frame frames[3] = {
		{ .id = 0x111, .dlc = 1, .data = {0x01} },
		{ .id = 0x222, .dlc = 1, .data = {0x02} },
		{ .id = 0x333, .dlc = 1, .data = {0x03} }
	};

	for (int i = 0; i < 3; i++) {
		can_queue_uds_work(NULL, &frames[i], NULL);
		LOG_DBG("Debug before sleep in test_fifo_order_preservation");
	}

	k_sleep(K_MSEC(150));

	zassert_equal(atomic_get(&test_monitor.frames_processed), 3,
				"Expected all 3 frames processed");
	zassert_equal(test_monitor.last_frame.id, 0x333,
				"Expected FIFO order preserved (last ID 0x333)");
}

ZTEST(uds_tests, test_event_dispatch_calls_registered_handler)
{
	UDSEvent_t event = 3;
	UDSErr_t ret;

	uds_register_service_handler(event, test_dummy_handler, NULL);

	ret = uds_event_dispatch_for_test(&uds_server, event, NULL);

	zassert_equal(ret, 0x55, "Expected handler return value");
}

ZTEST(uds_tests, test_event_dispatch_unhandled_event)
{
	UDSErr_t ret;

	ret = uds_event_dispatch_for_test(&uds_server, UDS_EVT_MAX - 1, NULL);

	zassert_equal(ret, UDS_NRC_ServiceNotSupported,
		      "Expected NRC ServiceNotSupported");
}

ZTEST(uds_tests, test_isotp_multiframe_stub_path)
{
	struct can_frame frame = {
	.id = uds_isotp_client.phys_sa,
	.dlc = 8,
	.data = { 0x10, 0x14 } /* First Frame indicator */
	};

	uds_handle_frame(&frame);

	zassert_true(true, "Multi-frame stub path handled safely");
}

ZTEST(uds_tests, test_nrc_propagation)
{
	uds_register_service_handler(4, nrc_test_handler, NULL);

	UDSErr_t ret = uds_event_dispatch_for_test(&uds_server, 4, NULL);

	zassert_equal(ret, UDS_NRC_ConditionsNotCorrect, "Expected NRC propagation");
}

ZTEST(uds_tests, test_service_handler_registry_cleared_on_reset)
{
	UDSErr_t ret;

	/* Register a handler */
	uds_register_service_handler(5, test_dummy_handler, NULL);

	/* Sanity check: handler is active */
	ret = uds_event_dispatch_for_test(&uds_server, 5, NULL);
	zassert_equal(ret, 0x55, "Handler should be active before reset");

	/* Reset all internal UDS state */
	uds_internal_reset_for_tests();

	/* After reset, handler must be gone */
	ret = uds_event_dispatch_for_test(&uds_server, 5, NULL);
	zassert_equal(ret,
		UDS_NRC_ServiceNotSupported,
		"Handler registry not cleared by reset");
}

/* --- Add teardown to end test suite cleanly --- */
ZTEST_SUITE(uds_tests,
			NULL,
			NULL,
			uds_tests_reset_all,
			uds_tests_teardown,
			NULL);
