/** Cdnsp.c

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Cdnsp.h"

STATIC
CDNSP_DEVICE
*gCdnspDevice = NULL;

STATIC
USB_ENDPOINT_DESCRIPTOR
  CdnspEp0Desc = {
  .Length         = sizeof (USB_ENDPOINT_DESCRIPTOR),
  .DescriptorType = USB_DESC_TYPE_ENDPOINT,
  .Attributes     = USB_ENDPOINT_CONTROL
};

CDNSP_DEVICE
*
CdnspGetInstance (
  VOID
  )
{
  return gCdnspDevice;
}

UINT32
CdnspPortSpeed (
  UINT32  PortStatus
  )
{
  if (DEV_SUPERSPEEDPLUS (PortStatus)) {
    return UsbBusSpeedSuperPlus;
  } else if (DEV_SUPERSPEED (PortStatus)) {
    return UsbBusSpeedSuper;
  } else if (DEV_HIGHSPEED (PortStatus)) {
    return UsbBusSpeedHigh;
  } else if (DEV_FULLSPEED (PortStatus)) {
    return UsbBusSpeedFull;
  }

  return UsbBusSpeedUnknown;
}

STATIC
VOID
CdnspDisablePort (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        *PortRegs
  )
{
  UINT32  Temp = CdnspPortStateToNeutral (CdnspRead (PortRegs));

  CdnspWrite (Temp | PORT_PED, PortRegs);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", PortRegs, (PORT_PED | Temp)));
}

EFI_STATUS
CdnspInternalEpDequeue (
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  EFI_STATUS  RemoveStatus = EFI_SUCCESS;

  if (GET_EP_CTX_STATE (CdnspEp->OutCtx) == EP_STATE_RUNNING) {
    Status = CdnspCmdStopEp (gCdnspDevice, CdnspEp);
  }

  RemoveStatus = CdnspRemoveRequest (gCdnspDevice, CdnspRequest, CdnspEp);

  return EFI_ERROR (RemoveStatus) ? RemoveStatus : Status;
}

EFI_STATUS
CdnspEpDequeueRequest (
  USB_EP       *Ep,
  USB_REQUEST  *Request
  )
{
  CDNSP_EP  *CdnspEp = (CDNSP_EP *)Ep;

  if (!CdnspEp->Endpoint.Des) {
    DEBUG ((DEBUG_ERROR, "Dequeue Event On Disabled Ep\n"));
    return EFI_NOT_READY;
  }

  if (!(CdnspEp->EpState & EP_ENABLED)) {
    return EFI_SUCCESS;
  }

  return CdnspInternalEpDequeue (CdnspEp, (CDNSP_REQUEST *)Request);
}

VOID
CdnspDisableInterfaceEndpoint (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_USB_ENDPOINT_DESCRIPTOR  *In;
  EFI_USB_ENDPOINT_DESCRIPTOR  *Out;

  if (CdnspDevice->Speed >= UsbBusSpeedSuper) {
    In  = &((CONFIG_SS_DESC *)GetConfigDesc ())->EndpointDescriptorIn;
    Out = &((CONFIG_SS_DESC *)GetConfigDesc ())->EndpointDescriptorOut;
  } else {
    In  = &((CONFIG_NOT_SS_DESC *)GetConfigDesc ())->EndpointDescriptorIn;
    Out = &((CONFIG_NOT_SS_DESC *)GetConfigDesc ())->EndpointDescriptorOut;
  }

  UINT8  InEpIndex  = ((In->EndpointAddress & (~USB_ENDPOINT_DIR_IN)) << 1);
  UINT8  OutEpIndex = ((Out->EndpointAddress & (~USB_ENDPOINT_DIR_IN)) << 1) - 1;

  CdnspEpDisable (&gCdnspDevice->Eps[InEpIndex].Endpoint);
  CdnspEpDisable (&gCdnspDevice->Eps[OutEpIndex].Endpoint);

  gCdnspDevice->Eps[InEpIndex].Endpoint.Des  = NULL;
  gCdnspDevice->Eps[OutEpIndex].Endpoint.Des = NULL;
}

VOID
CdnspDeviceDisconnect (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspDevice->CdnspState |= CDNSP_STATE_DISCONNECT_PENDING;

  DEBUG ((DEBUG_COMMON_LOG, "Set Device State To(0x%x)\n", CdnspDevice->CdnspState));

  CdnspDisableInterfaceEndpoint (CdnspDevice);
  CdnspDevice->Speed = UsbBusSpeedUnknown;
  SetDeviceState (USB_STATE_NOTATTACHED);
  NotifyEvent (EfiUsbMsgBusEventDetach, NULL, NULL, NULL);
  CdnspDevice->CdnspState &= ~CDNSP_STATE_DISCONNECT_PENDING;

  DEBUG ((DEBUG_COMMON_LOG, "Set Device State To(0x%x)\n", CdnspDevice->CdnspState));
}

EFI_STATUS
CdnspSetupDevice (
  CDNSP_DEVICE     *CdnspDevice,
  CDNSP_SETUP_DEV  Setup
  )
{
  CDNSP_INPUT_CONTROL_CTX  *CtrlCtx;
  CDNSP_SLOT_CTX           *SlotCtx;
  INT32                    DevState = 0;
  EFI_STATUS               Status;

  if (!CdnspDevice->SlotId) {
    DEBUG ((DEBUG_ERROR, "Set Up With SlotId(0)\n"));
    return EFI_INVALID_PARAMETER;
  }

  if (!CdnspDevice->ActivePort->PortNum) {
    DEBUG ((DEBUG_ERROR, "Set Up Without Port\n"));
    return EFI_INVALID_PARAMETER;
  }

  SlotCtx  = CdnspGetSlotCtx (&CdnspDevice->OutCtx);
  DevState = GET_SLOT_STATE (SlotCtx->DevState);

  DEBUG ((DEBUG_COMMON_LOG, "Slot State(%d)\n", DevState));
  //
  // without address is slot state = enable state after the enable slot command
  //
  if ((Setup == SETUP_CONTEXT_ONLY) && (DevState == SLOT_STATE_DEFAULT)) {
    return EFI_SUCCESS;
  }

  SlotCtx = CdnspGetSlotCtx (&CdnspDevice->InCtx);
  CtrlCtx = CdnspGetInputControlCtx (&CdnspDevice->InCtx);

  if (!SlotCtx->DevInfo || (DevState == SLOT_STATE_DEFAULT)) {
    Status = CdnspSetupAddressablePrivDev (CdnspDevice);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  CdnspCopyEp0DequeueIntoInputCtx (CdnspDevice);

  CtrlCtx->AddFlags  = SLOT_FLAG | EP0_FLAG;
  CtrlCtx->DropFlags = 0;

  CdnspQueueAddressDevice (CdnspDevice, (UINT64)CdnspDevice->InCtx.Bytes, Setup);
  CdnspRingCmdDb (CdnspDevice);
  Status = CdnspWaitForCmdCompl (CdnspDevice);

  CtrlCtx->AddFlags  = 0;
  CtrlCtx->DropFlags = 0;

  return Status;
}

EFI_STATUS
CdnspHaltEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep,
  INT32         Value
  )
{
  EFI_STATUS  Status;

  if (Value) {
    Status = CdnspCmdStopEp (CdnspDevice, Ep);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (GET_EP_CTX_STATE (Ep->OutCtx) == EP_STATE_STOPPED) {
      CdnspQueueHaltEndpoint (CdnspDevice, Ep->Index);
      CdnspRingCmdDb (CdnspDevice);
      Status = CdnspWaitForCmdCompl (CdnspDevice);
    }

    Ep->EpState |= EP_HALTED;
  } else {
    CdnspQueueResetEp (CdnspDevice, Ep->Index);
    CdnspRingCmdDb (CdnspDevice);
    Status = CdnspWaitForCmdCompl (CdnspDevice);

    if (EFI_ERROR (Status)) {
      return Status;
    }

    Ep->EpState &= ~EP_HALTED;

    if ((Ep->Index != 0) && !(Ep->EpState & EP_WEDGE)) {
      CdnspRingDoorbellForActiveRing (CdnspDevice, Ep);
    }

    Ep->EpState &= ~EP_WEDGE;
  }

  return Status;
}

EFI_STATUS
CdnspResetDevice (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_SLOT_CTX  *SlotCtx;
  INT32           SlotState;
  EFI_STATUS      Status;
  INT32           Index;

  SlotCtx                    = CdnspGetSlotCtx (&CdnspDevice->InCtx);
  SlotCtx->DevInfo           = 0;
  CdnspDevice->DeviceAddress = 0;

  SlotCtx   = CdnspGetSlotCtx (&CdnspDevice->OutCtx);
  SlotState = GET_SLOT_STATE (SlotCtx->DevState);

  DEBUG ((DEBUG_COMMON_LOG, "SlotState(%d)\n", SlotState));

  if ((SlotState <= SLOT_STATE_DEFAULT) &&
      CdnspDevice->Eps[0].EpState & EP_HALTED)
  {
    CdnspHaltEndpoint (CdnspDevice, &CdnspDevice->Eps[0], 0);
  }

  CdnspDevice->Eps[0].EpState &= ~(EP_STOPPED | EP_HALTED);
  CdnspDevice->Eps[0].EpState |= EP_ENABLED;

  if (SlotState <= SLOT_STATE_DEFAULT) {
    return 0;
  }

  CdnspQueueResetDevice (CdnspDevice);
  CdnspRingCmdDb (CdnspDevice);
  Status = CdnspWaitForCmdCompl (CdnspDevice);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Reset Device Failed(%r)\n", Status));
    return Status;
  }

  for (Index = 1; Index < CDNSP_ENDPOINTS_NUM; ++Index) {
    CdnspDevice->Eps[Index].EpState |= EP_STOPPED | EP_UNCONFIGURED;
  }

  return Status;
}

STATIC
VOID
CdnspClearChickenBits2 (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        Bit
  )
{
  UINT32  *Reg;
  VOID    *Base;
  UINT32  Offset = 0;

  Base   = &CdnspDevice->CapsRegs->HcCapBase;
  Offset = CdnspFindNextExtCap (Base, Offset, D_XEC_PRE_REGS_CAP);
  Reg    = Base + Offset + REG_CHICKEN_BITS_2_OFFSET;
  Bit    = CdnspRead (Reg) & ~Bit;

  CdnspWrite (Bit, Reg);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", Reg, Bit));
}

VOID
CdnspIrqReset (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_PORT_REGS  *PortRegs;

  CdnspResetDevice (CdnspDevice);

  PortRegs           = CdnspDevice->ActivePort->Regs;
  CdnspDevice->Speed = CdnspPortSpeed (CdnspRead (PortRegs));

  SetDeviceState (USB_STATE_DEFAULT);
  NotifyEvent (EfiUsbMsgBusEventReset, NULL, NULL, NULL);

  switch (CdnspDevice->Speed) {
    case UsbBusSpeedSuperPlus:
    case UsbBusSpeedSuper:
      CdnspEp0Desc.MaxPacketSize             = 512;
      CdnspDevice->Eps[0].Endpoint.MaxPacket = 512;
      break;
    case UsbBusSpeedHigh:
    case UsbBusSpeedFull:
      CdnspEp0Desc.MaxPacketSize             = 64;
      CdnspDevice->Eps[0].Endpoint.MaxPacket = 64;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "UnKnown Speed(%d)\n", CdnspDevice->Speed));
      break;
  }

  CdnspClearChickenBits2 (CdnspDevice, CHICKEN_XDMA_2_TP_CACHE_DIS);

  CdnspSetupDevice (CdnspDevice, SETUP_CONTEXT_ONLY);

  if (CdnspDevice->Speed >= UsbBusSpeedSuper) {
    SetBcdUSB (0x0320);
    SetMaxPacketSize0 (9);
  } else {
    SetBcdUSB (0x0200);
  }

  NotifyEvent (EfiUsbMsgBusEventSpeed, NULL, NULL, NULL);
}

INT32
CdnspFindNextExtCap (
  VOID    *Base,
  UINT32  Start,
  INT32   Id
  )
{
  UINT32  Offset = Start;
  UINT32  Next;
  UINT32  Val;

  if (!Start || (Start == HCC_PARAMS_OFFSET)) {
    Val = CdnspRead ((UINT8 *)Base + HCC_PARAMS_OFFSET);
    if (Val == ~0) {
      return 0;
    }

    Offset = HCC_EXT_CAPS (Val) << 2;
    if (!Offset) {
      return 0;
    }
  }

  do {
    Val = CdnspRead ((UINT8 *)Base + Offset);
    if (Val == ~0) {
      return 0;
    }

    if ((EXT_CAPS_ID (Val) == Id) && (Offset != Start)) {
      return Offset;
    }

    Next    = EXT_CAPS_NEXT (Val);
    Offset += Next << 2;
  } while (Next);

  return 0;
}

EFI_STATUS
CdnspConfigDeviceMode (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32      Reg      = OTGCMD_OTG_DIS;
  UINT32      ReadyBit = OTGSTS_CDNSP_DEV_READY;
  EFI_STATUS  Status   = EFI_SUCCESS;

  CdnspWrite (OTGCMD_DEV_BUS_REQ | Reg, &CdnspDevice->OtgRegs->Cmd);

  Status = ReadPollTimeout (&CdnspDevice->OtgRegs->Sts, 1, ReadyBit, 10, 100);
  if (Status == EFI_TIMEOUT) {
    DEBUG ((DEBUG_ERROR, "Timeout When Config Device\n"));
    return Status;
  }

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OtgRegs->Sts, (OTGCMD_DEV_BUS_REQ | Reg)));
  return Status;
}

VOID
CdnspDisableDeviceMode (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  gBS->Stall (1);
  CdnspWrite (
    OTGCMD_HOST_BUS_DROP | OTGCMD_DEV_BUS_DROP,
    &CdnspDevice->OtgRegs->Cmd
    );
  ReadPollTimeout (&CdnspDevice->OtgRegs->Sts, 0, OTGSTATE_DEV_STATE_MASK, 1, 2000);
  //
  // TODO set phy mode invalid
  //
}

VOID
CdnspDied (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspDevice->CdnspState |= CDNSP_STATE_DYING;
  CdnspHalt (CdnspDevice);

  DEBUG ((DEBUG_ERROR, "Cdnsp Died, State(%x)\n", CdnspDevice->CdnspState));
}

STATIC
VOID
CdnspQuiesce (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32  Halted;
  UINT32  Mask;
  UINT32  Cmd;

  Mask = ~(UINT32)(CDNSP_IRQS);

  Halted = CdnspRead (&CdnspDevice->OpRegs->Sts) & STS_HALT;
  // CMD_R_S clear to 0, completes any current or queue command or TDs,
  // and any usb transaction with them. then halts
  if (!Halted) {
    Mask &= ~(CMD_R_S | CMD_DEVEN);
  }

  Cmd  = CdnspRead (&CdnspDevice->OpRegs->Cmd);
  Cmd &= Mask;
  CdnspWrite (Cmd, &CdnspDevice->OpRegs->Cmd);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OpRegs->Cmd, Cmd));
}

EFI_STATUS
CdnspHalt (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  CdnspQuiesce (CdnspDevice);

  Status = ReadPollTimeout (&CdnspDevice->OpRegs->Sts, 1, STS_HALT, 1, 16);

  if (EFI_TIMEOUT == Status) {
    DEBUG ((DEBUG_ERROR, "Timeout When Halt Device\n"));
    return Status;
  }

  CdnspDevice->CdnspState |= CDNSP_STATE_HALTED;

  DEBUG ((DEBUG_COMMON_LOG, "Set Device State To Halted\n"));

  return Status;
}

EFI_STATUS
CdnspReset (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32      Cmd;
  UINT32      Temp;
  EFI_STATUS  Status = EFI_SUCCESS;

  Temp = CdnspRead (&CdnspDevice->OpRegs->Sts);

  if (~(UINT32)0 == Temp) {
    DEBUG ((DEBUG_ERROR, "Device Not Accessible Reset Failed\n"));
    return EFI_ACCESS_DENIED;
  }

  if (0 == (Temp & STS_HALT)) {
    DEBUG ((DEBUG_ERROR, "Not Halt before Reset, Reset Abort\n"));
    return EFI_NOT_READY;
  }

  Cmd  = CdnspRead (&CdnspDevice->OpRegs->Cmd);
  Cmd |= CMD_RESET;
  CdnspWrite (Cmd, &CdnspDevice->OpRegs->Cmd);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OpRegs->Cmd, Cmd));

  Status = ReadPollTimeout (&CdnspDevice->OpRegs->Cmd, 0, CMD_RESET, 10, 100);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Controller Reset Failed\n"));
    return Status;
  }

  Status = ReadPollTimeout (&CdnspDevice->OpRegs->Sts, 0, STS_CNR, 10, 100);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Controller Not Ready to Work\n"));
    return Status;
  }

  DEBUG ((DEBUG_COMMON_LOG, "Controller Ready to Work\n"));

  return Status;
}

VOID
CdnspGetRevCap (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  VOID  *Reg = &CdnspDevice->CapsRegs->HcCapBase;

  Reg += CdnspFindNextExtCap (Reg, 0, RTL_REV_CAP);

  CdnspDevice->RevCap = Reg;

  DEBUG ((
    DEBUG_COMMON_LOG,
    "Cdnsp:Rev (0x%x/0x%x), eps(0x%x), buffer(0x%x/0x%x\n)",
    CdnspRead (&CdnspDevice->RevCap->CtrlRevision),
    CdnspRead (&CdnspDevice->RevCap->RtlRevision),
    CdnspRead (&CdnspDevice->RevCap->EpSupported),
    CdnspRead (&CdnspDevice->RevCap->RxBuffSize),
    CdnspRead (&CdnspDevice->RevCap->TxBuffSize)
    ));
}

EFI_STATUS
CdnspGenSetup (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      HcCapBase;
  UINT8       *Base;

  CdnspDevice->CapsRegs = (CDNSP_CAP_REGS *)(CdnspDevice->BaseAddress + CAP_REGS_OFFSET);
  HcCapBase             = CdnspRead (&CdnspDevice->CapsRegs->HcCapBase);
  Base                  = (UINT8 *)&CdnspDevice->CapsRegs->HcCapBase;
  CdnspDevice->OpRegs   = (CDNSP_OP_REGS *)(Base + HC_LENGTH (HcCapBase));
  // reset value of runregsoff = 0x1000
  CdnspDevice->RunRegs    = (CDNSP_RUN_REGS *)(Base +(CdnspRead (&CdnspDevice->CapsRegs->RunRegsOff) & RTSOFF_MASK));
  CdnspDevice->HcsParams1 = CdnspRead (&CdnspDevice->CapsRegs->HcsParams1);
  CdnspDevice->HcsParams3 = CdnspRead (&CdnspDevice->CapsRegs->HcsParams3);
  CdnspDevice->HciVersion = HC_VERSION (HcCapBase);
  CdnspDevice->HccParams1 = CdnspRead (&CdnspDevice->CapsRegs->HccParams1);

  DEBUG ((DEBUG_COMMON_LOG, "CdnspGenSetup CapsRegs(0x%x),OpRegs(0x%x),RunRegs(0x%x)\n", CdnspDevice->CapsRegs, CdnspDevice->OpRegs, CdnspDevice->RunRegs));

  CdnspGetRevCap (CdnspDevice);

  Status = CdnspHalt (CdnspDevice);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CdnspReset (CdnspDevice);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (HCC_64BIT_ADDR (CdnspDevice->HccParams1)) {
    CdnspDevice->Support64Address = TRUE;
  }

  Status = CdnspMemInit (CdnspDevice);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

USB_REQUEST *
CdnspEpAllocRequest (
  USB_EP  *Ep
  )
{
  CDNSP_EP       *CdnspEp = (CDNSP_EP *)Ep;
  CDNSP_REQUEST  *CdnspRequest;

  CdnspRequest = AllocateZeroPool (sizeof (CDNSP_REQUEST));
  if (!CdnspRequest) {
    return NULL;
  }

  CdnspRequest->EpNum = CdnspEp->Number;
  CdnspRequest->Ep    = CdnspEp;

  return &CdnspRequest->Request;
}

VOID
CdnspEpFreeRequest (
  USB_EP       *Ep,
  USB_REQUEST  *Request
  )
{
  CDNSP_REQUEST  *CdnspRequest;

  CdnspRequest = (CDNSP_REQUEST *)Request;
  FreePool (CdnspRequest);
}

EFI_STATUS
CdnspInternalEpEnqueue (
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest
  )
{
  EFI_STATUS   Status = EFI_SUCCESS;
  USB_REQUEST  *Request;

  if ((CdnspEp->Number == 0) && !IsListEmpty (&CdnspEp->PendingList)) {
    return EFI_ALREADY_STARTED;
  }

  Request                 = &CdnspRequest->Request;
  Request->Actual         = 0;
  Request->Status         = ENDPOINT_INPROCESS;
  CdnspRequest->Direction = CdnspEp->Direction;
  CdnspRequest->EpNum     = CdnspEp->Number;

  InsertTailList (&CdnspEp->PendingList, &CdnspRequest->Request.Link);

  switch (UsbEndpointType (CdnspEp->Endpoint.Des)) {
    case USB_ENDPOINT_CONTROL:
      Status = CdnspQueueCtrlTx (gCdnspDevice, CdnspEp, CdnspRequest);
      break;
    case USB_ENDPOINT_BULK:
    case USB_ENDPOINT_INTERRUPT:
      Status = CdnspQueueBulkTx (gCdnspDevice, CdnspRequest);
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Unsupport Transfer Type(%d)\n", UsbEndpointType (CdnspEp->Endpoint.Des)));
  }

  if (EFI_ERROR (Status)) {
    RemoveEntryList (&CdnspRequest->Request.Link);
  }

  return Status;
}

EFI_STATUS
CdnspEpEnqueueRequest (
  USB_EP       *Ep,
  USB_REQUEST  *Request
  )
{
  CDNSP_REQUEST  *CdnspRequest;
  CDNSP_EP       *CdnspEp;
  EFI_STATUS     Status = EFI_SUCCESS;

  if (!Ep || !Request) {
    return EFI_INVALID_PARAMETER;
  }

  CdnspEp      = (CDNSP_EP *)Ep;
  CdnspRequest = (CDNSP_REQUEST *)Request;

  if (!(CdnspEp->EpState & EP_ENABLED)) {
    DEBUG ((DEBUG_ERROR, "Try Enqueue Event On A Disable Ep\n"));
    return EFI_INVALID_PARAMETER;
  }

  Status = CdnspInternalEpEnqueue (CdnspEp, CdnspRequest);

  return Status;
}

EFI_STATUS
CdnspQueueTransfer (
  UINT8  EpIndex,
  UINTN  *BufferSize,
  VOID   *Buffer
  )
{
  USB_REQUEST  *Request;
  CDNSP_EP     *Ep    = &gCdnspDevice->Eps[EpIndex];
  EFI_STATUS   Status = EFI_SUCCESS;

  if ((NULL == gCdnspDevice) || !(Ep->EpState & EP_ENABLED)) {
    Status = EFI_NOT_READY;
    DEBUG ((DEBUG_ERROR, "Queue Request To Disable Endpoint\n"));
  }

  if (Ep->ResidentRequest == NULL) {
    Request = CdnspEpAllocRequest (&Ep->Endpoint);
    if (Request != NULL) {
      Ep->ResidentRequest = Request;
      goto ENQUEUE_REQ;
    } else {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  if (IsNodeInList (&Ep->PendingList, &Ep->ResidentRequest->Link)) {
    Request = CdnspEpAllocRequest (&Ep->Endpoint);
    if (NULL == Request) {
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    Request = Ep->ResidentRequest;
  }

ENQUEUE_REQ:
  Request->Buf    = Buffer;
  Request->Length = *BufferSize;
  Status          = CdnspEpEnqueueRequest (&gCdnspDevice->Eps[EpIndex].Endpoint, Request);
  return Status;
}

STATIC
EFI_STATUS
CdnspInitEndpoints (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_EP  *Ep;
  UINT32    Index;
  BOOLEAN   Direction;
  UINT8     EpNum;

  for (Index = 0; Index < CDNSP_ENDPOINTS_NUM; Index++) {
    Direction = !(Index & 1);
    EpNum     = ((Index + 1) >> 1);

    if (!CDNSP_IF_EP_EXIST (CdnspDevice, EpNum, Direction)) {
      continue;
    }

    Ep         = &CdnspDevice->Eps[Index];
    Ep->Number = EpNum;
    // 0 for OUT, 1 for IN
    Ep->Direction = Direction;

    if (EpNum == 0) {
      Ep->Index = 0;
      UsbEpSetMaxpacketLimit (&Ep->Endpoint, 512);
      Ep->Endpoint.MaxBurst         = 1;
      Ep->Endpoint.Des              = &CdnspEp0Desc;
      Ep->Endpoint.SsCompDes        = NULL;
      Ep->Endpoint.Caps.TypeControl = TRUE;
      Ep->Endpoint.Caps.DirIn       = TRUE;
      Ep->Endpoint.Caps.DirOut      = TRUE;

      CdnspDevice->Ep0Preq.EpNum = Ep->Number;
      CdnspDevice->Ep0Preq.Ep    = Ep;
    } else {
      Ep->Index = (EpNum *2 + (Direction ? 1 : 0)) - 1;
      UsbEpSetMaxpacketLimit (&Ep->Endpoint, 1024);

      Ep->Endpoint.MaxStreams    = 0;
      Ep->Endpoint.Caps.TypeBulk = TRUE;
      Ep->Endpoint.Caps.TypeInt  = TRUE;
      Ep->Endpoint.Caps.DirIn    = Direction;
      Ep->Endpoint.Caps.DirOut   = !Direction;
    }

    Ep->InCtx  = CdnspGetEpCtx (&CdnspDevice->InCtx, Ep->Index);
    Ep->OutCtx = CdnspGetEpCtx (&CdnspDevice->OutCtx, Ep->Index);

    InitializeListHead (&Ep->PendingList);
  }

  return EFI_SUCCESS;
}

UINT32
CdnspPortStateToNeutral (
  UINT32  State
  )
{
  return (State & CDNSP_PORT_RO) | (State & CDNSP_PORT_RWS);
}

VOID
CdnspSetLinkState (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        *PortRegs,
  UINT32        LinkState
  )
{
  UINT32  Temp;

  Temp = CdnspRead (PortRegs);
  Temp = CdnspPortStateToNeutral (Temp);

  Temp |= PORT_WKCONN_E | PORT_WKDISC_E;

  CdnspWrite (Temp, PortRegs);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", PortRegs, Temp));

  Temp &= ~PORT_PLS_MASK;
  Temp |= PORT_LINK_STROBE | LinkState;

  CdnspWrite (Temp, PortRegs);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", PortRegs, Temp));

  Temp = CdnspRead (PortRegs);

  DEBUG ((DEBUG_COMMON_LOG, "Set Link State(%x)\n", Temp));
}

STATIC
EFI_STATUS
CdnspStart (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32      Temp;
  EFI_STATUS  Status;

  Temp  = CdnspRead (&CdnspDevice->OpRegs->Cmd);
  Temp |= (CMD_R_S | CMD_DEVEN);

  CdnspWrite (Temp, &CdnspDevice->OpRegs->Cmd);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", CdnspDevice->OpRegs->Cmd, Temp));

  CdnspDevice->CdnspState = 0;

  Status = ReadPollTimeout (&CdnspDevice->OpRegs->Sts, 0, STS_HALT, 1, 16);
  if (EFI_ERROR (Status)) {
    CdnspDevice->CdnspState = CDNSP_STATE_DYING;
    DEBUG ((DEBUG_ERROR, "Start Cdnsp Failed(%r) Set Device State To(%x)\n", Status, CDNSP_STATE_DYING));
  }

  return Status;
}

STATIC
EFI_STATUS
CdnspRun (
  CDNSP_DEVICE  *CdnspDevice,
  UINT8         Speed
  )
{
  UINT32      FsSpeed = 0;
  UINT32      Temp;
  EFI_STATUS  Status = EFI_SUCCESS;

  Temp  = CdnspRead (&CdnspDevice->IntrRegs->IrqControl);
  Temp &= ~IMOD_INTERVAL_MASK;
  Temp |= ((IMOD_DEFAULT_INTERVAL / 250) & IMOD_INTERVAL_MASK);
  CdnspWrite (Temp, &CdnspDevice->IntrRegs->IrqControl);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->IntrRegs->IrqControl, Temp));

  Temp = CdnspRead (&CdnspDevice->Port3xRegs->ModeAddr);

  // not support speed low
  switch (Speed) {
    case UsbBusSpeedSuperPlus:
      Temp |= CFG_3XPORT_SSP_SUPPORT;
      break;
    case UsbBusSpeedSuper:
      Temp &= ~CFG_3XPORT_SSP_SUPPORT;
      break;
    case UsbBusSpeedHigh:
      break;
    case UsbBusSpeedFull:
      FsSpeed = PORT_REG6_FORCE_FS;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Invalid Bus Speed = %d\n", Speed));
    case UsbBusSpeedUnknown:
      Speed = UsbBusSpeedSuper;
      break;
  }

  if (Speed >= UsbBusSpeedSuper) {
    CdnspWrite (Temp, &CdnspDevice->Port3xRegs->ModeAddr);

    DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->Port3xRegs->ModeAddr, Temp));

    CdnspSetLinkState (CdnspDevice, &CdnspDevice->Usb3Port.Regs->PortSc, XDEV_RXDETECT);
  } else {
    CdnspDisablePort (CdnspDevice, &CdnspDevice->Usb3Port.Regs->PortSc);
  }

  CdnspSetLinkState (CdnspDevice, &CdnspDevice->Usb2Port.Regs->PortSc, XDEV_RXDETECT);
  // configuration register if need, need change to before the power on reset
  CdnspWrite ((PORT_REG6_L1_L0_HW_EN | FsSpeed), &CdnspDevice->Port20Regs->PortReg6);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->Port20Regs->PortReg6, (PORT_REG6_L1_L0_HW_EN | FsSpeed)));

  CdnspEp0Desc.MaxPacketSize = 512;

  Status = CdnspStart (CdnspDevice);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Temp  = CdnspRead (&CdnspDevice->OpRegs->Cmd);
  Temp |= (CMD_INTE);
  CdnspWrite (Temp, &CdnspDevice->OpRegs->Cmd);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OpRegs->Cmd, Temp));

  Temp = CdnspRead (&CdnspDevice->IntrRegs->IrqPending);
  CdnspWrite (IMAN_IE_SET (Temp), &CdnspDevice->IntrRegs->IrqPending);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->IntrRegs->IrqPending, IMAN_IE_SET (Temp)));

  DEBUG ((DEBUG_COMMON_LOG, "Ready To Handle Event\n"));

  return Status;

Error:
  CdnspHalt (CdnspDevice);
  return Status;
}

VOID
CdnspSetVbus (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32  Reg;

  Reg  = CdnspRead (&CdnspDevice->OtgRegs->Override);
  Reg &= ~OVERRIDE_SESS_VLD_SEL;

  CdnspWrite (Reg, &CdnspDevice->OtgRegs->Override);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OtgRegs->Override, Reg));
}

VOID
CdnspClearVbus (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32  Reg;

  Reg  = CdnspRead (&CdnspDevice->OtgRegs->Override);
  Reg |= OVERRIDE_SESS_VLD_SEL;

  CdnspWrite (Reg, &CdnspDevice->OtgRegs->Override);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OtgRegs->Override, Reg));
}

STATIC
EFI_STATUS
CdnspPullUp (
  CDNSP_DEVICE  *CdnspDevice,
  BOOLEAN       IsOn
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (!IsOn) {
    Status = CdnspReset (CdnspDevice);
    CdnspClearVbus (CdnspDevice);
  } else {
    CdnspSetVbus (CdnspDevice);
  }

  return Status;
}

STATIC
VOID
CdnspFreeEndpoints (
  VOID
  )
{
  //
  // endpoint is store in a array
  //
  return;
}

STATIC
VOID
CdnspConsumeAllEvent (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_SEGMENT  *EventDeqSeg;
  CDNSP_TRB      *EventRingDeq;
  CDNSP_TRB      *Event;
  UINT32         CycleBit;

  EventRingDeq = CdnspDevice->EventRing->Dequeue;
  EventDeqSeg  = CdnspDevice->EventRing->DeqSeg;
  Event        = CdnspDevice->EventRing->Dequeue;

  while (1) {
    CycleBit = Event->EventCmd.Flags & TRB_CYCLE;

    if (CycleBit != CdnspDevice->EventRing->CycleState) {
      break;
    }

    CdnspIncDeq (CdnspDevice, CdnspDevice->EventRing);

    if (!CdnspLastTrbOnSeg (EventDeqSeg, Event)) {
      Event++;
      continue;
    }

    if (CdnspLastTrbOnRing (CdnspDevice->EventRing, EventDeqSeg, Event)) {
      CycleBit ^= 1;
    }

    EventDeqSeg = EventDeqSeg->Next;
    Event       = EventDeqSeg->Trbs;
  }

  CdnspUpdateErstDequeue (CdnspDevice, EventRingDeq, 1);
}

STATIC
VOID
CdnspClearPortChangeBit (
  CDNSP_DEVICE  *CdnspDevice,
  VOID          *PortRegs
  )
{
  UINT32  PortSc = CdnspRead (PortRegs);

  CdnspWrite (
    CdnspPortStateToNeutral (PortSc) |
    (PortSc & PORT_CHANGE_BITS),
    PortRegs
    );
}

STATIC
VOID
CdnspClearCmdRing (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_SEGMENT  *Seg;
  UINT64         Value;
  UINT32         Index;

  CdnspInitializeRingInfo (CdnspDevice->CommandRing);

  Seg = CdnspDevice->CommandRing->FirstSeg;

  for (Index = 0; Index < CdnspDevice->CommandRing->NumSegs; Index++) {
    gBS->SetMem (Seg->Trbs, sizeof (CDNSP_TRB)*(TRBS_PER_SEGMENT - 1), 0);
    Seg = Seg->Next;
  }

  Value = CdnspRead64 (&CdnspDevice->OpRegs->CRCR);
  Value = (Value & (UINT64)CRCR_RSVD_BITS) |
          ((UINT64)CdnspDevice->CommandRing->FirstSeg->Trbs & (UINT64) ~CRCR_RSVD_BITS) |
          CdnspDevice->CommandRing->CycleState;

  CdnspWrite64 (Value, &CdnspDevice->OpRegs->CRCR);
  DEBUG ((DEBUG_ERROR, "Cdnsp:control stoped \n"));
}

VOID
CdnspStop (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  USB_REQUEST  *Request = NULL;
  UINT32       Temp;

  CdnspCmdFlushEp (CdnspDevice, &CdnspDevice->Eps[0]);

  if (!IsListEmpty (&CdnspDevice->Eps[0].PendingList)) {
    Request = (USB_REQUEST *)GetFirstNode (&CdnspDevice->Eps[0].PendingList);
    if (Request == &CdnspDevice->Ep0Preq.Request) {
      CdnspEpDequeueRequest (&CdnspDevice->Eps[0].Endpoint, Request);
    }
  }

  CdnspDisablePort (CdnspDevice, &CdnspDevice->Usb2Port.Regs->PortSc);
  CdnspDisablePort (CdnspDevice, &CdnspDevice->Usb2Port.Regs->PortSc);
  CdnspDisableSlot (CdnspDevice);

  CdnspHalt (CdnspDevice);

  Temp = CdnspRead (&CdnspDevice->OpRegs->Sts);
  CdnspWrite ((Temp & ~0x1FFF) | STS_EINT, &CdnspDevice->OpRegs->Sts);
  Temp = CdnspRead (&CdnspDevice->IntrRegs->IrqPending);
  CdnspWrite (IMAN_IE_CLEAR (Temp), &CdnspDevice->IntrRegs->IrqPending);

  CdnspClearPortChangeBit (CdnspDevice, &CdnspDevice->Usb2Port.Regs->PortSc);
  CdnspClearPortChangeBit (CdnspDevice, &CdnspDevice->Usb3Port.Regs->PortSc);

  Temp  = CdnspRead (&CdnspDevice->IntrRegs->IrqPending);
  Temp |= IMAN_IP;
  CdnspWrite (Temp, &CdnspDevice->IntrRegs->IrqPending);

  CdnspConsumeAllEvent (CdnspDevice);

  CdnspClearCmdRing (CdnspDevice);
}

VOID
CdnspDeInit (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspPullUp (CdnspDevice, FALSE);
  CdnspStop (CdnspDevice);
  CdnspFreeEndpoints ();
  CdnspMemCleanup (CdnspDevice);
  CdnspDisableDeviceMode (CdnspDevice);
}

VOID
CdnspUpdateErstDequeue (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_TRB     *EventRingDeq,
  UINT8         ClearEhb
  )
{
  UINT64  Temp_64;

  Temp_64 = CdnspRead64 (&CdnspDevice->IntrRegs->ErDq);
  if (EventRingDeq != CdnspDevice->EventRing->Dequeue) {
    Temp_64 &= ERST_PTR_MASK;
    Temp_64 |= ((UINT64)CdnspDevice->EventRing->Dequeue) & ((UINT64) ~ERST_PTR_MASK);
  }

  if (ClearEhb) {
    Temp_64 |= ERST_EHB;
  } else {
    Temp_64 &= ~ERST_EHB;
  }

  CdnspWrite64 (Temp_64, &CdnspDevice->IntrRegs->ErDq);

  DEBUG ((DEBUG_EVENT_LOG, "Event Deq(%llx)\n", Temp_64));
}

EFI_STATUS
CdnspWaitForCmdCompl (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_SEGMENT  *EventDeqSeg;
  CDNSP_TRB      *CmdTrb;
  CDNSP_TRB      *Event;
  UINT32         CycleState;
  EFI_STATUS     Status;
  UINT32         Flags;

  CmdTrb                  = CdnspDevice->Cmd.CommandTrb;
  CdnspDevice->Cmd.Status = 0;

  Status = ReadPollTimeout (&CdnspDevice->OpRegs->CRCR, 0, CRCR_RB, 1, CDNSP_CMD_TIMEOUT_MS);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Timeout for wait Command\n"));
    CdnspDevice->CdnspState = CDNSP_STATE_DYING;
    return EFI_TIMEOUT;
  }

  Event       = CdnspDevice->EventRing->Dequeue;
  EventDeqSeg = CdnspDevice->EventRing->DeqSeg;
  CycleState  = CdnspDevice->EventRing->CycleState;

  while (1) {
    Flags = Event->EventCmd.Flags;

    if ((Flags & TRB_CYCLE) != CycleState) {
      return EFI_NOT_READY;
    }

    //
    // Because the event ring has no link Trb,
    // so if is the last trb in the segment change to the next Segment
    //
    if ((TRB_FIELD_TO_TYPE (Flags) != TRB_COMPLETION) ||
        ((UINT64)CmdTrb != Event->EventCmd.CmdTrb))
    {
      if (!CdnspLastTrbOnSeg (EventDeqSeg, Event)) {
        Event++;
        continue;
      }

      if (CdnspLastTrbOnRing (CdnspDevice->EventRing, EventDeqSeg, Event)) {
        CycleState ^= 1;
      }

      EventDeqSeg = EventDeqSeg->Next;
      Event       = EventDeqSeg->Trbs;
      continue;
    }

    CdnspDevice->Cmd.Status = GET_COMP_CODE (Event->EventCmd.Status);
    if (CdnspDevice->Cmd.Status == COMP_SUCCESS) {
      return EFI_SUCCESS;
    }

    return EFI_ABORTED;
  }
}

EFI_STATUS
CdnspDisableSlot (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  CdnspQueueSlotControl (CdnspDevice, TRB_DISABLE_SLOT);
  CdnspRingCmdDb (CdnspDevice);

  Status = CdnspWaitForCmdCompl (CdnspDevice);

  CdnspDevice->SlotId     = 0;
  CdnspDevice->ActivePort = NULL;

  gBS->SetMem (CdnspDevice->InCtx.Bytes, CdnspDevice->InCtx.Size, 0);
  gBS->SetMem (CdnspDevice->OutCtx.Bytes, CdnspDevice->OutCtx.Size, 0);

  return Status;
}

EFI_STATUS
CdnspEnableSlot (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_SLOT_CTX  *SlotCtx;
  INT32           SlotState;
  EFI_STATUS      Status;

  SlotCtx   = CdnspGetSlotCtx (&CdnspDevice->OutCtx);
  SlotState = GET_SLOT_STATE (SlotCtx->DevState);

  if (SlotState != SLOT_STATE_DISABLED) {
    return EFI_SUCCESS;
  }

  CdnspQueueSlotControl (CdnspDevice, TRB_ENABLE_SLOT);
  CdnspRingCmdDb (CdnspDevice);
  Status = CdnspWaitForCmdCompl (CdnspDevice);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CdnspDevice->SlotId = 1;

  return Status;
}

UINT32
CdnspGetEndpointIndex (
  USB_ENDPOINT_DESCRIPTOR  *Desc
  )
{
  UINT32  Index = (UINT32)UsbEndpointNum (Desc);

  if (UsbEndpointXferControl (Desc)) {
    return Index * 2;
  }

  return (Index * 2) + (UsbEndpointDirIN (Desc) ? 1 : 0) -1;
}

UINT32
CdnspGetEndpointFlag (
  USB_ENDPOINT_DESCRIPTOR  *Desc
  )
{
  return 1 << (CdnspGetEndpointIndex (Desc) + 1);
}

STATIC
EFI_STATUS
CdnspConfigureEndpoint (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspQueueConfigureEndpoint (CdnspDevice, (UINT64)CdnspDevice->Cmd.InCtx->Bytes);
  CdnspRingCmdDb (CdnspDevice);
  return CdnspWaitForCmdCompl (CdnspDevice);
}

VOID
CdnspGiveback (
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest,
  INT32          Status
  )
{
  RemoveEntryList (&CdnspRequest->Request.Link);

  if (CdnspRequest->Request.Status == ENDPOINT_INPROCESS) {
    CdnspRequest->Request.Status = Status;
  }

  // not handle the out control transfer
  if (CdnspRequest != &gCdnspDevice->Ep0Preq) {
    if (CdnspEp->Direction) {
      NotifyEvent (EfiUsbMsgEndpointStatusChangedTx, &CdnspEp->Endpoint, (USB_REQUEST *)CdnspRequest, NULL);
    } else {
      NotifyEvent (EfiUsbMsgEndpointStatusChangedRx, &CdnspEp->Endpoint, (USB_REQUEST *)CdnspRequest, NULL);
    }
  }
}

STATIC
VOID
CdnspZeroInCtx (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_INPUT_CONTROL_CTX  *CtrlCtx;
  CDNSP_SLOT_CTX           *SlotCtx;
  CDNSP_EP_CTX             *EpCtx;

  UINT32  Index = 0;

  CtrlCtx = CdnspGetInputControlCtx (&CdnspDevice->InCtx);

  CtrlCtx->DropFlags = 0;
  CtrlCtx->AddFlags  = 0;
  SlotCtx            = CdnspGetSlotCtx (&CdnspDevice->InCtx);
  SlotCtx->DevInfo  &= ~LAST_CTX_MASK;
  SlotCtx->DevInfo  |= LAST_CTX (1);

  for (Index = 1; Index < CDNSP_ENDPOINTS_NUM; ++Index) {
    EpCtx          = CdnspGetEpCtx (&CdnspDevice->InCtx, Index);
    EpCtx->EpInfo  = 0;
    EpCtx->EpInfo2 = 0;
    EpCtx->Deq     = 0;
    EpCtx->TxInfo  = 0;
  }
}

STATIC
EFI_STATUS
CdnspUpdateEpsConfiguration (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *CdnspEp
  )
{
  CDNSP_INPUT_CONTROL_CTX  *CtrlCtx;
  CDNSP_SLOT_CTX           *SlotCtx;
  UINT32                   EpSts;
  UINT32                   Index;
  UINT32                   Temp;
  EFI_STATUS               Status = EFI_SUCCESS;

  CtrlCtx = CdnspGetInputControlCtx (&CdnspDevice->InCtx);

  if ((CtrlCtx->AddFlags == 0) && (CtrlCtx->DropFlags == 0)) {
    return EFI_SUCCESS;
  }

  CtrlCtx->AddFlags  |= SLOT_FLAG;
  CtrlCtx->AddFlags  &= ~EP0_FLAG;
  CtrlCtx->DropFlags &= (~(SLOT_FLAG | EP0_FLAG));

  SlotCtx = CdnspGetSlotCtx (&CdnspDevice->InCtx);

  for (Index = CDNSP_ENDPOINTS_NUM; Index >= 1; Index--) {
    Temp = BIT (Index);

    if ((CdnspDevice->Eps[Index -1].Ring && !(CtrlCtx->DropFlags & Temp)) ||
        (CtrlCtx->AddFlags & Temp) || (Index == 1))
    {
      SlotCtx->DevInfo &= ~LAST_CTX_MASK;
      SlotCtx->DevInfo |= LAST_CTX (Index);
      break;
    }
  }

  EpSts = GET_EP_CTX_STATE (CdnspEp->OutCtx);

  if (((CtrlCtx->AddFlags != SLOT_FLAG) &&
       (EpSts == EP_STATE_DISABLED)) ||
      ((EpSts != EP_STATE_DISABLED) && CtrlCtx->DropFlags))
  {
    Status = CdnspConfigureEndpoint (CdnspDevice);
  }

  CdnspZeroInCtx (CdnspDevice);

  return Status;
}

EFI_STATUS
CdnspEpEnable (
  USB_EP                   *Ep,
  USB_ENDPOINT_DESCRIPTOR  *Desc
  )
{
  CDNSP_INPUT_CONTROL_CTX  *CtrlCtx;
  CDNSP_EP                 *CdnspEp = (CDNSP_EP *)Ep;
  UINT32                   AddedCtxs;
  CDNSP_DEVICE             *CdnspDevice = CdnspGetInstance ();
  EFI_STATUS               Status;

  if (!Ep || !Desc || (Desc->DescriptorType != USB_DESC_TYPE_ENDPOINT) ||
      !Desc->MaxPacketSize || !CdnspDevice)
  {
    return EFI_INVALID_PARAMETER;
  }

  CdnspEp->EpState &= ~EP_UNCONFIGURED;

  if (CdnspEp->EpState & EP_ENABLED) {
    DEBUG ((DEBUG_ERROR, "Ep%d already enabled\n", CdnspEp->Number));
    return EFI_SUCCESS;
  }

  AddedCtxs = CdnspGetEndpointFlag (Desc);
  if ((AddedCtxs == SLOT_FLAG) || (AddedCtxs == EP0_FLAG)) {
    return EFI_INVALID_PARAMETER;
  }

  CdnspEp->Internal = Desc->Interval ? BIT (Desc->Interval - 1) : 0;

  if (CdnspDevice->Speed == UsbBusSpeedFull) {
    if (UsbEndpointType (Desc) == USB_ENDPOINT_INTERRUPT) {
      CdnspEp->Internal = Desc->Interval << 3;
    }

    if (UsbEndpointType (Desc) == USB_ENDPOINT_ISO) {
      DEBUG ((DEBUG_ERROR, "Ep%d do not support iso transfer\n", CdnspEp->Number));
    }
  }

  Status = CdnspEndpointInit (CdnspDevice, CdnspEp);
  if (EFI_ERROR ((Status))) {
    return Status;
  }

  CtrlCtx            = CdnspGetInputControlCtx (&CdnspDevice->InCtx);
  CtrlCtx->AddFlags  = AddedCtxs;
  CtrlCtx->DropFlags = 0;

  Status = CdnspUpdateEpsConfiguration (CdnspDevice, CdnspEp);
  if (EFI_ERROR (Status)) {
    CdnspFreeEndpointRings (CdnspDevice, CdnspEp);
    return Status;
  }

  CdnspEp->EpState |= EP_ENABLED;
  CdnspEp->EpState &= ~EP_STOPPED;

  return Status;
}

STATIC
VOID
CdnspInvalidateEpEvent (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *CdnspEp
  )
{
  CDNSP_SEGMENT  *Segment;
  CDNSP_TRB      *Event;
  UINT32         CycleState;
  UINT32         Data;

  Event      = CdnspDevice->EventRing->Dequeue;
  Segment    = CdnspDevice->EventRing->DeqSeg;
  CycleState = CdnspDevice->EventRing->CycleState;

  while (1) {
    Data = Event->TransferEvent.Flags;

    if ((Data & TRB_CYCLE) != CycleState) {
      break;
    }

    if ((TRB_FIELD_TO_TYPE (Data) == TRB_TRANSFER) &&
        (TRB_TO_EP_ID (Data) == (CdnspEp->Index + 1)))
    {
      Data                      |= TRB_EVENT_INVALIDATE;
      Event->TransferEvent.Flags = Data;
    }

    //
    // Event Ring only one segment
    //
    if (CdnspLastTrbOnSeg (Segment, Event)) {
      CycleState ^= 1;
      Segment     = CdnspDevice->EventRing->DeqSeg->Next;
      Event       = Segment->Trbs;
    } else {
      Event++;
    }
  }
}

EFI_STATUS
CdnspEpDisable (
  USB_EP  *Ep
  )
{
  CDNSP_INPUT_CONTROL_CTX  *CtrlCtx;
  CDNSP_EP                 *CdnspEp = (CDNSP_EP *)Ep;
  UINT32                   DropFlag;
  EFI_STATUS               Status = EFI_SUCCESS;
  USB_REQUEST              *Request;

  if (!Ep || !gCdnspDevice) {
    return EFI_INVALID_PARAMETER;
  }

  if (!(CdnspEp->EpState & EP_ENABLED)) {
    DEBUG ((DEBUG_ERROR, "Ep%d has already disabled\n", CdnspEp->Index));
    return EFI_INVALID_PARAMETER;
  }

  CdnspEp->EpState |= EP_DIS_IN_RROGRESS;

  if (!(CdnspEp->EpState & EP_UNCONFIGURED)) {
    CdnspCmdStopEp (gCdnspDevice, CdnspEp);
    CdnspCmdFlushEp (gCdnspDevice, CdnspEp);
  }

  //
  // remove pending request complete after enqueue transfer
  //
  while (!IsListEmpty (&CdnspEp->PendingList)) {
    Request = (USB_REQUEST *)GetFirstNode (&CdnspEp->PendingList);
    CdnspEpDequeueRequest (Ep, Request);
  }

  CdnspInvalidateEpEvent (gCdnspDevice, CdnspEp);

  CdnspEp->EpState  &= ~EP_DIS_IN_RROGRESS;
  DropFlag           = CdnspGetEndpointFlag (CdnspEp->Endpoint.Des);
  CtrlCtx            = CdnspGetInputControlCtx (&gCdnspDevice->InCtx);
  CtrlCtx->DropFlags = DropFlag;
  CtrlCtx->AddFlags  = 0;

  CdnspEndpointZero (CdnspEp);

  if (!(CdnspEp->EpState & EP_UNCONFIGURED)) {
    Status = CdnspUpdateEpsConfiguration (gCdnspDevice, CdnspEp);
  }

  CdnspFreeEndpointRings (gCdnspDevice, CdnspEp);

  CdnspEp->EpState &= ~(EP_ENABLED | EP_UNCONFIGURED);
  CdnspEp->EpState |= EP_STOPPED;

  return Status;
}

EFI_STATUS
CdnspSetConfig (
  VOID                    *ConfigDesc,
  EFI_USB_DEVICE_REQUEST  *CtrlRequest
  )
{
  CDNSP_DEVICE                 *CdnspDevice = CdnspGetInstance ();
  EFI_STATUS                   Status       = EFI_SUCCESS;
  EFI_USB_ENDPOINT_DESCRIPTOR  *In;
  EFI_USB_ENDPOINT_DESCRIPTOR  *Out;

  UINT8  InEpIndex  = 0;
  UINT8  OutEpIndex = 0;

  if (NULL != CdnspDevice) {
    if (CdnspDevice->Speed >= UsbBusSpeedSuper) {
      In                                               = &((CONFIG_SS_DESC *)ConfigDesc)->EndpointDescriptorIn;
      Out                                              = &((CONFIG_SS_DESC *)ConfigDesc)->EndpointDescriptorOut;
      InEpIndex                                        = ((In->EndpointAddress & (~USB_ENDPOINT_DIR_IN)) << 1);
      OutEpIndex                                       = ((Out->EndpointAddress & (~USB_ENDPOINT_DIR_IN)) << 1) - 1;
      gCdnspDevice->Eps[InEpIndex].Endpoint.Des        = In;
      gCdnspDevice->Eps[OutEpIndex].Endpoint.Des       = Out;
      gCdnspDevice->Eps[InEpIndex].Endpoint.SsCompDes  = &((CONFIG_SS_DESC *)ConfigDesc)->EndpointDescriptorCompIn;
      gCdnspDevice->Eps[OutEpIndex].Endpoint.SsCompDes = &((CONFIG_SS_DESC *)ConfigDesc)->EndpointDescriptorCompOut;
    } else {
      In                                         = &((CONFIG_NOT_SS_DESC *)ConfigDesc)->EndpointDescriptorIn;
      Out                                        = &((CONFIG_NOT_SS_DESC *)ConfigDesc)->EndpointDescriptorOut;
      InEpIndex                                  = (In->EndpointAddress & (~USB_ENDPOINT_DIR_IN)) << 1;
      OutEpIndex                                 = ((Out->EndpointAddress & (~USB_ENDPOINT_DIR_IN)) << 1) -1;
      gCdnspDevice->Eps[InEpIndex].Endpoint.Des  = In;
      gCdnspDevice->Eps[OutEpIndex].Endpoint.Des = Out;
    }
  } else {
    return EFI_NOT_READY;
  }

  Status = CdnspEpEnable (&gCdnspDevice->Eps[InEpIndex].Endpoint, In);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Enable Ep%d Failed(%d)\n", InEpIndex, Status));
    return Status;
  }

  Status = CdnspEpEnable (&gCdnspDevice->Eps[OutEpIndex].Endpoint, Out);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Enable Ep%d Failed(%d)\n", OutEpIndex, Status));
    CdnspEpDisable (&gCdnspDevice->Eps[InEpIndex].Endpoint);
    return Status;
  }

  NotifyEvent (EfiUsbMsgSetupPacket, NULL, NULL, CtrlRequest);

  return Status;
}

EFI_STATUS
CdnspResetConfig (
  EFI_USB_DEVICE_REQUEST  *CtrlRequest
  )
{
  CDNSP_DEVICE  *CdnspDevice = CdnspGetInstance ();
  EFI_STATUS    Status       = EFI_SUCCESS;

  if (NULL != CdnspDevice) {
    CdnspDisableInterfaceEndpoint (CdnspDevice);
  } else {
    return EFI_NOT_READY;
  }

  // NotifyEvent(EfiUsbMsgSetupPacket, NULL, NULL, CtrlRequest);

  return Status;
}

VOID *
CdnspGetCtrlResponseBuffer (
  VOID
  )
{
  CDNSP_DEVICE  *CdnspDevice = CdnspGetInstance ();

  if ((NULL != CdnspDevice) && (NULL != CdnspDevice->SetupBuf)) {
    return CdnspDevice->SetupBuf;
  } else {
    return NULL;
  }
}

EFI_STATUS
CdnspGetSpeedInfo (
  UINT8  *MaxSpeed,
  UINT8  *Speed
  )
{
  CDNSP_DEVICE  *CdnspDevice = CdnspGetInstance ();

  if (NULL == CdnspDevice) {
    return EFI_NOT_READY;
  }

  *MaxSpeed = CdnspDevice->MaxSpeed;
  *Speed    = CdnspDevice->Speed;

  return EFI_SUCCESS;
}

VOID
CdnspSetMem (
  VOID   *Buffer,
  UINTN  Length,
  UINT8  Value
  )
{
  gBS->SetMem (Buffer, Length, 0);
}

VOID
CdnspCopyMem (
  VOID   *Destination,
  VOID   *Source,
  UINTN  Length
  )
{
  gBS->CopyMem (Destination, Source, Length);
}

EFI_STATUS
CdnspInit (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  CdnspDevice->OtgRegs = (CDNSP_OTG_REGS *)CdnspDevice->BaseAddress;

  CdnspConfigDeviceMode (CdnspDevice);

  CdnspDevice->SgSupport = FALSE;
  CdnspDevice->Speed     = UsbBusSpeedUnknown;
  CdnspDevice->LpmCable  = 0;
  CdnspDevice->SetupBuf  = UncachedAllocateAlignedZeroPool (CDNSP_EP0_SETUP_SIZE, CDNSP_EP0_SETUP_SIZE);

  if (!CdnspDevice->SetupBuf) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = CdnspGenSetup (CdnspDevice);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Generic Initialization Failed(%r)\n", Status));
    goto FreeSetup;
  }

  Status = CdnspInitEndpoints (CdnspDevice);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Endpoint Initialization Failed(%r)\n", Status));
    goto HaltDevice;
  }

  Status = CdnspRun (CdnspDevice, CdnspDevice->MaxSpeed);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Run failed(%r)\n", Status));
    goto FreeEndpoint;
  }

  Status = CdnspPullUp (CdnspDevice, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Set Vbus Failed(%r)\n", Status));
    goto FreeEndpoint;
  }

  gCdnspDevice                                    = CdnspDevice;
  CdnspDevice->StdDeviceOps.SetAddress            = CdnspEp0SetAddress;
  CdnspDevice->StdDeviceOps.SetConfig             = CdnspSetConfig;
  CdnspDevice->StdDeviceOps.ResetConfig           = CdnspResetConfig;
  CdnspDevice->StdDeviceOps.SendCtrlResponse      = CdnspSendCtrlResponse;
  CdnspDevice->StdDeviceOps.GetSpeedInfo          = CdnspGetSpeedInfo;
  CdnspDevice->StdDeviceOps.GetCtrlResponseBuffer = CdnspGetCtrlResponseBuffer;
  CdnspDevice->StdDeviceOps.SetMem                = CdnspSetMem;
  CdnspDevice->StdDeviceOps.CopyMem               = CdnspCopyMem;

  return Status;

FreeEndpoint:
  CdnspFreeEndpoints ();
HaltDevice:
  CdnspHalt (CdnspDevice);
  CdnspReset (CdnspDevice);
  CdnspMemCleanup (CdnspDevice);
FreeSetup:
  UncachedSafeFreePool (CdnspDevice->SetupBuf);
  return Status;
}
