/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOCK_LOG_BACKEND_H_
#define MOCK_LOG_BACKEND_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void mock_log_backend_clear(void);
bool mock_log_backend_contains(const char *substr);
const char *mock_log_backend_get_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_LOG_BACKEND_H_ */
