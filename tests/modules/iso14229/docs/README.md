# UDS Test Suite Documentation

## Overview
This document describes the **UDS (ISO 14229) unit test suite** implemented in the Zephyr Project environment. The test suite validates server initialization, service registration, CAN / ISO-TP transport behavior, FIFO processing, and logging in a **deterministic and hardware-independent** manner.

The documentation is written in **GitHub-Flavored Markdown (GFM)** and is intended to be committed directly to a GitHub repository and rendered automatically by GitHub.

---

## Test Environment

- RTOS: Zephyr
- Target: Native / QEMU (Raspberry Pi target for CI)
- Test framework: Zephyr `ztest`
- Transport: CAN (mocked)
- Protocol: ISO 14229 (UDS)
- Special configuration: `CONFIG_ISO14229_TEST`

### Test Mode Behavior (`CONFIG_ISO14229_TEST`)
When enabled, this configuration ensures deterministic behavior:

- UDS worker thread is disabled
- CAN frames are processed synchronously
- `uds_init()` is re-entrant
- FIFO items are heap allocated
- Poll timers can be safely stopped
- Internal reset APIs are exposed for tests

---

## UDS Test Suite – Purpose and Scope

The test suite verifies:

- Correct initialization and shutdown behavior
- Safe and deterministic service registration
- Robust CAN and ISO-TP frame handling
- FIFO burst handling and order preservation
- Logging and diagnostic visibility

The tests are written to be:

- Repeatable
- Isolated
- Hardware-independent
- Suitable for CI and audit review

---

## UDS Test Suite – Detailed Flowchart (19 Tests)

```mermaid
flowchart TD
    T1[Test 1: Register Service Success]
    T2[Test 2: Reject Duplicate Service]
    T3[Test 3: Init and Re-init]
    T4[Test 4: CAN Send Success]
    T5[Test 5: CAN Send Error]
    T6[Test 6: Physical Frame Handling]
    T7[Test 7: Functional Frame (Idle)]
    T8[Test 8: Functional Frame (Busy)]
    T9[Test 9: Unknown CAN ID]
    T10[Test 10: Poll Timer Safety]
    T11[Test 11: Log Capture]
    T12[Test 12: Log Format]
    T13[Test 13: FIFO Burst]
    T14[Test 14: FIFO Order]

    T15[Test 15: Event Dispatch → Handler]
    T16[Test 16: Event Dispatch → NRC]
    T17[Test 17: ISO-TP First Frame Stub]
    T18[Test 18: NRC Propagation]
    T19[Test 19: Handler Registry Reset]

    T1 --> T2 --> T3
    T3 --> T4 --> T5
    T5 --> T6 --> T7 --> T8 --> T9
    T9 --> T10 --> T11 --> T12
    T12 --> T13 --> T14
    T14 --> T15 --> T16
    T16 --> T17 --> T18 --> T19
```

---

## Test Descriptions

### Purpose of the Global Test Flow

The Global Test Flow defines the **shared execution model** used by all UDS unit and integration tests in this suite. Rather than validating isolated functions, the tests collectively verify that the UDS server behaves correctly as a **stateful, concurrent system** across repeated initialization, message handling, and shutdown cycles.

The purpose of this flow is to ensure that the UDS implementation is **deterministic, re-entrant, and safe to execute in a continuous integration (CI) environment**, where tests are run repeatedly and in isolation.

---

### What the Global Test Flow Does

![Global test flow](doc/modules/iso14229/images/Global_test_flow.png)

For every test case, the following logical phases apply:

1. **Global State Reset**
   All UDS-related global state is reset before each test. This includes:
   - Internal server structures
   - ISO-TP client state
   - Service handler registry
   - FIFO contents
   - Worker threads and timers

   This guarantees that each test starts from a **known, clean baseline**.

2. **Controlled Initialization**
   The UDS module is initialized in `CONFIG_ISO14229_TEST` mode. Hardware-dependent components such as CAN drivers and timers are replaced with mocks, allowing tests to execute deterministically without relying on physical hardware.

3. **Stimulus Injection**
   Each test injects a specific stimulus into the system, such as:
   - Registering a UDS service handler
   - Sending a CAN or ISO-TP frame
   - Triggering the poll timer
   - Enqueuing frames into the FIFO for worker-thread processing

4. **System Processing**
   The UDS server processes the stimulus using its real runtime logic:
   - FIFO-based worker thread execution
   - ISO-TP frame handling
   - UDS event dispatch
   - Logging and error handling

   Test hooks and mocks observe these behaviors without altering execution flow.

5. **Verification and Assertions**
   Each test validates the observed behavior against expected outcomes, including:
   - Return codes
   - Correct execution order
   - FIFO ordering guarantees
   - Log output content
   - Absence of crashes or deadlocks

6. **Clean Shutdown**
   Worker threads, timers, and queues are stopped or drained at the end of each test to ensure no background activity affects subsequent tests.

---

### Why This Global Test Flow Matters

This global test flow is critical for ensuring system reliability and test validity:

- **Test Isolation**
  Ensures that tests do not influence each other, which is essential for CI reliability and reproducibility.

