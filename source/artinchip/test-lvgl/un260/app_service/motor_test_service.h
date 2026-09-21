#ifndef UN260_MOTOR_TEST_SERVICE_H
#define UN260_MOTOR_TEST_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { MOTOR_IDLE, MOTOR_QUEUED, MOTOR_STARTING, MOTOR_RUNNING,
    MOTOR_STOPPING, MOTOR_UNCONFIRMED, MOTOR_REJECTED, MOTOR_SEND_FAILED } motor_test_phase_t;
typedef struct { motor_test_phase_t phase; bool running, pending, queued; } motor_test_snapshot_t;
bool motor_test_service_request(unsigned motor, bool run);
void motor_test_service_stop_all(void);
void motor_test_service_poll(uint32_t now);
void motor_test_service_on_reply(uint8_t command, uint8_t result);
motor_test_snapshot_t motor_test_service_get(unsigned motor);
bool motor_test_service_busy(void);
#endif
