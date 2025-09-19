# UDS Server (ISO 14229) — Zephyr Module

This module implements a UDS (Unified Diagnostic Services, ISO 14229)
server over CAN using ISO-TP, targeting Zephyr RTOS.

It supports both production builds and unit testing via Zephyr Twister.

---

## Features

- ISO 14229 UDS server
- ISO-TP transport over CAN
- Event-based service handler registry
- Dedicated FIFO + worker thread for CAN frame processing
- Full unit test coverage using ztest
- Test-only hooks for deterministic behavior

---

## Build & Test

### Unit Tests (Recommended)

Run using Twister:

```sh
west twister -T tests/modules/iso14229