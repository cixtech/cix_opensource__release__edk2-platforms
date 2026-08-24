/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

External (\_SB.I2C3, DeviceObj)

Scope (\_SB.I2C3)
{
  // RTC RX8900
  Device (RTC0) {
    Name (_HID, "PRP0001")
    Name (_UID, 0x3)
    Name (_STA, 0xB)
    Name (_CRS, ResourceTemplate () {
      I2cSerialBusV2 (0x32,
                      ControllerInitiated,
                      400000,
                      AddressingMode7Bit,
                      "\\_SB.I2C3",
                      0x0,
                      ResourceConsumer,
                      ,
                      Exclusive
                      ,)
      PinGroupFunction(Exclusive, 0x0, "\\_SB.MUX1", 0,
                      "pinctrl_ra8900ce_irq", ResourceConsumer,)
      GpioInt(Level, ActiveLow, Exclusive, PullUp, , "\\_SB.GPI4") { 10 }
    })

    Name (_DSD, Package () {
      ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package () { "compatible", "rtc,rx8900" },
      }
    })
  }
}
