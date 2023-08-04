.. SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
.. sectionauthor:: Neha Francis <n-francis@ti.com>

AM65x Platforms
===============

Introduction:
-------------
The AM65x family of SoCs is the first device family from K3 Multicore
SoC architecture, targeted for broad market and industrial control with
aim to meet the complex processing needs of modern embedded products.

The device is built over three domains, each containing specific processing
cores, voltage domains and peripherals:

1. Wake-up (WKUP) domain:
        * Device Management and Security Controller (DMSC)

2. Microcontroller (MCU) domain:
        * Dual Core ARM Cortex-R5F processor

3. MAIN domain:
        * Quad core 64-bit ARM Cortex-A53

More info can be found in TRM: http://www.ti.com/lit/pdf/spruid7

Boot Flow:
----------
On AM65x family devices, ROM supports boot only via MCU(R5). This means that
bootloader has to run on R5 core. In order to meet this constraint, and for
the following reasons the boot flow is designed as mentioned:

1. Need to move away from R5 asap, so that we want to start *any*
firmware on the R5 cores for example autosar can be loaded to receive CAN
response and other safety operations to be started. This operation is
very time critical and is applicable for all automotive use cases.

2. U-Boot on A53 should start other remotecores for various
applications. This should happen before running Linux.

3. In production boot flow, we might not like to use full U-Boot,
instead use Falcon boot flow to reduce boot time.

.. code-block:: text

 +------------------------------------------------------------------------+
 |        DMSC            |         R5            |        A53            |
 +------------------------------------------------------------------------+
 |    +--------+          |                       |                       |
 |    |  Reset |          |                       |                       |
 |    +--------+          |                       |                       |
 |         :              |                       |                       |
 |    +--------+          |   +-----------+       |                       |
 |    | *ROM*  |----------|-->| Reset rls |       |                       |
 |    +--------+          |   +-----------+       |                       |
 |    |        |          |         :             |                       |
 |    |  ROM   |          |         :             |                       |
 |    |services|          |         :             |                       |
 |    |        |          |   +-------------+     |                       |
 |    |        |          |   |  *R5 ROM*   |     |                       |
 |    |        |          |   +-------------+     |                       |
 |    |        |<---------|---|Load and auth|     |                       |
 |    |        |          |   | tiboot3.bin |     |                       |
 |    |        |          |   +-------------+     |                       |
 |    |        |          |         :             |                       |
 |    |        |          |         :             |                       |
 |    |        |          |         :             |                       |
 |    |        |          |   +-------------+     |                       |
 |    |        |          |   |  *R5 SPL*   |     |                       |
 |    |        |          |   +-------------+     |                       |
 |    |        |          |   |    Load     |     |                       |
 |    |        |          |   |  sysfw.itb  |     |                       |
 |    | Start  |          |   +-------------+     |                       |
 |    | System |<---------|---|    Start    |     |                       |
 |    |Firmware|          |   |    SYSFW    |     |                       |
 |    +--------+          |   +-------------+     |                       |
 |        :               |   |             |     |                       |
 |    +---------+         |   |   Load      |     |                       |
 |    | *SYSFW* |         |   |   system    |     |                       |
 |    +---------+         |   | Config data |     |                       |
 |    |         |<--------|---|             |     |                       |
 |    |         |         |   +-------------+     |                       |
 |    |         |         |   |             |     |                       |
 |    |         |         |   |    DDR      |     |                       |
 |    |         |         |   |   config    |     |                       |
 |    |         |         |   +-------------+     |                       |
 |    |         |         |   |             |     |                       |
 |    |         |<--------|---| Start A53   |     |                       |
 |    |         |         |   |  and Reset  |     |                       |
 |    |         |         |   +-------------+     |                       |
 |    |         |         |                       |     +-----------+     |
 |    |         |---------|-----------------------|---->| Reset rls |     |
 |    |         |         |                       |     +-----------+     |
 |    |  DMSC   |         |                       |          :            |
 |    |Services |         |                       |     +------------+    |
 |    |         |<--------|-----------------------|---->|*ATF/OP-TEE*|    |
 |    |         |         |                       |     +------------+    |
 |    |         |         |                       |          :            |
 |    |         |         |                       |     +-----------+     |
 |    |         |<--------|-----------------------|---->| *A53 SPL* |     |
 |    |         |         |                       |     +-----------+     |
 |    |         |         |                       |     |   Load    |     |
 |    |         |         |                       |     | u-boot.img|     |
 |    |         |         |                       |     +-----------+     |
 |    |         |         |                       |          :            |
 |    |         |         |                       |     +-----------+     |
 |    |         |<--------|-----------------------|---->| *U-Boot*  |     |
 |    |         |         |                       |     +-----------+     |
 |    |         |         |                       |     |  prompt   |     |
 |    |         |         |                       |     +-----------+     |
 |    +---------+         |                       |                       |
 |                        |                       |                       |
 +------------------------------------------------------------------------+

