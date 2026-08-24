/** @file
   Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

   SPDX-License-Identifier: BSD-2-Clause-Patent
 **/

#ifndef _CDNSP_DXE_H_
#define _CDNSP_DXE_H_

#include "Cdnsp.h"

typedef struct {
  UINT64    RcsuBase;
  UINT64    ControlBase;
  UINT64    Phy3Address;
  UINT64    Phy2Address;
} CDNSP_DEVICE_INFO;

typedef enum {
  TYPE_CLOCK = 0,
  TYPE_RESET,
  TYPE_REGISTER,
  TYPE_PHY_REGISTER
} CDNSP_CONF_TYPE;

typedef struct {
  UINT64             Reg;
  UINT64             Value;
  CDNSP_CONF_TYPE    Type;
} CDNSP_CONF;

#define FN_MESSAGE_NODE_COUNT       8
#define USB_DESCRIPTION_MAX_INDEX   5
#define CDNSP_CONTROLLER_SIGNATURE  SIGNATURE_32 ('C', 'D', 'N', 'S', 'P')

typedef struct _FN_MESSAGE_NODE {
  LIST_ENTRY                   Link;
  EFI_USBFN_MESSAGE            Type;
  EFI_USBFN_MESSAGE_PAYLOAD    Playload;
} FN_MESSAGE_NODE;

typedef struct _CDNSP_FN_DEV {
  UINTN                              Signature;
  EFI_HANDLE                         Controller;
  EFI_USBFN_IO_PROTOCOL              UsbFnIo;
  EFI_EVENT                          ExitBootServiceEvent;
  EFI_USB_DEVICE_CONTROL_PROTOCOL    *UsbDeviceControlProtocol;
  CDNSP_DEVICE                       CdnspDevice;
  LIST_ENTRY                         FnMessages;
  LIST_ENTRY                         FnMessagePool;
  UINTN                              DeviceInfoIndex;
  BOOLEAN                            ControlStatus;
} CDNSP_FN_DEV;
#endif
