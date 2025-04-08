#ifndef UDS_H_
#define UDS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get milliseconds since startup
 *
 * @return uint32_t Milliseconds since system startup
 */
uint32_t UDSMillis(void);

/**
 * @brief Poll the UDS server for incoming messages
 *
 * This function should be called periodically to process incoming messages.
 *
 * @return uint8_t 0 if successful
 */
uint8_t uds_poll_server(void);

#ifdef __cplusplus
}
#endif

#endif /* UDS_H_ */