- Here DMSC acts as master and provides all the critical services. R5/A53
  requests DMSC to get these services done as shown in the above diagram.

Sources:
--------
1. Trusted Firmware-A:
        Tree: https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git/
        Branch: master

2. OP-TEE:
        Tree: https://github.com/OP-TEE/optee_os.git
        Branch: master

3. U-Boot:
        Tree: https://source.denx.de/u-boot/u-boot
        Branch: master

4. TI Linux Firmware:
        Tree: git://git.ti.com/processor-firmware/ti-linux-firmware.git
        Branch: ti-linux-firmware

Build procedure:
----------------
1. Trusted Firmware-A:

.. code-block:: bash

 $ make CROSS_COMPILE=aarch64-linux-gnu- ARCH=aarch64 PLAT=k3 \
        TARGET_BOARD=generic SPD=opteed

2. OP-TEE:

.. code-block:: bash

 $ make PLATFORM=k3-am65x CFG_ARM64_core=y

3. U-Boot:

* 4.1 R5:

.. code-block:: bash

 $ make am65x_evm_r5_defconfig
 $ make CROSS_COMPILE=arm-linux-gnueabihf- \
        BINMAN_INDIRS=<path/to/ti-linux-firmware>

* 4.2 A53:

.. code-block:: bash

 $ make am65x_evm_a53_defconfig
 $ make CROSS_COMPILE=aarch64-linux-gnu- \
        BL31=<path/to/trusted-firmware-a/dir>/build/k3/generic/release/bl31.bin \
        TEE=<path/to/optee_os/dir>/out/arm-plat-k3/core/tee-raw.bin \
        BINMAN_INDIRS=<path/to/ti-linux-firmware>

Target Images
--------------
Copy the below images to an SD card and boot:

- GP

        * tiboot3-am65x_sr2-gp-evm.bin, sysfw-am65x_sr2-gp-evm.itb from step 4.1
        * tispl.bin_unsigned, u-boot.img_unsigned from step 4.2

- HS

        * tiboot3-am65x_sr2-hs-evm.bin, sysfw-am65x_sr2-hs-evm.itb from step 4.1
        * tispl.bin, u-boot.img from step 4.2

Image formats:
--------------

- tiboot3.bin:

.. code-block:: text

                +-----------------------+
                |        X.509          |
                |      Certificate      |
                | +-------------------+ |
                | |                   | |
                | |        R5         | |
                | |   u-boot-spl.bin  | |
                | |                   | |
                | +-------------------+ |
                | |                   | |
                | |     FIT header    | |
                | | +---------------+ | |
                | | |               | | |
                | | |   DTB 1...N   | | |
                | | +---------------+ | |
                | +-------------------+ |
                +-----------------------+

- tispl.bin

.. code-block:: text

                +-----------------------+
                |                       |
                |       FIT HEADER      |
                | +-------------------+ |
                | |                   | |
                | |      A53 ATF      | |
                | +-------------------+ |
                | |                   | |
                | |     A53 OP-TEE    | |
                | +-------------------+ |
                | |                   | |
                | |      A53 SPL      | |
                | +-------------------+ |
                | |                   | |
                | |   SPL DTB 1...N   | |
                | +-------------------+ |
                +-----------------------+

- sysfw.itb

.. code-block:: text

                +-----------------------+
                |                       |
                |       FIT HEADER      |
                | +-------------------+ |
                | |                   | |
                | |     sysfw.bin     | |
                | +-------------------+ |
                | |                   | |
                | |    board config   | |
                | +-------------------+ |
                | |                   | |
                | |     PM config     | |
                | +-------------------+ |
                | |                   | |
                | |     RM config     | |
                | +-------------------+ |
                | |                   | |
                | |    Secure config  | |
                | +-------------------+ |
                +-----------------------+

eMMC:
-----
ROM supports booting from eMMC from boot0 partition offset 0x0

Flashing images to eMMC:

The following commands can be used to download tiboot3.bin, tispl.bin,
u-boot.img, and sysfw.itb from an SD card and write them to the eMMC boot0
partition at respective addresses.

.. code-block:: text

 => mmc dev 0 1
 => fatload mmc 1 ${loadaddr} tiboot3.bin
 => mmc write ${loadaddr} 0x0 0x400
 => fatload mmc 1 ${loadaddr} tispl.bin
 => mmc write ${loadaddr} 0x400 0x1000
 => fatload mmc 1 ${loadaddr} u-boot.img
 => mmc write ${loadaddr} 0x1400 0x2000
 => fatload mmc 1 ${loadaddr} sysfw.itb
 => mmc write ${loadaddr} 0x3600 0x800

To give the ROM access to the boot partition, the following commands must be
used for the first time:

