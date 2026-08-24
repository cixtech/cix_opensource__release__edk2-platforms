/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

DefinitionBlock("SsdtTable.aml", "SSDT", 5, "CIXTEK", "SKY1EDK2", 1) {
  Scope(_SB) {
    include("Audio.asl")
    include("Rtc.asl")
    include("Iomux.asl")
    include("I2cPD.asl")
    include("UsbVbusPwr.asl")
    include("GpioSbuAux.asl")
    include("PwrBtn.asl")
    include("HardwareMonitor.asl")
  }
}
