/** CdnspDxe.c

  CdnspDxe driver APIs for read, write, initialize, set speed and reset

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "CdnspDxe.h"
#include <Protocol/ResetId.h>
#include <Protocol/ClockId.h>
#include <UsbDpPhyRegister.h>
#include <Protocol/ConfigParamsManageProtocol.h>
#include <Protocol/PdProtocol.h>
#include "FastbootFlashUpdate.h"
#include <Library/DebugLib.h>

STATIC
CDNSP_FN_DEV
*gCdnspFnDev = NULL;

extern USB_STD_DEV  gUsbDevice;
#define MAX_SERIAL_NUM  64
STATIC CHAR8  SerialNumStr[MAX_SERIAL_NUM] = "";
STATIC
CIX_CLOCK_PROTOCOL  *ClockProtocol = NULL;

STATIC
CIX_RESET_PROTOCOL  *ResetProtocol = NULL;

STATIC
USB_DEVICE_DESCRIPTOR
  CdnspDeviceDesc = {
  .Length            = sizeof (EFI_USB_DEVICE_DESCRIPTOR),
  .DescriptorType    = USB_DESC_TYPE_DEVICE,
  .BcdUSB            = 0x0200,
  .DeviceClass       = USB_CLASS_PER_INTERFACE,
  .DeviceSubClass    = 0,
  .DeviceProtocol    = 0,
  .MaxPacketSize0    = 64,
  .IdVendor          = 0x35b1,
  .IdProduct         = 0xa4a5,
  .BcdDevice         = 0x02,
  .StrManufacturer   = 1,
  .StrProduct        = 2,
  .StrSerialNumber   = 3,
  .NumConfigurations = 1,
};

#ifdef FPGA_BOARD
#define DEV_SOF_CLOCK_FREQUENCY  4000   // 4Mhz fpga setting
#define DEV_LPM_CLOCK_FREQUENCY  4000   // 4Mhz fpga setting
#else
#define DEV_SOF_CLOCK_FREQUENCY  8000   // 4Mhz emu setting
#define DEV_LPM_CLOCK_FREQUENCY  32     // 32k  setting
#endif

#define DEV_SOF_CLOCK_250_NS  250 * DEV_SOF_CLOCK_FREQUENCY /1000000 - 1
#define DEV_SOF_CLOCK_1_US    1000 * DEV_SOF_CLOCK_FREQUENCY /1000000 - 1
#define DEV_SOF_CLOCK_10_US   10000 * DEV_SOF_CLOCK_FREQUENCY /1000000 - 1
#define DEV_SOF_CLOCK_100_US  100000 * DEV_SOF_CLOCK_FREQUENCY /1000000 - 1
#define DEV_SOF_CLOCK_125_US  125000 * DEV_SOF_CLOCK_FREQUENCY /1000000 - 1
#define DEV_SOF_CLOCK_1_MS    1000000 /1000000 * DEV_SOF_CLOCK_FREQUENCY  - 1
#define DEV_SOF_CLOCK_10_MS   10000000 /1000000 * DEV_SOF_CLOCK_FREQUENCY  - 1
#define DEV_SOF_CLOCK_100_MS  100000000 /1000000 * DEV_SOF_CLOCK_FREQUENCY  - 1

#define DEV_LPM_CLOCK_250_NS  250 * DEV_LPM_CLOCK_FREQUENCY /1000000 - 1
#define DEV_LPM_CLOCK_1_US    1000 * DEV_LPM_CLOCK_FREQUENCY /1000000 - 1
#define DEV_LPM_CLOCK_10_US   10000 * DEV_LPM_CLOCK_FREQUENCY /1000000 - 1
#define DEV_LPM_CLOCK_100_US  100000 * DEV_LPM_CLOCK_FREQUENCY /1000000 - 1
#define DEV_LPM_CLOCK_125_US  125000 * DEV_LPM_CLOCK_FREQUENCY /1000000 - 1
#define DEV_LPM_CLOCK_1_MS    1000000 /1000000 * DEV_LPM_CLOCK_FREQUENCY  - 1
#define DEV_LPM_CLOCK_10_MS   10000000 /1000000 * DEV_LPM_CLOCK_FREQUENCY  - 1
#define DEV_LPM_CLOCK_100_MS  100000000 /1000000 * DEV_LPM_CLOCK_FREQUENCY  - 1

CONST
UINT8
  Str0Des[4] =
{
  sizeof (Str0Des),
  USB_DESC_TYPE_STRING,
  0x09, 0x04
};

CONST
UINT8
  StrManufacturerDesc[8] =
{
  sizeof (StrManufacturerDesc),
  USB_DESC_TYPE_STRING,
  'c',                         0,
  'i',                         0,
  'x',                         0,
};

CONST
UINT8
  StrProductDesc[14] =
{
  sizeof (StrProductDesc),
  USB_DESC_TYPE_STRING,
  's',                    0,
  'k',                    0,
  'y',                    0,
  'o',                    0,
  'n',                    0,
  'e',                    0,
};

UINT8
  StrSerialNumber[34] =
{
  sizeof (StrSerialNumber),
  USB_DESC_TYPE_STRING,
  '1',                     0,
  '2',                     0,
  '0',                     0,
  '8',                     0,
  '3',                     0,
  '1',                     0,
  'd',                     0,
  '6',                     0,
  'f',                     0,
  '6',                     0,
  'c',                     0,
  '9',                     0,
  '9',                     0,
  'e',                     0,
  'c',                     0,
  'b',                     0,
};

CONST
UINT8
  StrInterfaceDesc[18] =
{
  sizeof (StrInterfaceDesc),
  USB_DESC_TYPE_STRING,
  'f',                      0,
  'a',                      0,
  's',                      0,
  't',                      0,
  'b',                      0,
  'o',                      0,
  'o',                      0,
  't',                      0,
};

EFI_USB_STRING_DESCRIPTOR  *StrDes[USB_DESCRIPTION_MAX_INDEX] =
{
  (EFI_USB_STRING_DESCRIPTOR *)Str0Des,
  (EFI_USB_STRING_DESCRIPTOR *)StrManufacturerDesc,
  (EFI_USB_STRING_DESCRIPTOR *)StrProductDesc,
  (EFI_USB_STRING_DESCRIPTOR *)StrSerialNumber,
  (EFI_USB_STRING_DESCRIPTOR *)StrInterfaceDesc,
};

#define DEVICE_INFO_MAX  3
STATIC
CDNSP_DEVICE_INFO
  gCdnspDeviceInfo[DEVICE_INFO_MAX] = {
  { 0x9000300, 0x9010000, 0x9030000, 0x9020000 }, // usb typec drd
  { 0x091c300, 0x91d0000, 0x9180000, 0x91f0000 }, // usb standrd A drd
  { 0x091c300, 0x91e0000, 0x9180000, 0x9200000 }  // usb standrd A drd
};

#define USBC_DRD_3PHY_BASE  0x9030000

STATIC
CDNSP_CONF  UsbTypecDrdConf[] = {
  { USBC_SS0_PRST_N,              0,                    TYPE_RESET        },
  { USBC_SS0_RST_N,               0,                    TYPE_RESET        },
  { CLK_TREE_USB3C_DRD_APB_GATE,  1,                    TYPE_CLOCK        },
  { CLK_TREE_USB3C_DRD_AXI_GATE,  1,                    TYPE_CLOCK        },
  { CLK_TREE_USB3C_DRD_CLK_SOF,   1,                    TYPE_CLOCK        },
  { CLK_TREE_USB3C_DRD_CLK_LPM,   1,                    TYPE_CLOCK        },
  { 0x16000424,                   0x2000,               TYPE_REGISTER     },
  { 0x09000310,                   0x33,                 TYPE_REGISTER     },
  { USBC_SS0_PRST_N,              1,                    TYPE_RESET        },
  { 0x09016174,                   0x80000000,           TYPE_REGISTER     },
  { 0x09016174,                   0x840105c2,           TYPE_REGISTER     },
  { 0x0901617c,                   0x7,                  TYPE_REGISTER     },
  { 0x09016174,                   0x040105c2,           TYPE_REGISTER     },
 #ifdef FPGA_BOARD
  { 0x09016040,                   0xa0031e03,           TYPE_REGISTER     },
 #endif
  { 0x090161E8,                   DEV_SOF_CLOCK_250_NS, TYPE_REGISTER     },
  { 0x090161Ec,                   DEV_SOF_CLOCK_1_US,   TYPE_REGISTER     },
  { 0x090161F0,                   DEV_SOF_CLOCK_10_US,  TYPE_REGISTER     },
  { 0x090161F4,                   DEV_SOF_CLOCK_100_US, TYPE_REGISTER     },
  { 0x090161F8,                   DEV_SOF_CLOCK_125_US, TYPE_REGISTER     },
  { 0x090161Fc,                   DEV_SOF_CLOCK_1_MS,   TYPE_REGISTER     },
  { 0x09016200,                   DEV_SOF_CLOCK_10_MS,  TYPE_REGISTER     },
  { 0x09016204,                   DEV_SOF_CLOCK_100_MS, TYPE_REGISTER     },
  { 0x09016208,                   DEV_LPM_CLOCK_250_NS, TYPE_REGISTER     },
  { 0x0901620c,                   DEV_LPM_CLOCK_1_US,   TYPE_REGISTER     },
  { 0x09016210,                   DEV_LPM_CLOCK_10_US,  TYPE_REGISTER     },
  { 0x09016214,                   DEV_LPM_CLOCK_100_US, TYPE_REGISTER     },
  { 0x09016218,                   DEV_LPM_CLOCK_125_US, TYPE_REGISTER     },
  { 0x0901621c,                   DEV_LPM_CLOCK_1_MS,   TYPE_REGISTER     },
  { 0x09016220,                   DEV_LPM_CLOCK_10_MS,  TYPE_REGISTER     },
  { 0x09016224,                   DEV_LPM_CLOCK_100_MS, TYPE_REGISTER     },
  { USBC_SS0_RST_N,               1,                    TYPE_RESET        },
  { USB_DP_PHY0_PRST_N,           0,                    TYPE_RESET        },
  { USB_DP_PHY0_RST_N,            0,                    TYPE_RESET        },
  { CLK_TREE_USB3C_DRD_PHY3_GATE, 1,                    TYPE_CLOCK        },
  { USB_DP_PHY0_PRST_N,           1,                    TYPE_RESET        },
 #ifdef PHYSICAL_PHY
  { 0,                            0,                    TYPE_PHY_REGISTER },
 #endif
  { USB_DP_PHY0_RST_N,            1,                    TYPE_RESET        },
  { USBPHY_HS4_PRST_N,            1,                    TYPE_RESET        }
};

STATIC
FN_MESSAGE_NODE *
AllocateMessage (
  VOID
  )
{
  FN_MESSAGE_NODE  *NewMsg;

  if (IsListEmpty (&gCdnspFnDev->FnMessagePool)) {
    return NULL;
  }

  NewMsg = (FN_MESSAGE_NODE *)GetFirstNode (&gCdnspFnDev->FnMessagePool);
  RemoveEntryList (&NewMsg->Link);
  InsertTailList (&gCdnspFnDev->FnMessages, &NewMsg->Link);

  return NewMsg;
}

EFI_STATUS
NotifyEvent (
  EFI_USBFN_MESSAGE   Event,
  USB_EP              *Ep,
  USB_REQUEST         *Request,
  USB_DEVICE_REQUEST  *CtrRequest
  )
{
  EFI_STATUS       Status = EFI_SUCCESS;
  FN_MESSAGE_NODE  *NewMsg;
  UINT8            Index = SS_CONFIG_INDEX;

  if (NULL == gCdnspFnDev) {
    goto ON_EXIT;
  }

  switch (Event) {
    case EfiUsbMsgBusEventDetach:
      NewMsg = AllocateMessage ();
      if (NULL == NewMsg) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "No buffer In Message Pool \n"));
        goto ON_EXIT;
      }

      NewMsg->Type = EfiUsbMsgBusEventDetach;
      break;
    case EfiUsbMsgBusEventReset:
      NewMsg = AllocateMessage ();
      if (NULL == NewMsg) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "No buffer In Message Pool \n"));
        goto ON_EXIT;
      }

      NewMsg->Type = EfiUsbMsgBusEventReset;
      break;
    case EfiUsbMsgBusEventSpeed:
      NewMsg = AllocateMessage ();
      if (NULL == NewMsg) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "No buffer In Message Pool \n"));
        goto ON_EXIT;
      }

      if (gCdnspFnDev->CdnspDevice.Speed == UsbBusSpeedFull) {
        Index = FS_CONFIG_INDEX;
      } else if (gCdnspFnDev->CdnspDevice.Speed == UsbBusSpeedHigh) {
        Index = HS_CONFIG_INDEX;
      } else {
        // using the default value
      }

      if (NULL != gCdnspFnDev->UsbDeviceControlProtocol) {
        gCdnspFnDev->UsbDeviceControlProtocol->ControlGetDes (
                                                 (VOID **)&gUsbDevice.ConfigDesc,
                                                 Index
                                                 );
      }

      NewMsg->Type         = EfiUsbMsgBusEventSpeed;
      NewMsg->Playload.ubs = gCdnspFnDev->CdnspDevice.Speed;
      break;
    case EfiUsbMsgSetupPacket:
      NewMsg = AllocateMessage ();
      if (NULL == NewMsg) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "No buffer In Message Pool \n"));
        goto ON_EXIT;
      }

      NewMsg->Type = EfiUsbMsgSetupPacket;
      gBS->CopyMem (&NewMsg->Playload.udr, CtrRequest, sizeof (EFI_USB_DEVICE_REQUEST));
      break;
    case EfiUsbMsgEndpointStatusChangedTx:
      NewMsg = AllocateMessage ();
      if (NULL == NewMsg) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "No buffer In Message Pool \n"));
        goto ON_EXIT;
      }

      NewMsg->Type                          = EfiUsbMsgEndpointStatusChangedTx;
      NewMsg->Playload.utr.BytesTransferred = Request->Actual;
      if (Request->Status == ENDPOINT_COMPLETE) {
        NewMsg->Playload.utr.TransferStatus = UsbTransferStatusComplete;
        NewMsg->Playload.utr.EndpointIndex  = ((CDNSP_EP *)Ep)->Number;
        NewMsg->Playload.utr.Direction      = EfiUsbEndpointDirectionDeviceTx;
        NewMsg->Playload.utr.Buffer         = Request->Buf;
        DEBUG ((
          DEBUG_TRB_LOG,
          "Transfer data size(%d), buffer address(0x%llx), ep index(%d), direction(%d)\n",
          NewMsg->Playload.utr.BytesTransferred,
          NewMsg->Playload.utr.Buffer,
          NewMsg->Playload.utr.EndpointIndex,
          NewMsg->Playload.utr.Direction,
          NewMsg->Playload.utr.BytesTransferred
          ));
      } else {
        NewMsg->Playload.utr.TransferStatus = UsbTransferStatusAborted;
      }

      break;
    case EfiUsbMsgEndpointStatusChangedRx:
      NewMsg = AllocateMessage ();
      if (NULL == NewMsg) {
        Status = EFI_OUT_OF_RESOURCES;
        DEBUG ((DEBUG_ERROR, "No buffer In Message Pool \n"));
        goto ON_EXIT;
      }

      NewMsg->Type                          = EfiUsbMsgEndpointStatusChangedRx;
      NewMsg->Playload.utr.BytesTransferred = Request->Actual;
      if (Request->Status == ENDPOINT_COMPLETE) {
        NewMsg->Playload.utr.TransferStatus = UsbTransferStatusComplete;
        NewMsg->Playload.utr.EndpointIndex  = ((CDNSP_EP *)Ep)->Number;
        NewMsg->Playload.utr.Direction      = EfiUsbEndpointDirectionDeviceRx;
        NewMsg->Playload.utr.Buffer         = Request->Buf;
        DEBUG ((
          DEBUG_TRB_LOG,
          "Receive data size(%d), buffer address(0x%llx), ep index(%d), direction(%d)\n",
          NewMsg->Playload.utr.BytesTransferred,
          NewMsg->Playload.utr.Buffer,
          NewMsg->Playload.utr.EndpointIndex,
          NewMsg->Playload.utr.Direction,
          NewMsg->Playload.utr.BytesTransferred
          ));
      } else {
        NewMsg->Playload.utr.TransferStatus = UsbTransferStatusAborted;
      }

      break;
    default:
      DEBUG ((DEBUG_ERROR, "UnSupport Notify Event(%r)\n", Event));
      goto ON_EXIT;
  }

  if ((Request != NULL) && (Request != ((CDNSP_EP *)Ep)->ResidentRequest)) {
    CdnspEpFreeRequest (Ep, Request);
  }

ON_EXIT:
  return Status;
}

/**
  Returns the maximum packet size of the specified endpoint type for the supplied
  bus speed.
  If the BusSpeed is UsbBusSpeedUnknown, the maximum speed the underlying controller
  supports is assumed.
  This protocol currently does not support isochronous or interrupt transfers. Future
  revisions of this protocol may eventually support it.
  @param[in]  This              A pointer to the EFI_USBFN_IO_PROTOCOLinstance.
  @param[in]  EndpointType      Endpoint type as defined as EFI_USB_ENDPOINT_TYPE.
  @param[in]  BusSpeed          Bus speed as defined as EFI_USB_BUS_SPEED.
  @param[out] MaxPacketSize     The maximum packet size, in bytes, of the specified
                                endpoint type.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_DEVICE_ERROR      The physical device reported an error.
  @retval EFI_NOT_READY         The physical device is busy or not ready to process
                                this request.
**/
#define USBFN_NUM_EP_TYPES    (USB_ENDPOINT_INTERRUPT + 1)
#define USBFN_NUM_BUS_SPEEDS  (UsbBusSpeedSuperPlus + 1)

