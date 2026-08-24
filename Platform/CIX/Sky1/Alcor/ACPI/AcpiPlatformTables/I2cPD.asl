
/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

External (\_SB.I2C0, DeviceObj)
External (\_SB.I2C1, DeviceObj)
External (\_SB.I2C7, DeviceObj)
External (\_SB.SUB2.CUB2, DeviceObj)
External (\_SB.UCP2, DeviceObj)
External (\_SB.SUB3.CUB3, DeviceObj)
External (\_SB.UCP3, DeviceObj)
External (\_SB.SUB0.CUB0, DeviceObj)
External (\_SB.UCP0, DeviceObj)
External (\_SB.SUB1.CUB1, DeviceObj)
External (\_SB.UCP1, DeviceObj)
External (\_SB.UVS0, DeviceObj) /* USBC0 VBUS supply */
External (\_SB.UVS1, DeviceObj) /* USBC1 VBUS supply */
External (\_SB.DP00, DeviceObj) /* DP0 */
External (\_SB.DP01, DeviceObj) /* DP1 */
External (\_SB.DP03, DeviceObj) /* DP3 */
External (\_SB.DP04, DeviceObj) /* DP4 */
External (\_SB.GSA0, DeviceObj) /* DP3 GPIO Aux Mux */
External (\_SB.GSA1, DeviceObj) /* DP4 GPIO Aux Mux */

#define DP_USBC_CON_DSD(Name) \
        ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),\
        Package () { \
            Package () { Name, "UC00" },\
        }

#define DP_USBC_CON_NODES(DevRefUsbRole,DevRefUsbOriSwitch,DevRefDpAltMux) \
  Name (UC00, Package() {\
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
            Package () {\
                Package () { "data-role", "host" }, \
                Package () { "power-role", "source" }, \
                Package () { "try-power-role", "source" }, \
            },\
            ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),\
            Package () {\
                Package () { "port@0", "PRT0" },\
                Package () { "port@1", "PRT1" },\
                Package () { "port@2", "PRT2" },\
            }\
  })\
  Name (PRT0, Package() {\
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
            Package () {\
                Package () { "reg", 0 }, \
            },\
            ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),\
            Package () {\
                Package () { "endpoint@0", "EP00" },\
            }\
  })\
  Name (EP00, Package() {\
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
            Package () {\
                Package () { "reg", 0 }, \
                Package () { "remote-endpoint", Package() { DevRefUsbRole, "port@0", "endpoint@0" } },\
            }\
  }) \
  Name (PRT1, Package() {\
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
            Package () {\
                Package () { "reg", 1 }, \
            },\
            ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),\
            Package () {\
                Package () { "endpoint@0", "EP01" },\
            }\
  })\
  Name (EP01, Package() {\
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
            Package () {\
                Package () { "reg", 0 }, \
                Package () { "remote-endpoint", Package() { DevRefUsbOriSwitch, "port@0", "endpoint@0" } },\
            }\
  }) \
  Name (PRT2, Package() {\
	    ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
	    Package () {\
		Package () { "reg", 2 }, \
	    },\
	    ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),\
	    Package () {\
		Package () { "endpoint@1", "EP02" },\
	    }\
  })\
  Name (EP02, Package() {\
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),\
            Package () {\
                Package () { "reg", 1 }, \
                Package () { "remote-endpoint", Package() { DevRefDpAltMux, "port@0", "endpoint@1" } },\
            }\
  })