- **Concurrency Safety**
  Validates correct behavior in the presence of worker threads, FIFOs, and asynchronous processing.

- **Reinitialization Robustness**
  Confirms that the UDS server can be safely initialized and deinitialized multiple times — a key requirement for unit testing and fault recovery.

- **Production Relevance**
  Although hardware is mocked, the execution model mirrors real runtime behavior, increasing confidence that test results reflect production behavior.

By applying this shared global flow, the test suite validates not only **functional correctness**, but also **system stability, safety, and long-term maintainability**.

---

### Test 1 – `test_register_service_handler_success`

**Purpose**
Verify that a UDS service handler can be successfully registered before server initialization.

**Description**
The test registers a handler for a valid UDS event and verifies that the registration succeeds without error.

**Why it matters**
- UDS services must be registered during startup.
- Prevents missing or misconfigured diagnostic services.

![Test register service handler success](doc/modules/iso14229/images/Test_register_service_handler_success.png)

---

### Test 2 – `test_register_service_handler_duplicate`

**Purpose**
Ensure duplicate service registration is rejected.

**Description**
The test registers a service handler twice for the same event and verifies that the second registration fails.

**Why it matters**
- Prevents accidental overwriting of diagnostic services.
- Protects service dispatch integrity.

![Test register service handler duplicate](doc/modules/iso14229/images/Test_register_service_handler_duplicate.png)

---

### Test 3 – `test_uds_init_and_reinit`

**Purpose**
Verify correct initialization and controlled re-initialization behavior.

**Description**
The test calls `uds_init()` twice and confirms that re-initialization is allowed in test mode.

**Why it matters**
- Ensures predictable initialization behavior.
- Allows repeatable unit testing without restarting the system.

![Test uds init and reinit](doc/modules/iso14229/images/Test_uds_init_and_reinit.png)

---

### Test 4 – `test_isotp_user_send_can_success`

**Purpose**
Verify correct behavior when CAN transmission succeeds.

**Description**
A mocked CAN send function returns success, and the ISO-TP transmit function is expected to report success.

**Why it matters**
- Ensures ISO-TP transmit logic correctly interprets CAN driver results.
- Critical for positive diagnostic responses.

![Test isotp user send can success](doc/modules/iso14229/images/Test_isotp_user_send_can_success.png)

---

### Test 5 – `test_isotp_user_send_can_error`

**Purpose**
Verify error propagation when CAN transmission fails.

**Description**
The CAN send function is mocked to return an error, and the ISO-TP layer must return an error code.

**Why it matters**
- Prevents silent message loss.
- Ensures transport-layer failures propagate correctly.

![Test isotp user send can error](doc/modules/iso14229/images/Test_isotp_user_send_can_error.png)

---

### Test 6 – `test_handle_frame_phys`

**Purpose**
Verify correct handling of physically addressed CAN frames.

**Description**
A CAN frame matching the physical source address is processed and routed to the physical ISO-TP link.

**Why it matters**
- Physical addressing is required for most UDS services.
- Ensures correct routing of diagnostic requests.

![Test handle frame phys](doc/modules/iso14229/images/Test_handle_frame_phys.png)

---

### Test 7 – `test_handle_frame_func_idle`

**Purpose**
Verify functional addressing when the physical link is idle.

**Description**
A functional CAN frame is processed successfully when no physical session is active.

**Why it matters**
- Functional requests (e.g., broadcast diagnostics) must be handled safely.
- Prevents collisions with ongoing physical sessions.

![Test handle frame func idle](doc/modules/iso14229/images/Test_handle_frame_func_idle.png)

---

### Test 8 – `test_handle_frame_func_not_idle`

**Purpose**
Ensure functional frames are rejected when the physical link is busy.

**Description**
The test forces the physical ISO-TP link into a non-idle state and verifies that functional frames are not processed.

**Why it matters**
- Prevents protocol violations.
- Enforces ISO-TP concurrency rules.

![Test handle frame func not idle](doc/modules/iso14229/images/Test_handle_frame_func_not_idle.png)

---

### Test 9 – `test_handle_frame_unknown`

**Purpose**
Verify safe handling of unknown CAN identifiers.

**Description**
A CAN frame with an unsupported ID is processed, and an error is logged without crashing the server.

**Why it matters**
- Ensures resilience to bus noise or misconfigured nodes.
- Protects system stability.

![Test handle frame unknown](doc/modules/iso14229/images/Test_handle_frame_unknown.png)

---

### Test 10 – `test_timer_function`

**Purpose**
Verify poll timer safety.

**Description**
The polling timer function is called directly to ensure it does not crash when the server is uninitialized.

**Why it matters**
- Timers may fire during shutdown or partial initialization.
- Ensures defensive coding.

![Test timer function](doc/modules/iso14229/images/Test_timer_function.png)

---

### Test 11 – `test_log_capture`

**Purpose**
Verify that UDS error logs are captured by the logging backend.

**Description**
The test emits a log message and confirms it is captured by the mock logging system.

**Why it matters**
- Logging is essential for diagnostics and debugging.
- Required for test verification of error paths.

![Test_log_capture](doc/modules/iso14229/images/Test_log_capture.png)