CONST
UINTN
  UsbEpMaxPacketSize[USBFN_NUM_EP_TYPES][USBFN_NUM_BUS_SPEEDS] = {
  // unknown, low,    full,   high,   super
  { 0, 8, 64,   64,   512,  512  },                  // control
  { 0, 0, 1023, 1024, 1024, 1024 },                  // isoch
  { 0, 0, 64,   512,  1024, 1024 },                  // bulk
  { 0, 8, 64,   1024, 1024, 1024 }                   // interrupt
};

EFI_STATUS
EFIAPI
CdnspGetEndpointMaxPacketSize (
  IN   EFI_USBFN_IO_PROTOCOL  *This,
  IN   EFI_USB_ENDPOINT_TYPE  EndpointType,
  IN   EFI_USB_BUS_SPEED      BusSpeed,
  OUT  UINT16                 *MaxPacketSize
  )
{
  if ((NULL == This) || (NULL == MaxPacketSize) ||
      (((UINT8)EndpointType > USB_ENDPOINT_INTERRUPT) || (BusSpeed > UsbBusSpeedSuperPlus)))
  {
    return EFI_INVALID_PARAMETER;
  }

  *MaxPacketSize = UsbEpMaxPacketSize[EndpointType][BusSpeed];

  return EFI_SUCCESS;
}