Scope (\_SB.I2C1)
{
  Device (PD10) {
    Name (_HID, "CIXH200D")
    Name (_UID, 0x0)
    Name (_STA, 0xF)
    Name (_CRS, ResourceTemplate () {
      I2cSerialBusV2 (0x30,
                      ControllerInitiated,
                      100000,
                      AddressingMode7Bit,
                      "\\_SB.I2C1",
                      0x0,
                      ResourceConsumer,
                      ,
                      Exclusive
                      ,)
      GpioInt(Level, ActiveLow, Exclusive, PullUp, , "\\_SB.GPI4") { 9 }
    })
    Name (_DSD, Package () {
          ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
              Package () {"id", 0},
          },
          DP_USBC_CON_DSD("usbc_con0")
    })
    DP_USBC_CON_NODES(\_SB.SUB0.CUB0, \_SB.UCP0, \_SB.UCP0)
  }

  Device (PD11) {
    Name (_HID, "CIXH200D")
    Name (_UID, 0x1)
    Name (_STA, 0xF)
    Name (_CRS, ResourceTemplate () {
      I2cSerialBusV2 (0x31,
                      ControllerInitiated,
                      100000,
                      AddressingMode7Bit,
                      "\\_SB.I2C1",
                      0x0,
                      ResourceConsumer,
                      ,
                      Exclusive
                      ,)
      GpioInt(Level, ActiveLow, Exclusive, PullUp, , "\\_SB.GPI4") { 9 }
    })
    Name (_DSD, Package () {
          ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
              Package () {"id", 1},
          },
          DP_USBC_CON_DSD("usbc_con1")
    })
    DP_USBC_CON_NODES(\_SB.SUB1.CUB1, \_SB.UCP1, \_SB.UCP1)
  }
}

Scope (\_SB.I2C7)
{
  Device (PD00) {
    Name (_HID, "CIXH200E")
    Name (_UID, 0x0)
    Name (_STA, 0xF)
    Name (_CRS, ResourceTemplate () {
      I2cSerialBusV2 (0x4e,
                      ControllerInitiated,
                      100000,
                      AddressingMode7Bit,
                      "\\_SB.I2C7",
                      0x0,
                      ResourceConsumer,
                      ,
                      Exclusive
                      ,)
      /* S5 GPIO0 7 */
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX1", 0, "pinctrl_usbc2_pd_int", ResourceConsumer,)
      GpioIo (Exclusive, PullNone, 0, 0, IoRestrictionOutputOnly,
      "\\_SB.GPI4", 0, ResourceConsumer) { 7 }
      GpioInt(Level, ActiveLow, Exclusive, PullUp, , "\\_SB.GPI4") { 7 }
    })
    Name (_DSD, Package () {
          ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
	    Package () { "vbus-supply", \_SB.UVS0 },
          },
          ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
          Package () {
                Package () { "port@0", "PRT0" },
                Package () { "connector", "UC00" },
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
	  },
    })
  Name (EP00, Package() {
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
            Package () {
                Package () { "reg", 0 },
                Package () { "remote-endpoint", Package() { \_SB.SUB2.CUB2, "port@0", "endpoint@0" } },
            },
  })
    Name (UC00, Package() {
	  ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	  Package () {
	      Package () { "label", "USB-C" },
	      /*
	       * USBC2 is host/source only (no DRD/DRP). Advertising dual lets
	       * docks PR_SWAP/DR_SWAP the port to sink/device and tear down DP.
	       */
	      Package () { "data-role", "host" },
	      Package () { "power-role", "source" },
	      Package () { "typec-power-opmode", "3.0A" },
	      Package () { "self-powered", 0 },
	      Package () { "displayport", \_SB.DP03 },
	      Package () { "source-pdos", 0x0401912c }, /* PDO_FIXED(5000, 3000, PDO_FIXED_USB_COMM) */
	  },
	  ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	  Package () {
		Package () { "altmodes", "ALMS" },
		Package () { "port@1", "PRT1" },
		Package () { "port@2", "PRT2" },
	  },
    })
    Name (ALMS, Package() {
	  ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	  Package () {
	      Package () { "altmode@0", "ALM0" },
	  },
    })
    Name (ALM0, Package() {
	  ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	  Package () {
	      Package () { "reg", 0 },
	      Package () { "svid", 0xff01 },
	      Package () { "vdo", 0xffffffff },
	  },
    })
  Name (PRT1, Package() {
	    ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	    Package () {
		Package () { "reg", 1 },
	    },
	    ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	    Package () {
		Package () { "endpoint@0", "EP01" },
	    }
  })
    Name (EP01, Package() {
	ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	Package () {
		Package () { "reg", 0 },
		Package () { "remote-endpoint", Package() { \_SB.UCP2, "port@0", "endpoint@0" } },
	},
  })
  Name (PRT2, Package() {
	    ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	    Package () {
		Package () { "reg", 2 },
	    },
	    ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	    Package () {
		Package () { "endpoint@0", "EP02" },
	    }
  })
    Name (EP02, Package() {
	ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	Package () {
		Package () { "reg", 0 },
		Package () { "remote-endpoint", Package() { \_SB.GSA0, "port@0", "endpoint@0" } },
	},
  })

  } /* Device (PD00) */

	Name (DLKL, Package() {
	  Package() {\_SB.UVS0 , \_SB.I2C7, 0},
	})
} /* \_SB.I2C7 */

