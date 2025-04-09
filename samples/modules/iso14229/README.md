# TODO

- [ ] Contact the author of driftregion/iso14229
- [ ] Start the license approval process

# RFC

## Problem description

## Overview of proposal

This is a proposal to introduce a module that provides a UDS server that can communicate over CAN according to ISO14229-3. The module will provide a layer for translating ISO-TP (ISO15765-2) to UDS, as well as mechanisms for some flow control, but it will be up to each user of the module to define responses to the services.

## Use cases

Define methods for e.g. reading memory, flashing and resetting your ECU using a UDS compliant tool on
an external machine, connected via CAN bus.

## Design details

The current naive prototype can be divided into three parts, one external library, bridging code between the library and Zephyr interfaces, as well as an API for the module user.

The library (driftregion/iso14229,
[https://github.com/driftregion/iso14229/tree/main]) implements part of
the UDS protocol, and bundles an ISO-TP implementation. This was the most appropriate
existing library that I was able to findl. For each service, the library defines an
Args struct that provides the service-unique parameters.

The layer between the library and zephyr interfaces, will also provide most of the API to the
module user.

The layer backend consists of a callback that passes CAN frames from the zephyr CAN subsystem to
the library isotp interface. It also consists of a timer that triggers UDS frame processing in the
library UDSServer.

The layer frontend (module user API) consists of a init function, Kconfig options for CAN
addresses, bitrate, etc. and an interface for registering service callbacks.

### API

## Dependencies

## Concerns and Unresolved questions

- what mode of external component integration should be used?
- is the current thread safety strategy for the registry ok?
- is the third party library appropriate for use?

## Alternatives

Some alternatives to my current approach:

* Implement iso14229 from scratch instead of using a third party library
*

## Test Strategy

For now I have only tested the prototype with hardware (mr_canhubk3), but my aim is to set up
unittests and (if possible) integration tests using qemu and socketcan.


# Hardware setup for testing

RPi (USB)->J-Link Base -> J-Link Cortex-M adapter -> SWD cable -> *DCD-LZ breakout board*
RPi (USB)->USB to TTL cable->*DCD-LZ breakout board*
*DCD-LZ breakout board*->MR-CANHUBK3 (P6)
Rpi(USB)->PCAN usb -> homemade d-sub to jst gh cable (with can terminators) -> MR-CANHUBK3 (P12)

AC mains->12v adapter to barrel jack-> barrel jack to JST SY --> JST SY to JST GH --> MR-CANHUBK3 (P27)

## Software setup for testing

The pcan driver was already part of the Raspbian distro. See appendix E [https://www.peak-system.com/produktcd/Pdf/English/PCAN-USB_UserMan_eng.pdf]

Install segger jlink software pack

[https://www.segger.com/downloads/jlink#J-LinkSoftwareAndDocumentationPack]

[wget https://www.segger.com/downloads/jlink/JLink_Linux_arm64.deb]

Setup a zephyrproject according to Zephyr getting started guide [https://docs.zephyrproject.org/latest/develop/getting_started/index.html].

Move origin to new remote called upstream, set oslundstrom/zephyr as origin and checkout any of the uds related branches. Then run west update.

## Scripts

./scripts/send_uds_ecu_reset
./scripts/send_uds_unknown

# Resources

Devboard: https://www.nxp.com/part/MR-CANHUBK344
Standard: https://www.sis.se/produkter/fordonsteknik/diagnostik-underhalls-och-provningsutrustningar/iso-14229-32022/
Procure: https://www.mouser.se/ProductDetail/NXP-Semiconductors/MR-CANHUBK344?qs=ulEaXIWI0c%2FcZcyHO8xKsQ%3D%3D&srsltid=AfmBOopx4huYh7UhgGN1_Ykb7Ss5TmiOxJhXvh2I0J_qzpld_6s39dZArKo

S32K3xx datasheet https://www.nxp.com/docs/en/data-sheet/S32K3xx.pdf

# Evaluation of existing UDS libraries for use in zephyr

Performed March 2025.

| URL                                                               | commits | last commit | lang       | license        | comment                       |
| ----------------------------------------------------------------- | ------- | ----------- | ---------- | -------------- | ----------------------------- |
| ~~https://github.com/test-fullautomation/robotframework-uds<br>~~ | ~~141~~ | ~~2 mo~~    | ~~python~~ | ~~Apache-2.0~~ | Python                        |
| ~~https://github.com/astand/uds-to-go<br>~~                       | ~~2~~   | ~~2 y~~     | ~~C/C++~~  | ~~MIT~~        | Inactive                      |
| https://github.com/Serpent03/UDS-Protocol<br>                     | 144     | 1 y         | C          | none           | Looks incomplete (no license) |
| https://github.com/driftregion/iso14229<br>                       | 222     | 1 week      | C          | MIT            |                               |
| https://github.com/openxc/uds-c                                   | 118     | 4 y         | C          | BSD-3          | no server impl                |
|                                                                   |         |             |            |                |                               |
|                                                                   |         |             |            |                |                               |