/**
  This function handles transferring data to or from the host on the specified
  endpoint, depending on the direction specified.
  A class driver must call EFI_USBFN_IO_PROTOCOL.EventHandler() repeatedly to
  receive updates on the transfer status and the number of bytes transferred on
  various endpoints. Upon an update of the transfer status, the Buffer field of
  the EFI_USBFN_TRANSFER_RESULT structure (as described in the function description
  for EFI_USBFN_IO_PROTOCOL.EventHandler()) must be initialized with the Buffer
  pointer that was supplied to this method.
  The overview of the call sequence is illustrated in the Figure 54.
  This function should fail with EFI_INVALID_PARAMETER if the specified direction
  is incorrect for the endpoint.
  @param[in]      This          A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @param[in]      EndpointIndex Indicates the endpoint on which TX or RX transfer
                                needs to take place.
  @param[in]      Direction     Direction of the endpoint.
  @param[in, out] BufferSize    If Direction is EfiUsbEndpointDirectionDeviceRx:
                                  On input, the size of the Bufferin bytes.
                                  On output, the amount of data returned in Buffer
                                  in bytes.
                                If Direction is EfiUsbEndpointDirectionDeviceTx:
                                  On input, the size of the Bufferin bytes.
                                  On output, the amount of data transmitted in bytes.
  @param[in, out] Buffer        If Direction is EfiUsbEndpointDirectionDeviceRx:
                                  The Buffer to return the received data.
                                If Directionis EfiUsbEndpointDirectionDeviceTx:
                                  The Buffer that contains the data to be transmitted.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_DEVICE_ERROR      The physical device reported an error.
  @retval EFI_NOT_READY         The physical device is busy or not ready to process
                                this request.
**/
EFI_STATUS
EFIAPI
CdnspTransfer (
  IN     EFI_USBFN_IO_PROTOCOL         *This,
  IN     UINT8                         EndpointIndex,
  IN     EFI_USBFN_ENDPOINT_DIRECTION  Direction,
  IN OUT UINTN                         *BufferSize,
  IN OUT VOID                          *Buffer
  )
{
  EFI_STATUS  Status  = EFI_SUCCESS;
  UINT8       EpIndex = 0;

  if ((NULL == This) || (NULL == BufferSize) || (NULL == Buffer) || (EndpointIndex >= 16) || ((UINTN)Direction >= 2)) {
    DEBUG ((DEBUG_ERROR, "Invalid Parameter\n"));
    Status = EFI_INVALID_PARAMETER;
    goto ON_EXIT;
  }

  EpIndex = (EndpointIndex * 2) + (Direction ? 1 : 0) - 1;
  Status  = CdnspQueueTransfer (EpIndex, BufferSize, Buffer);

ON_EXIT:

  return Status;
}

