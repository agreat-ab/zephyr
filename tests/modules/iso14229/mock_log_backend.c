/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mock log backend for capturing Zephyr log output in unit tests.
 * Zephyr 3.7.x / native_sim friendly.
 *
 * Important: backend callbacks may be called from IRQ context, so never
 * use blocking APIs (mutexes, sleeping) inside the output handler.
 */

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(mock_log_backend, LOG_LEVEL_DBG);

/* --- Internal circular buffer --- */
#define MOCK_LOG_BUF_SIZE 2048
static char mock_log_buffer[MOCK_LOG_BUF_SIZE];
static size_t mock_log_buf_pos;

/* --- Log output handler --- */
/* This must be non-blocking and safe to call from ISR context.
 * Use irq_lock()/irq_unlock() to protect the buffer.
 */
static int mock_log_output_func(uint8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	unsigned int key = irq_lock();

	if (length >= MOCK_LOG_BUF_SIZE) {
		/* If incoming chunk itself is larger than buffer, drop it */
		length = MOCK_LOG_BUF_SIZE - 1;
	}

	if (mock_log_buf_pos + length >= MOCK_LOG_BUF_SIZE) {
		/* Wrap/truncate: move to beginning */
		mock_log_buf_pos = 0;
		memset(mock_log_buffer, 0, sizeof(mock_log_buffer));
	}

	memcpy(&mock_log_buffer[mock_log_buf_pos], data, length);
	/* Ensure NUL termination in case other code treats buffer as string */
	if (mock_log_buf_pos + length < MOCK_LOG_BUF_SIZE) {
		mock_log_buffer[mock_log_buf_pos + length] = '\0';
	}
	mock_log_buf_pos += length;

	irq_unlock(key);
	return (int)length;
}

LOG_OUTPUT_DEFINE(mock_log_output, mock_log_output_func, NULL, 0);

/* --- Backend API callbacks (Zephyr 3.7.x style) --- */
static void mock_log_backend_process(const struct log_backend *const backend,
									union log_msg_generic *msg)
{
	uint32_t flags = LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP |
			 LOG_OUTPUT_FLAG_LEVEL |
			 LOG_OUTPUT_FLAG_FORMAT_SYSLOG;

	/* log_output_msg_process may call the output function (above)
	 * from different contexts — we already made the output func IRQ-safe.
	 */
	log_output_msg_process(&mock_log_output, &msg->log, flags);
}

static void mock_log_backend_panic(const struct log_backend *const backend)
{
	/* In panic, flush synchronously */
	log_output_flush(&mock_log_output);
}

static void mock_log_backend_dropped(const struct log_backend *const backend,
				     uint32_t cnt)
{
	char dropped_msg[64];
	int len = snprintk(dropped_msg, sizeof(dropped_msg),
			   "[mock_backend] dropped %u messages\n", cnt);
	/* Use the same IRQ-safe output function directly */
	mock_log_output_func((uint8_t *)dropped_msg, (size_t)len, NULL);
}

static const struct log_backend_api mock_log_backend_api = {
	.process = mock_log_backend_process,
	.panic   = mock_log_backend_panic,
	.dropped = mock_log_backend_dropped,
};

/* --- Registration --- */
LOG_BACKEND_DEFINE(mock_log_backend, mock_log_backend_api, true);

/* --- Public test utilities --- */
/* These helpers are also made IRQ-safe using irq_lock as they may be
 * called while logging is active.
 */
void mock_log_backend_clear(void)
{
	unsigned int key = irq_lock();

	memset(mock_log_buffer, 0, sizeof(mock_log_buffer));
	mock_log_buf_pos = 0;
	irq_unlock(key);
}

bool mock_log_backend_contains(const char *substr)
{
	bool found = false;

	unsigned int key = irq_lock();

	if (substr != NULL && strstr(mock_log_buffer, substr) != NULL) {
		found = true;
	}
	irq_unlock(key);
	return found;
}

const char *mock_log_backend_get_buffer(void)
{
	/* Return pointer to buffer (read-only expected). Caller should not modify. */
	return mock_log_buffer;
}

/* --- Initialization helper (call from test setup) --- */
/* Do not enable the backend during SYS_INIT; enable it from test code
 * after kernel + log core are stable to avoid races.
 */
void mock_log_backend_enable(void)
{
	/* Enable the backend (safe to call from thread/test setup) */
	log_backend_enable(&mock_log_backend, NULL, LOG_LEVEL_DBG);
}