---

### Test 12 – `test_log_format_contains_timestamp_and_level`

**Purpose**
Verify log output contains required diagnostic metadata.

**Description**
The test checks that log output includes log level and module identification.

**Why it matters**
- Ensures logs remain useful and traceable.
- Supports field debugging and audit requirements.

![Test_log_format_contains_timestamp_and_level](doc/modules/iso14229/images/Test_log_format_contains_timestamp_and_level.png)

---

### Test 13 – `test_fifo_burst_processing`

**Purpose**
Verify reliable processing of multiple CAN frames in a burst.

**Description**
Multiple frames are queued in rapid succession and the test confirms that all frames are processed.

**Why it matters**
- CAN traffic is bursty by nature.
- Ensures no frame loss under load.

![Test_fifo_burst_processing](doc/modules/iso14229/images/Test_fifo_burst_processing.png)

---

### Test 14 – `test_fifo_order_preservation`

**Purpose**
Verify FIFO ordering guarantees.

**Description**
Frames are queued in a known order, and the test verifies that processing preserves that order.

**Why it matters**
- UDS request/response ordering is critical.
- Prevents subtle protocol errors.

![Test_fifo_order_preservation](doc/modules/iso14229/images/Test_fifo_order_preservation.png)

---

### Test 15 – `test_event_dispatch_calls_registered_handler`

**Purpose**
Verify that a registered UDS service handler is correctly invoked when its corresponding event is dispatched.

**Description**
This test registers a dummy service handler for a specific UDS event and then calls uds_event_dispatch_for_test().
The handler returns a known value, which is asserted as the dispatch result.

**Why it matters**
UDS service execution depends entirely on correct event dispatch.
If the dispatcher fails to invoke the correct handler, diagnostic services silently break.
This test validates the core control path that connects incoming requests to service logic.

![Test_event_dispatch_calls_registered_handler](doc/modules/iso14229/images/Test_event_dispatch_calls_registered_handler.png)

---

### Test 16 – `test_event_dispatch_unhandled_event`

**Purpose**
Ensure that dispatching an event with no registered handler returns the correct Negative Response Code (NRC).

**Description**
The test dispatches an event index that has no handler registered.
The expected result is UDS_NRC_ServiceNotSupported.

**Why it matters**
This confirms standards-compliant error handling.
In UDS, unsupported services must return a deterministic NRC — not crash, hang, or return undefined data.

![Test_event_dispatch_unhandled_event](doc/modules/iso14229/images/Test_event_dispatch_unhandled_event.png)

---

### Test 17 – `test_isotp_multiframe_stub_path`

**Purpose**
Ensure that the UDS receive path safely handles ISO-TP First Frame (FF) messages.

**Description**
A CAN frame with a First Frame (0x10) PCI byte is injected via uds_handle_frame().
The test verifies that the stubbed ISO-TP path executes without crashing or corrupting state.

**Why it matters**
Multi-frame messages are common in real diagnostics (e.g., ReadDataByIdentifier).
This test ensures the system is future-proof and safe even before full ISO-TP reassembly is implemented.

![Test_isotp_multiframe_stub_path](doc/modules/iso14229/images/Test_isotp_multiframe_stub_path.png)

---

### Test 18 – `test_nrc_propagation`

**Purpose**
Verify that a service handler returning a Negative Response Code propagates that NRC unchanged.

**Description**
A handler is registered that explicitly returns UDS_NRC_ConditionsNotCorrect.
The dispatch result is checked to ensure the same NRC is returned.

**Why it matters**
This guarantees correct error semantics across layers.
Handlers must be able to control diagnostic responses without being overridden or masked by the dispatcher.

![Test_nrc_propagation](doc/modules/iso14229/images/Test_nrc_propagation.png)

---

### Test 19 – `test_service_handler_registry_cleared_on_reset` (Implicit via uds_internal_reset_for_tests() usage)

**Purpose**
Ensure that service handler registrations do not leak across tests.

**Description**
Before and after tests, uds_internal_reset_for_tests() is called to clear:
- Registered service handlers
- Worker threads
- Timers
- Global UDS state

**Why it matters**
Without proper isolation:
- Tests become order-dependent
- Failures become non-deterministic
- CI results become unreliable
This test infrastructure guarantees repeatable, hermetic test execution.

![Test_service_handler_registry_cleared_on_reset](doc/modules/iso14229/images/Test_service_handler_registry_cleared_on_reset.png)

---

## CI Coverage Checklist

```md
- [x] Server initialization
- [x] Service registration
- [x] CAN transmit success/failure
- [x] Physical addressing
- [x] Functional addressing
- [x] Unknown CAN handling
- [x] Poll timer safety
- [x] FIFO burst handling
- [x] FIFO order preservation
- [x] Logging and diagnostics
- [x] UDS event dispatch logic
- [x] Negative Response Code (NRC) handling
- [x] ISO-TP multi-frame safety paths
- [x] Test isolation and global state reset
```

---

## Conclusion

This UDS unit test suite provides deterministic, repeatable verification of core ISO 14229 server behavior. The tests are suitable for continuous integration, regression testing, and safety-oriented development workflows.
