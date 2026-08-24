/** @file

  Copyright 2025 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/*
 * DTS mapping:
 *   dp_3_sbu_mux (USBC2) → fch_gpio0 2, pinctrl_dp_3_aux_mux → i2c7/PD00 (HUSB311)
 *   dp_4_sbu_mux (USBC3) → fch_gpio0 3, pinctrl_dp_4_aux_mux → i2c0/PD01 (HUSB311)
 */

#define USBC2_PD_DEVICE     \_SB.I2C7.PD00  /* HUSB311 on I2C7, addr 0x4e */
#define USBC3_PD_DEVICE     \_SB.I2C0.PD01  /* HUSB311 on I2C0, addr 0x4e */

  Device (GSA0) {
    Name (_HID, "CIXH200F")
    Name (_UID, 0x00)
    Name (_STA, 0x0F)

    Name (_CRS, ResourceTemplate () {
	/* FCH GPIO0 2 - matches DTS dp_3_sbu_mux */
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX0", 0, "pinctrl_gpio_dp3_sbu_aux", ResourceConsumer,)
      GpioIo (Exclusive, PullNone, 0, 0, IoRestrictionOutputOnly,
      "\\_SB.GPI0", 0, ResourceConsumer) { 2 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () { "compatible", "gpio-sbu-mux" },
        Package () { "orientation-switch", 0 },
	Package () { "select-gpio", Package () { ^GSA0, 0, 0, 0 } }, /* active low */
	},
      ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
      Package () {
	Package () { "port@0", "PRT0" },
      },
    })
  Name (PRT0, Package() {
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
            Package () {
                Package () { "reg", 0 },
            },
            ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
            Package () {
                Package () { "endpoint@0", "EP00" },
            }
  })
  Name (EP00, Package() {
	ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	Package () {
		Package () { "reg", 0 },
		/* remote-endpoint → USBC2 (i2c7/PD00) connector port@2 */
		Package () { "remote-endpoint", Package() { USBC2_PD_DEVICE, "connector", "port@2", "endpoint@0" } },
    }
  })
  }


  Device (GSA1) {
    Name (_HID, "CIXH200F")
    Name (_UID, 0x01)
    Name (_STA, 0x0F)

    Name (_CRS, ResourceTemplate () {
	/* FCH GPIO0 3 - matches DTS dp_4_sbu_mux */
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX0", 0, "pinctrl_gpio_dp4_sbu_aux", ResourceConsumer,)
      GpioIo (Exclusive, PullNone, 0, 0, IoRestrictionOutputOnly,
      "\\_SB.GPI0", 0, ResourceConsumer) { 3 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () { "compatible", "gpio-sbu-mux" },
        Package () { "orientation-switch", 0 },
	Package () { "select-gpio", Package () { ^GSA1, 0, 0, 0 } }, /* active low */
	},
      ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
      Package () {
	Package () { "port@0", "PRT0" },
      },
    })
  Name (PRT0, Package() {
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
            Package () {
                Package () { "reg", 0 },
            },
            ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
            Package () {
                Package () { "endpoint@0", "EP00" },
            }
  })
  Name (EP00, Package() {
	ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	Package () {
		Package () { "reg", 0 },
		/* remote-endpoint → USBC3 (i2c0/PD01) connector port@2 */
		Package () { "remote-endpoint", Package() { USBC3_PD_DEVICE, "connector", "port@2", "endpoint@0" } },
    }
  })
  }