/**
  This function supplies power to the USB controller if needed and initializes
  the hardware and the internal data structures. The port must not be activated
  by this function.
  @param[in]  This              A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_DEVICE_ERROR      The physical device reported an error.
**/
EFI_STATUS
EFIAPI
CdnspStartController (
  IN  EFI_USBFN_IO_PROTOCOL  *This
  )
{
  EFI_STATUS        Status     = EFI_SUCCESS;
  UINT32            ArrayIndex = 0;
  UINT32            RegIndex   = 0;
  UINT32            RegisterValue;
  CIX_FD_PROTOCOL   *PdProtocol;
  TYPEC_PORT_STATE  PortState;

  Status = gBS->LocateProtocol (
                  &gEfiUsbDeviceControlGuid,
                  NULL,
                  (VOID **)&gCdnspFnDev->UsbDeviceControlProtocol
                  );

  if (EFI_ERROR (Status)) {
    gCdnspFnDev->UsbDeviceControlProtocol = NULL;
    DEBUG ((DEBUG_ERROR, "LocateProtocol(UsbDeviceContorl) Failed Status = %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_COMMON_LOG, "Enter CdnspStartController\n"));

  if (gCdnspFnDev->ControlStatus == 1) {
    DEBUG ((DEBUG_ERROR, "try reopen usb device \n"));
    return Status;
  }

  if (gCdnspFnDev->DeviceInfoIndex == 0) {
    Status = gBS->LocateProtocol (
                    &gCixPdProtocolGuid,
                    NULL,
                    (VOID **)&PdProtocol
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a Locate Protocol %g %r\n", __FUNCTION__, &gCixPdProtocolGuid, Status));
      PortState.Orientation = TYPEC_ORIENTATION_NORMAL;
    } else {
      if (PdProtocol->GetPortState (0, &PortState) != EFI_SUCCESS) {
        PortState.Orientation = TYPEC_ORIENTATION_NORMAL;
      }
    }

    RegisterValue = MmioRead32 (RCSU_USB_TYPEC_CTRL_ADDRESS);

    if (PortState.Orientation == TYPEC_ORIENTATION_REVERSE) {
      RegisterValue = RegisterValue | BIT (0);
    } else {
      RegisterValue = RegisterValue & (~BIT (0));
    }

    MmioWrite32 (RCSU_USB_TYPEC_CTRL_ADDRESS, RegisterValue);
    DEBUG ((DEBUG_ERROR, "Set Oritation = %d  update register to %x\n", PortState.Orientation, RegisterValue));
    for (ArrayIndex = 0; ArrayIndex < sizeof (UsbTypecDrdConf) / sizeof (UsbTypecDrdConf[0]); ArrayIndex++) {
      if (UsbTypecDrdConf[ArrayIndex].Type == TYPE_RESET) {
        if (UsbTypecDrdConf[ArrayIndex].Value == 0) {
          ResetProtocol->ResetAssert (UsbTypecDrdConf[ArrayIndex].Reg);
        } else {
          ResetProtocol->ResetDeassert (UsbTypecDrdConf[ArrayIndex].Reg);
        }

        gBS->Stall (10);
      } else if (UsbTypecDrdConf[ArrayIndex].Type == TYPE_CLOCK) {
        if (UsbTypecDrdConf[ArrayIndex].Value == 0) {
          ClockProtocol->ClockDisable (UsbTypecDrdConf[ArrayIndex].Reg);
        } else {
          ClockProtocol->ClockEnable (UsbTypecDrdConf[ArrayIndex].Reg);
        }

        gBS->Stall (10);
      } else if (UsbTypecDrdConf[ArrayIndex].Type == TYPE_REGISTER) {
        if (UsbTypecDrdConf[ArrayIndex].Value >= 0xffffffff) {
          UsbTypecDrdConf[ArrayIndex].Value = 0;
        }

        MmioWrite32 (UsbTypecDrdConf[ArrayIndex].Reg, UsbTypecDrdConf[ArrayIndex].Value);
        gBS->Stall (10);
        DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", UsbTypecDrdConf[ArrayIndex].Reg, UsbTypecDrdConf[ArrayIndex].Value));
      } else if (TYPE_PHY_REGISTER == UsbTypecDrdConf[ArrayIndex].Type) {
        if (PortState.Orientation == TYPEC_ORIENTATION_REVERSE) {
          for (RegIndex = 0; RegIndex < sizeof (UsbDpFlipRegConf) / sizeof (UsbDpFlipRegConf[0]); RegIndex++) {
            MmioWrite32 ((UINTN)(USBC_DRD_3PHY_BASE + (UsbDpFlipRegConf[RegIndex].Address << 2)), UsbDpFlipRegConf[RegIndex].Value);
            DEBUG ((
              DEBUG_REGISTER_LOG,
              "dump reg:%llx:%x\n",
              (UINTN)(USBC_DRD_3PHY_BASE + (UsbDpFlipRegConf[RegIndex].Address << 2)),
              UsbDpFlipRegConf[RegIndex].Value
              ));
          }
        } else {
          for (RegIndex = 0; RegIndex < sizeof (UsbDpNormalRegConf) / sizeof (UsbDpNormalRegConf[0]); RegIndex++) {
            MmioWrite32 ((UINTN)(USBC_DRD_3PHY_BASE + (UsbDpNormalRegConf[RegIndex].Address << 2)), UsbDpNormalRegConf[RegIndex].Value);
            DEBUG ((
              DEBUG_REGISTER_LOG,
              "dump reg:%llx:%x\n",
              (UINTN)(USBC_DRD_3PHY_BASE + (UsbDpNormalRegConf[RegIndex].Address << 2)),
              UsbDpNormalRegConf[RegIndex].Value
              ));
          }
        }
      } else {
        DEBUG ((DEBUG_ERROR, "Invalid conf type \n"));
      }
    }
  }

  Status = CdnspInit (&gCdnspFnDev->CdnspDevice);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Cdnsp Init Failed(%r)\n", Status));
    return Status;
  }

 #ifdef FPGA_BOARD
  gBS->Stall (50);
  MmioWrite32 (0x0901003c, 0x0400);
  MmioWrite32 (0x0901003c, 0x0c00);
 #endif
  gBS->Stall (50);
  MmioWrite32(0x0901003c,0x0c00);
  InitUsbStdDev (&CdnspDeviceDesc, StrDes, USB_DESCRIPTION_MAX_INDEX, &gCdnspFnDev->CdnspDevice.StdDeviceOps);

  gCdnspFnDev->ControlStatus = 1;


  return Status;
}

/**InitUsbStdDev
  This function stops the USB hardware device.
  @param[in]  This              A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_DEVICE_ERROR      The physical device reported an error.
**/
EFI_STATUS
EFIAPI
CdnspStopController (
  IN  EFI_USBFN_IO_PROTOCOL  *This
  )
{
  LIST_ENTRY  *ListEntry;

  if (gCdnspFnDev->ControlStatus == 0) {
    DEBUG ((DEBUG_ERROR, "no need to double release related resource \n"));
    return EFI_SUCCESS;
  }

  CdnspDisableInterfaceEndpoint (&gCdnspFnDev->CdnspDevice);
  CdnspDeInit (&gCdnspFnDev->CdnspDevice);

  gBS->SetMem (&gCdnspFnDev->CdnspDevice, sizeof (gCdnspFnDev->CdnspDevice), 0);

  while (!IsListEmpty (&gCdnspFnDev->FnMessages)) {
    ListEntry = GetFirstNode (&gCdnspFnDev->FnMessages);
    RemoveEntryList (ListEntry);
    InsertTailList (&gCdnspFnDev->FnMessagePool, ListEntry);
  }

  gCdnspFnDev->ControlStatus = 0;
  return EFI_SUCCESS;
}

/**
  This function is called repeatedly to get information on USB bus states,
  receive-completion and transmit-completion events on the endpoints, and
  notification on setup packet on endpoint 0.
  A class driver must call EFI_USBFN_IO_PROTOCOL.EventHandler()repeatedly
  to receive updates on the transfer status and number of bytes transferred
  on various endpoints.
  @param[in]      This          A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @param[out]     Message       Indicates the event that initiated this notification.
  @param[in, out] PayloadSize   On input, the size of the memory pointed by
                                Payload. On output, the amount ofdata returned
                                in Payload.
  @param[out]     Payload       A pointer to EFI_USBFN_MESSAGE_PAYLOAD instance
                                to return additional payload for current message.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_DEVICE_ERROR      The physical device reported an error.
  @retval EFI_NOT_READY         The physical device is busy or not ready to process
                                this request.
  @retval EFI_BUFFER_TOO_SMALL  The Supplied buffer is not large enough to hold
                                the message payload.
**/
EFI_STATUS
EFIAPI
CdnspFnEventHandler (
  IN   EFI_USBFN_IO_PROTOCOL      *This,
  OUT  EFI_USBFN_MESSAGE          *Message,
  IN   OUT UINTN                  *PayloadSize,
  OUT  EFI_USBFN_MESSAGE_PAYLOAD  *Payload
  )
{
  LIST_ENTRY         *ListEntry =  NULL;
  EFI_USBFN_MESSAGE  NoneMsg    = EfiUsbMsgNone;
  EFI_USBFN_MESSAGE  *Msg       = NULL;
  FN_MESSAGE_NODE    *MsgNode   = NULL;
  VOID               *Src       = NULL;
  UINTN              SrcSize    = 0;
  EFI_STATUS         Status     = EFI_SUCCESS;

  if ((NULL == This) || (NULL == Message) || (NULL == PayloadSize) ||
      (NULL == Payload) || (*PayloadSize == 0))
  {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((DEBUG_ERROR, "Invalid Parameter \n"));
    goto ON_EXIT;
  }

  if (IsListEmpty (&gCdnspFnDev->FnMessages)) {
    Msg =  &NoneMsg;
  } else {
    ListEntry = GetFirstNode (&gCdnspFnDev->FnMessages);
    MsgNode   = (FN_MESSAGE_NODE *)ListEntry;
    Msg       = &MsgNode->Type;
    switch (MsgNode->Type) {
      case EfiUsbMsgEndpointStatusChangedRx:
        Src     = &(MsgNode->Playload.utr);
        SrcSize = sizeof (MsgNode->Playload.utr);
        break;
      case EfiUsbMsgEndpointStatusChangedTx:
        Src     = &(MsgNode->Playload.utr);
        SrcSize = sizeof (MsgNode->Playload.utr);
        break;
      case EfiUsbMsgSetupPacket:
        Src     = &(MsgNode->Playload.udr);
        SrcSize = sizeof (MsgNode->Playload.udr);
        break;
      case EfiUsbMsgBusEventReset:
        SrcSize = 0;
        break;
      case EfiUsbMsgBusEventDetach:
        SrcSize = 0;
        break;
      case EfiUsbMsgBusEventSpeed:
        Src     = &(MsgNode->Playload.ubs);
        SrcSize = sizeof (MsgNode->Playload.ubs);
        break;
      default:
        DEBUG ((DEBUG_ERROR, "UnSupport Event Message(%d)\n", MsgNode->Type));
    }
  }

  if (*Msg == EfiUsbMsgNone) {
    CdnspEventRingHandler (&gCdnspFnDev->CdnspDevice);
  } else {
    if (*PayloadSize < SrcSize) {
      Status = EFI_BUFFER_TOO_SMALL;
    } else if ( 0 == SrcSize) {
      // no need copy
    } else {
      gBS->CopyMem (Payload, Src, SrcSize);
    }
  }

  *Message = *Msg;
  if (SrcSize) {
    *PayloadSize = sizeof (*Payload);
  } else {
    *PayloadSize = 0;
  }

ON_EXIT:
  if (ListEntry) {
    //
    // Remove the message from the active
    // message list and put it back to the
    // memory pool
    //
    RemoveEntryList (ListEntry);
    InsertTailList (&gCdnspFnDev->FnMessagePool, ListEntry);
  }

  return Status;
}

/**
  Allocates a transfer buffer of the specified sizethat satisfies the controller
  requirements.
  The AllocateTransferBuffer() function allocates a memory region of Size bytes and
  returns the address of the allocated memory that satisfies the underlying controller
  requirements in the location referenced by Buffer.
  The allocated transfer buffer must be freed using a matching call to
  EFI_USBFN_IO_PROTOCOL.FreeTransferBuffer()function.
  @param[in]  This              A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @param[in]  Size              The number of bytes to allocate for the transfer buffer.
  @param[out] Buffer            A pointer to a pointer to the allocated buffer if the
                                call succeeds; undefined otherwise.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_OUT_OF_RESOURCES  The requested transfer buffer could not be allocated.
**/
EFI_STATUS
EFIAPI
CdnspAllocateTransferBuffer (
  IN   EFI_USBFN_IO_PROTOCOL  *This,
  IN   UINTN                  Size,
  OUT  VOID                   **Buffer
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (NULL == Buffer) {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((DEBUG_ERROR, "Buffer Is Null When Alloc Transfer Buffer\n"));
    goto ON_EXIT;
  }

  *Buffer = UncachedAllocateAlignedZeroPool (Size, 1024);
  if (*Buffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "Out Of Memory"));
    goto ON_EXIT;
  }

  DEBUG ((DEBUG_COMMON_LOG, "Transfer Buffer addr(0x%llx). Transfer Buffer Size(%d) \n", *Buffer, Size));

ON_EXIT:
  return Status;
}

