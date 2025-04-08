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