.. code-block:: text

 => mmc partconf 0 1 1 1
 => mmc bootbus 0 1 0 0

To create a software partition for the rootfs, the following command can be
used:

.. code-block:: text

 => gpt write mmc 0 ${partitions}

eMMC layout:

.. code-block:: text

            boot0 partition (8 MB)                        user partition
    0x0+----------------------------------+      0x0+-------------------------+
       |     tiboot3.bin (512 KB)         |         |                         |
  0x400+----------------------------------+         |                         |
       |       tispl.bin (2 MB)           |         |                         |
 0x1400+----------------------------------+         |        rootfs           |
       |       u-boot.img (4 MB)          |         |                         |
 0x3400+----------------------------------+         |                         |
       |      environment (128 KB)        |         |                         |
 0x3500+----------------------------------+         |                         |
       |   backup environment (128 KB)    |         |                         |
 0x3600+----------------------------------+         |                         |
       |          sysfw (1 MB)            |         |                         |
 0x3E00+----------------------------------+         +-------------------------+

Kernel image and DT are expected to be present in the /boot folder of rootfs.
To boot kernel from eMMC, use the following commands:

.. code-block:: text

 => setenv mmcdev 0
 => setenv bootpart 0
 => boot

OSPI:
-----
ROM supports booting from OSPI from offset 0x0.

Flashing images to OSPI:

Below commands can be used to download tiboot3.bin, tispl.bin, u-boot.img,
and sysfw.itb over tftp and then flash those to OSPI at their respective
addresses.

.. code-block:: text

 => sf probe
 => tftp ${loadaddr} tiboot3.bin
 => sf update $loadaddr 0x0 $filesize
 => tftp ${loadaddr} tispl.bin
 => sf update $loadaddr 0x80000 $filesize
 => tftp ${loadaddr} u-boot.img
 => sf update $loadaddr 0x280000 $filesize
 => tftp ${loadaddr} sysfw.itb
 => sf update $loadaddr 0x6C0000 $filesize

Flash layout for OSPI:

.. code-block:: text

         0x0 +----------------------------+
             |     ospi.tiboot3(512K)     |
             |                            |
     0x80000 +----------------------------+
             |     ospi.tispl(2M)         |
             |                            |
    0x280000 +----------------------------+
             |     ospi.u-boot(4M)        |
             |                            |
    0x680000 +----------------------------+
             |     ospi.env(128K)         |
             |                            |
    0x6A0000 +----------------------------+
             |   ospi.env.backup (128K)   |
             |                            |
    0x6C0000 +----------------------------+
             |      ospi.sysfw(1M)        |
             |                            |
    0x7C0000 +----------------------------+
             |      padding (256k)        |
    0x800000 +----------------------------+
             |     ospi.rootfs(UBIFS)     |
             |                            |
             +----------------------------+

Kernel Image and DT are expected to be present in the /boot folder of UBIFS
ospi.rootfs just like in SD card case. U-Boot looks for UBI volume named
"rootfs" for rootfs.

To boot kernel from OSPI, at the U-Boot prompt:

.. code-block:: text

 => setenv boot ubi
 => boot

UART:
-----
ROM supports booting from MCU_UART0 via X-Modem protocol. The entire UART-based
boot process up to U-Boot (proper) prompt goes through different stages and uses
different UART peripherals as follows:

.. code-block:: text

 +---------+---------------+-------------+------------+
 | WHO     | Loading WHAT  |  HW Module  |  Protocol  |
 +---------+---------------+-------------+------------+
 |Boot ROM |  tiboot3.bin  |  MCU_UART0  |  X-Modem(*)|
 |R5 SPL   |  sysfw.itb    |  MCU_UART0  |  Y-Modem(*)|
 |R5 SPL   |  tispl.bin    |  MAIN_UART0 |  Y-Modem   |
 |A53 SPL  |  u-boot.img   |  MAIN_UART0 |  Y-Modem   |
 +---------+---------------+-------------+------------+

Note that in addition to X/Y-Modem related protocol timeouts the DMSC
watchdog timeout of 3min (typ.) needs to be observed until System Firmware
is fully loaded (from sysfw.itb) and started.

Example bash script sequence for running on a Linux host PC feeding all boot
artifacts needed to the device:

.. code-block:: text

 MCU_DEV=/dev/ttyUSB1
 MAIN_DEV=/dev/ttyUSB0

 stty -F $MCU_DEV 115200 cs8 -cstopb -parenb
 stty -F $MAIN_DEV 115200 cs8 -cstopb -parenb

 sb --xmodem tiboot3.bin > $MCU_DEV < $MCU_DEV
 sb --ymodem sysfw.itb > $MCU_DEV < $MCU_DEV
 sb --ymodem tispl.bin > $MAIN_DEV < $MAIN_DEV
 sleep 1
 sb --xmodem u-boot.img > $MAIN_DEV < $MAIN_DEV
