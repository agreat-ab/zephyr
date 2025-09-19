#ifndef UDS_H_
#define UDS_H_

#include <stdint.h>
#include <zephyr/lib/iso14229/iso14229.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get milliseconds since startup
 *
 * @return uint32_t Milliseconds since system startup
 */
uint32_t UDSMillis(void);

int uds_init(void);

/* Define handler signatures for service handlers, to enforce use of the correct */
/* args-structure */
typedef UDSErr_t (*UDSGenericHandler_t)(UDSServer_t *srv, void *args, void *user_data);
int uds_register_service_handler(UDSEvent_t evt, UDSGenericHandler_t handler, void *context);
typedef UDSErr_t (*UDSECUResetHandler_t)(UDSServer_t *srv, UDSECUResetArgs_t *args,
					 void *user_data);
int uds_register_ecureset_handler(UDSEvent_t evt, UDSECUResetHandler_t handler, void *context);

#ifdef __cplusplus
}
#endif

#endif /* UDS_H_ */