/**
  Deallocates the memory allocated for the transfer buffer by the
  EFI_USBFN_IO_PROTOCOL.AllocateTransferBuffer() function.
  The EFI_USBFN_IO_PROTOCOL.FreeTransferBuffer() function deallocates the
  memory specified by Buffer. The Buffer that is freed must have been allocated
  by EFI_USBFN_IO_PROTOCOL.AllocateTransferBuffer().
  @param[in]  This              A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @param[in]  Buffer            A pointer to the transfer buffer to deallocate.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
**/
EFI_STATUS
EFIAPI
CdnspFreeTransferBuffer (
  IN  EFI_USBFN_IO_PROTOCOL  *This,
  IN  VOID                   *Buffer
  )
{
  if (NULL != Buffer) {
    UncachedSafeFreePool (Buffer);
  } else {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  Returns the vendor-id and product-id of the device.
  @param[in]  This              A pointer to the EFI_USBFN_IO_PROTOCOL instance.
  @param[out] Vid               Returned vendor-id of the device.
  @param[out] Pid               Returned product-id of the device.
  @retval EFI_SUCCESS           The function returned successfully.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         Unable to return the vendor-id or the product-id.
**/
EFI_STATUS
EFIAPI
CdnspGetIds (
  IN   EFI_USBFN_IO_PROTOCOL  *This,
  OUT  UINT16                 *Vid,
  OUT  UINT16                 *Pid
  )
{
  if ((NULL == This) || (NULL == Vid) || (NULL == Pid)) {
    DEBUG ((DEBUG_ERROR, "Invalid Parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  *Vid = CdnspDeviceDesc.IdVendor;
  *Pid = CdnspDeviceDesc.IdProduct;

  return EFI_SUCCESS;
}

STATIC
CDNSP_FN_DEV *
CreateCdnspFnDev (
  VOID
  )
{
  CDNSP_FN_DEV     *CdnspFnDev = NULL;
  UINTN            LoopCount   = 0;
  FN_MESSAGE_NODE  *NewMsg     = NULL;
  FN_MESSAGE_NODE  *MsgNode    = NULL;

  if (NULL != gCdnspFnDev) {
    return gCdnspFnDev;
  }

  CdnspFnDev = (CDNSP_FN_DEV *)AllocateZeroPool (sizeof (CDNSP_FN_DEV));
  if (NULL == CdnspFnDev) {
    return NULL;
  }

  CdnspFnDev->UsbFnIo.Revision                 = EFI_USBFN_IO_PROTOCOL_REVISION;
  CdnspFnDev->UsbFnIo.DetectPort               = NULL;
  CdnspFnDev->UsbFnIo.ConfigureEnableEndpoints = NULL;
  CdnspFnDev->UsbFnIo.GetEndpointMaxPacketSize = CdnspGetEndpointMaxPacketSize;
  CdnspFnDev->UsbFnIo.GetDeviceInfo            = NULL;
  CdnspFnDev->UsbFnIo.GetVendorIdProductId     = CdnspGetIds;
  CdnspFnDev->UsbFnIo.AbortTransfer            = NULL;
  CdnspFnDev->UsbFnIo.GetEndpointStallState    = NULL;
  CdnspFnDev->UsbFnIo.SetEndpointStallState    = NULL;
  CdnspFnDev->UsbFnIo.EventHandler             = CdnspFnEventHandler;
  CdnspFnDev->UsbFnIo.Transfer                 = CdnspTransfer;
  CdnspFnDev->UsbFnIo.GetMaxTransferSize       = NULL;
  CdnspFnDev->UsbFnIo.AllocateTransferBuffer   = CdnspAllocateTransferBuffer;
  CdnspFnDev->UsbFnIo.FreeTransferBuffer       = CdnspFreeTransferBuffer;
  CdnspFnDev->UsbFnIo.StartController          = CdnspStartController;
  CdnspFnDev->UsbFnIo.StopController           = CdnspStopController;
  CdnspFnDev->UsbFnIo.SetEndpointPolicy        = NULL;
  CdnspFnDev->UsbFnIo.GetEndpointPolicy        = NULL;

  InitializeListHead (&CdnspFnDev->FnMessages);
  InitializeListHead (&CdnspFnDev->FnMessagePool);
  for (LoopCount = 0; LoopCount < FN_MESSAGE_NODE_COUNT; LoopCount++) {
    NewMsg = (FN_MESSAGE_NODE *)AllocateZeroPool (sizeof (FN_MESSAGE_NODE));
    if (NULL != NewMsg) {
      InsertTailList (&CdnspFnDev->FnMessagePool, &NewMsg->Link);
    } else {
      goto ERROR_EXIT;
    }
  }

  return CdnspFnDev;
ERROR_EXIT:
  while (!IsListEmpty (&gCdnspFnDev->FnMessages)) {
    MsgNode = (FN_MESSAGE_NODE *)GetFirstNode (&gCdnspFnDev->FnMessages);
    RemoveEntryList (&MsgNode->Link);
    FreePool (MsgNode);
  }

  FreePool (CdnspFnDev);

  return NULL;
}

STATIC
VOID
CleanCdnspFnDev (
  VOID
  )
{
  FN_MESSAGE_NODE  *MsgNode = NULL;

  while (!IsListEmpty (&gCdnspFnDev->FnMessagePool)) {
    MsgNode = (FN_MESSAGE_NODE *)GetFirstNode (&gCdnspFnDev->FnMessagePool);
    RemoveEntryList (&MsgNode->Link);
    FreePool (MsgNode);
  }

  while (!IsListEmpty (&gCdnspFnDev->FnMessages)) {
    MsgNode = (FN_MESSAGE_NODE *)GetFirstNode (&gCdnspFnDev->FnMessages);
    RemoveEntryList (&MsgNode->Link);
    FreePool (MsgNode);
  }

  if (NULL != gCdnspFnDev) {
    FreePool (gCdnspFnDev);
  }

  gCdnspFnDev = NULL;
}

VOID
EFIAPI
CdnspDxeExitBootService (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  DEBUG ((DEBUG_INFO, "%a\n",__FUNCTION__));

  CdnspStopController (&((CDNSP_FN_DEV *)Context)->UsbFnIo);

  Status = gBS->UninstallProtocolInterface (
                  &((CDNSP_FN_DEV *)Context)->Controller,
                  &gEfiUsbfnIoProtocolGuid,
                  &((CDNSP_FN_DEV *)Context)->UsbFnIo
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Uninstall Protocol(UsbFnIo) Failled\n"));
  }

  CleanCdnspFnDev ();
}

EFI_STATUS
EFIAPI
CdnspEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                         Status = EFI_SUCCESS;
  CONFIG_PARAMS_DATA_BLOCK           *ConfigData;
  CIX_CONFIG_PARAMS_MANAGE_PROTOCOL  *ConfigManage;
  UINT32                             BufferSize;
  EFI_USB_STRING_DESCRIPTOR          *SerialNumberStringDesc;

  Status = GetSerialNum (SerialNumStr, sizeof (SerialNumStr));
  BufferSize = AsciiStrSize (SerialNumStr)*2;
  SerialNumberStringDesc = (EFI_USB_STRING_DESCRIPTOR *)AllocateZeroPool (BufferSize);
  SerialNumberStringDesc->DescriptorType = USB_DESC_TYPE_STRING;
  SerialNumberStringDesc->Length         = BufferSize;
  AsciiStrToUnicodeStrS (SerialNumStr + 2, SerialNumberStringDesc->String, AsciiStrSize (SerialNumStr) - 2);
  StrDes[3] = SerialNumberStringDesc;
  DebugPrint (DEBUG_ERROR,"[JK] enter CdnspEntry\n");
  POST_CODE (CdnspDxeStart);

  Status = gBS->LocateProtocol (
                  &gCixConfigParamsManageProtocolGuid,
                  NULL,
                  (VOID **)&ConfigManage
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: config parameters invalid %r\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  gCdnspFnDev = CreateCdnspFnDev ();
  if (NULL == gCdnspFnDev) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "Memory Alloc Failed for CdnspFnDev\n"));
    goto ERROR_RESOURCE;
  }

  ConfigData                   = ConfigManage->Data;
  gCdnspFnDev->DeviceInfoIndex = DEVICE_INFO_MAX;

  if ((ConfigData->UsbCDrd[0].DataRole == 1) && ConfigData->UsbCDrd[0].Enable) {
    gCdnspFnDev->DeviceInfoIndex         = 0;
    gCdnspFnDev->CdnspDevice.MaxSpeed    = ConfigData->UsbCDrd[0].MaxSpeed;
    gCdnspFnDev->CdnspDevice.BaseAddress = gCdnspDeviceInfo[gCdnspFnDev->DeviceInfoIndex].ControlBase;
    DEBUG ((DEBUG_INFO, "Enable Usb Typec Drd 0\n"));
    goto ControlFind;
  }

  if (gCdnspFnDev->DeviceInfoIndex == DEVICE_INFO_MAX) {
    Status = EFI_UNSUPPORTED;
    goto ControlNotFind;
  }

ControlFind:

  Status = gBS->LocateProtocol (
                  &gCixResetProtocolGuid,
                  NULL,
                  (VOID **)&ResetProtocol
                  );

  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "%a: failed to get sky1 reset protocol (Status == %r)\n",
       __FUNCTION__, Status)
      );
    goto ERROR_RESOURCE;
  }

  Status = gBS->LocateProtocol (
                  &gCixClockProtocolGuid,
                  NULL,
                  (VOID **)&ClockProtocol
                  );

  if (EFI_ERROR (Status)) {
    DEBUG (
      (DEBUG_ERROR,
       "%a: failed to get sky1 clock protocol (Status == %r)\n",
       __FUNCTION__, Status)
      );
    goto ERROR_RESOURCE;
  }

  // Install UsbFnIo Protocol (Device Control Use)
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &gCdnspFnDev->Controller,
                  &gEfiUsbfnIoProtocolGuid,
                  &gCdnspFnDev->UsbFnIo,
                  NULL
                  );

  // Register Boot Service Exit Event For Release Resource
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  CdnspDxeExitBootService,
                  gCdnspFnDev,
                  &gEfiEventExitBootServicesGuid,
                  &gCdnspFnDev->ExitBootServiceEvent
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Register Cdnsp Exit Boot Service Failed %x \n", Status));
    gCdnspFnDev->ExitBootServiceEvent = NULL;
  }

ControlNotFind:
  if (EFI_ERROR (Status)) {
    CleanCdnspFnDev ();
  }

ERROR_RESOURCE:
  POST_CODE (CdnspDxeEnd);

  return Status;
}
