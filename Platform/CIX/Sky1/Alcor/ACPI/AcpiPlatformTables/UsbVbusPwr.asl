/** @file

  Copyright 2025 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

  Device (UVS0) {
    Name (_HID, "PRP0001")
    Name (_UID, 0x00)
    Name (_STA, 0x0F)

    Name (_CRS, ResourceTemplate () {
	/* S5 GPIO0 30 */
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX1", 0, "pinctrl_usbc2_vbus_pwr", ResourceConsumer,)
      GpioIo (Exclusive, PullNone, 0, 0, IoRestrictionOutputOnly,
      "\\_SB.GPI4", 0, ResourceConsumer) { 30 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () { "compatible", "regulator-fixed" },
        Package () { "regulator-name", "usbc2_vbus_power" },
        Package () { "regulator-min-microvolt", 5000000 },
        Package () { "regulator-max-microvolt", 5000000 },
        Package () { "gpio", Package () { ^UVS0, 0, 0, 0 } },
        Package () { "regulator-pull-down", 1 },
        Package () { "enable-active_high", 1 },
        Package () { "off-on-delay-us", 15000 },
      }
    })
  }

  Device (UVS1) {
    Name (_HID, "PRP0001")
    Name (_UID, 0x01)
    Name (_STA, 0x0F)

    Name (_CRS, ResourceTemplate () {
	/* S5 GPIO0 31 */
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX1", 0, "pinctrl_usbc3_vbus_pwr", ResourceConsumer,)
      GpioIo (Exclusive, PullNone, 0, 0, IoRestrictionOutputOnly,
      "\\_SB.GPI4", 0, ResourceConsumer) { 31 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () { "compatible", "regulator-fixed" },
        Package () { "regulator-name", "usbc3_vbus_power" },
        Package () { "regulator-min-microvolt", 5000000 },
        Package () { "regulator-max-microvolt", 5000000 },
        Package () { "gpio", Package () { ^UVS1, 0, 0, 0 } },
        Package () { "regulator-pull-down", 1 },
        Package () { "enable-active_high", 1 },
        Package () { "off-on-delay-us", 15000 },
      }
    })
  }