Scope (\_SB.I2C0)
{
  Device (PD01) {
    Name (_HID, "CIXH200E")
    Name (_UID, 0x1)
    Name (_STA, 0xF)
    Name (_CRS, ResourceTemplate () {
      I2cSerialBusV2 (0x4e,
                      ControllerInitiated,
                      100000,
                      AddressingMode7Bit,
                      "\\_SB.I2C0",
                      0x0,
                      ResourceConsumer,
                      ,
                      Exclusive
                      ,)
      /* S5 GPIO0 8 */
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX1", 0, "pinctrl_usbc3_pd_int", ResourceConsumer,)
      GpioIo (Exclusive, PullNone, 0, 0, IoRestrictionOutputOnly,
      "\\_SB.GPI4", 0, ResourceConsumer) { 8 }
      GpioInt(Level, ActiveLow, Exclusive, PullUp, , "\\_SB.GPI4") { 8 }
    })
    Name (_DSD, Package () {
          ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
	    Package () { "vbus-supply", \_SB.UVS1 },
          },
          ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
          Package () {
                Package () { "port@0", "PRT0" },
                Package () { "connector", "CON0" },
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
	  },
    })
  Name (EP00, Package() {
            ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
            Package () {
                Package () { "reg", 0 },
                Package () { "remote-endpoint", Package() { \_SB.SUB3.CUB3, "port@0", "endpoint@0" } },
            },
  })
    Name (CON0, Package() {
	  ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	  Package () {
	      Package () { "label", "USB-C" },
	      Package () { "data-role", "host" },
	      Package () { "power-role", "source" },
	      Package () { "typec-power-opmode", "3.0A" },
	      Package () { "self-powered", 0 },
	      Package () { "displayport", \_SB.DP04 },
	      Package () { "source-pdos", 0x401912c }, /* PDO_FIXED(5000, 3000, PDO_FIXED_USB_COMM) */
	  },
	  ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	  Package () {
		Package () { "altmodes", "ALMS" },
		Package () { "port@1", "PRT1" },
		Package () { "port@2", "PRT2" },
	  },
    })
    Name (ALMS, Package() {
	  ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	  Package () {
	      Package () { "altmode@0", "ALM0" },
	  },
    })
    Name (ALM0, Package() {
	  ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	  Package () {
	      Package () { "reg", 0 },
	      Package () { "svid", 0xff01 },
	      Package () { "vdo", 0xffffffff },
	  },
    })
  Name (PRT1, Package() {
	    ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	    Package () {
		Package () { "reg", 1 },
	    },
	    ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	    Package () {
		Package () { "endpoint@0", "EP01" },
	    }
  })
    Name (EP01, Package() {
	ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	Package () {
		Package () { "reg", 0 },
		Package () { "remote-endpoint", Package() { \_SB.UCP3, "port@0", "endpoint@0" } },
	},
  })
  Name (PRT2, Package() {
	    ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	    Package () {
		Package () { "reg", 2 },
	    },
	    ToUUID("dbb8e3e6-5886-4ba6-8795-1319f52a966b"),
	    Package () {
		Package () { "endpoint@0", "EP02" },
	    }
  })
    Name (EP02, Package() {
	ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
	Package () {
		Package () { "reg", 0 },
		Package () { "remote-endpoint", Package() { \_SB.GSA1, "port@0", "endpoint@0" } },
	},
  })

  } /* Device (PD01) */

	Name (DLKL, Package() {
	  Package() {\_SB.UVS1 , \_SB.I2C0, 0},
	})
} /* \_SB.I2C0 */
