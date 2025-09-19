#pragma once
#include <zephyr/drivers/can.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a UDS service handler for a specific event.
 *
 * Must be called before uds_init().
 */
int uds_register_service_handler(UDSEvent_t event, UDSGenericHandler_t handler,
	void *context);

/**
 * @brief ISO-TP transmit hook used by the UDS stack.
 *
 * Wraps CAN transmission and is overrideable for testing.
 */
int isotp_user_send_can(uint32_t arbitration_id, const uint8_t *data, const uint8_t size,
	void *user_data);

/**
 * @brief Enqueue a received CAN frame for UDS processing.
 */
void can_queue_uds_work(const struct device *dev, struct can_frame *frame, void *user_data);

/* Public UDS server instances */
extern UDSServer_t uds_server;
extern UDSISOTpC_t uds_isotp_client;

/**
 * @brief Handle a CAN frame immediately (used in test mode).
 */
void uds_handle_frame(struct can_frame *frame);

/**
 * @brief Initialize the UDS server and transport.
 */
int uds_init(void);

/**
 * @brief Poll timer callback for the UDS server state machine.
 */
void uds_poll_timer_function(struct k_timer *timer);

#ifdef CONFIG_ISO14229_TEST
/**
 * @brief Reset all internal UDS state for unit tests.
 *
 * Test-only API. Not available in production builds.
 */
void uds_internal_reset_for_tests(void);

/**
 * @brief Dispatch a UDS event directly (test-only).
 *
 * Exposes internal dispatch logic for unit testing without
 * making it public in production builds.
 */
UDSErr_t uds_event_dispatch_for_test(UDSServer_t *uds_server, UDSEvent_t event,
	void *arg);
#endif /* CONFIG_ISO14229_TEST */

#ifdef __cplusplus
}
#endif
