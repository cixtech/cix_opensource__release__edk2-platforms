/** @file
 *
 *  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 **/

#include "Cdnsp.h"

STATIC
INT16
CdnspIndexToEpIndex (
  UINT16  Index
  )
{
  if (!(Index & USB_ENDPOINT_NUMBER_MASK)) {
    return 0;
  }

  return ((Index & USB_ENDPOINT_NUMBER_MASK) * 2) +
         (Index & USB_ENDPOINT_DIR_MASK ? 1 : 0) -1;
}

EFI_STATUS
CdnspStatusState (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspDevice->Ep0Stage               = CDNSP_STATUS_STAGE;
  CdnspDevice->Ep0Preq.Request.Length = 0;

  return CdnspInternalEpEnqueue (CdnspDevice->Ep0Preq.Ep, &CdnspDevice->Ep0Preq);
}

EFI_STATUS
CdnspEp0SetAddress (
  UINT32  Addr
  )
{
  CDNSP_SLOT_CTX  *SlotCtx;
  UINT32          SlotState;
  CDNSP_DEVICE    *CdnspDevice = CdnspGetInstance ();
  EFI_STATUS      Status       = EFI_SUCCESS;

  if (NULL == CdnspDevice) {
    return EFI_NOT_READY;
  }

  CdnspDevice->DeviceAddress = Addr;
  //
  // TODO kernel code initial double
  //
  SlotCtx   = CdnspGetSlotCtx (&CdnspDevice->OutCtx);
  SlotState = GET_SLOT_STATE (SlotCtx->DevState);
  if (SlotState == SLOT_STATE_ADDRESSED) {
    CdnspResetDevice (CdnspDevice);
  }

  Status = CdnspSetupDevice (CdnspDevice, SETUP_CONTEXT_ADDRESS);

  return Status;
}

STATIC
EFI_STATUS
CdnspEp0HandleStatus (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl
  )
{
  CDNSP_EP  *CdnspEp;
  INT16     EpSts = 0;
  UINT16    *Response;
  UINT32    Recipient;
  UINT16    UsbStatus = 0;

  Recipient = Ctrl->RequestType & USB_RECIP_MASK;

  switch (Recipient) {
    case USB_TARGET_DEVICE:
      UsbStatus  = CdnspDevice->IsSelfpowered;
      UsbStatus |= CdnspDevice->MayWakeup << USB_DEVICE_REMOTE_WAKEUP;
      //
      // do not support u1/u2
      //
      break;
    case USB_TARGET_INTERFACE:
      UsbStatus = 0;
      break;
    case USB_TARGET_ENDPOINT:
      EpSts   = CdnspIndexToEpIndex (Ctrl->Index);
      CdnspEp = &CdnspDevice->Eps[EpSts];
      EpSts   = GET_EP_CTX_STATE (CdnspEp->OutCtx);

      if (EpSts == EP_STATE_HALTED) {
        UsbStatus = BIT (USB_FEATURE_ENDPOINT_HALT);
      }

      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  Response  = (UINT16 *)CdnspDevice->SetupBuf;
  *Response = UsbStatus;

  CdnspDevice->Ep0Preq.Request.Length = sizeof (*Response);
  CdnspDevice->Ep0Preq.Request.Buf    = CdnspDevice->SetupBuf;

  return CdnspInternalEpEnqueue (CdnspDevice->Ep0Preq.Ep, &CdnspDevice->Ep0Preq);
}

STATIC
EFI_STATUS
CdnspEp0HandleFeatureDevice (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl,
  INT32               Set
  )
{
  USB_DEVICE_STATE  DeviceState = GetDeviceState ();

  switch (Ctrl->Value) {
    case USB_DEVICE_REMOTE_WAKEUP:
      CdnspDevice->MayWakeup = !!Set;
      break;
    case USB_DEVICE_U1_ENABLE:
      if ((DeviceState != USB_STATE_CONFIGURED) || (CdnspDevice->Speed < UsbBusSpeedSuper)) {
        return EFI_UNSUPPORTED;
      }

      //
      // Ignore Power related ops
      //
      break;
    case USB_DEVICE_U2_ENABLE:
      if ((DeviceState != USB_STATE_CONFIGURED) || (CdnspDevice->Speed < UsbBusSpeedSuper)) {
        return EFI_UNSUPPORTED;
      }

      //
      // Ignore Power related ops
      //
      break;
    case USB_DEVICE_LTM_ENABLE:
      return EFI_UNSUPPORTED;
    case USB_DEVICE_TEST_MODE:
      //
      // not support test mode
      //
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CdnspEp0HandleFeatureIntf (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl,
  INT32               Set
  )
{
  UINT16  Value = Ctrl->Value;
  UINT16  Index = Ctrl->Index;

  switch (Value) {
    case USB_INTRF_FUNC_SUSPEND:
      //
      // Not Support just return success(function suspend)
      //
      if (Index & USB_INTRF_FUNC_SUSPEND_RW) {
        CdnspDevice->MayWakeup++;
      } else {
        if (CdnspDevice->MayWakeup > 0) {
          CdnspDevice->MayWakeup--;
        }
      }

      return EFI_SUCCESS;
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CdnspEp0HandleFeatureEndpoint (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl,
  INT32               Set
  )
{
  CDNSP_EP  *CdnspEp;
  UINT16    Value;

  Value   = Ctrl->Value;
  CdnspEp = &CdnspDevice->Eps[CdnspIndexToEpIndex (Ctrl->Index)];

  switch (Value) {
    case USB_FEATURE_ENDPOINT_HALT:
      if (!Set && (CdnspEp->EpState & EP_WEDGE)) {
        CdnspHaltEndpoint (CdnspDevice, CdnspEp, 0);
        CdnspHaltEndpoint (CdnspDevice, CdnspEp, 1);
        break;
      }

      return CdnspHaltEndpoint (CdnspDevice, CdnspEp, Set);
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CdnspEp0HandleFeature (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl,
  INT32               Set
  )
{
  switch (Ctrl->RequestType & USB_RECIP_MASK) {
    case USB_TARGET_DEVICE:
      return CdnspEp0HandleFeatureDevice (CdnspDevice, Ctrl, Set);
    case USB_TARGET_INTERFACE:
      return CdnspEp0HandleFeatureIntf (CdnspDevice, Ctrl, Set);
    case USB_TARGET_ENDPOINT:
      return CdnspEp0HandleFeatureEndpoint (CdnspDevice, Ctrl, Set);
    default:
      return EFI_UNSUPPORTED;
  }
}

STATIC
EFI_STATUS
CdnspEp0SetSel (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl
  )
{
  USB_DEVICE_STATE  DeviceState = GetDeviceState ();
  UINT16            Length;

  if (DeviceState == USB_STATE_DEFAULT) {
    return EFI_UNSUPPORTED;
  }

  Length = Ctrl->Length;

  if (Length != 6) {
    return EFI_INVALID_PARAMETER;
  }

  CdnspDevice->Ep0Preq.Request.Length = 6;
  CdnspDevice->Ep0Preq.Request.Buf    = CdnspDevice->SetupBuf;

  return CdnspInternalEpEnqueue (CdnspDevice->Ep0Preq.Ep, &CdnspDevice->Ep0Preq);
}

EFI_STATUS
CdnspSendCtrlResponse (
  INT32  BufferLength,
  VOID   *Buffer
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  CDNSP_DEVICE  *CdnspDevice = CdnspGetInstance ();

  if (NULL == CdnspDevice) {
    return EFI_NOT_READY;
  }

  if (BufferLength > 0) {
    if (Buffer != NULL) {
      gBS->SetMem (CdnspDevice->SetupBuf, CDNSP_EP0_SETUP_SIZE, 0);
      gBS->CopyMem (CdnspDevice->SetupBuf, Buffer, BufferLength);
    }

    CdnspDevice->Ep0Preq.Request.Length = BufferLength;
    CdnspDevice->Ep0Preq.Request.Buf    = CdnspDevice->SetupBuf;

    Status = CdnspInternalEpEnqueue (CdnspDevice->Ep0Preq.Ep, &CdnspDevice->Ep0Preq);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Ep0 Send Ctrl Response Failed(%r)\n", Status));
    }
  }

  return Status;
}

STATIC
EFI_STATUS
CdnspEp0StdRequest (
  CDNSP_DEVICE        *CdnspDevice,
  USB_DEVICE_REQUEST  *Ctrl
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  switch (Ctrl->Request) {
    case USB_REQ_SET_ADDRESS:
      Status = HandleCh9Request (Ctrl);
      break;
    case USB_REQ_SET_CONFIG:
      Status = HandleCh9Request (Ctrl);
      break;
    case USB_REQ_GET_DESCRIPTOR:
      Status = HandleCh9Request (Ctrl);
      break;
    case USB_REQ_GET_STATUS:
      Status = CdnspEp0HandleStatus (CdnspDevice, Ctrl);
      break;
    case USB_REQ_SET_ISOCH_DELAY:
      //
      // do not support isoc do noting
      //
      break;
    case USB_REQ_SET_SEL:
      Status = CdnspEp0SetSel (CdnspDevice, Ctrl);
      break;
    case USB_REQ_CLEAR_FEATURE:
      Status = CdnspEp0HandleFeature (CdnspDevice, Ctrl, 0);
      break;
    case USB_REQ_SET_FEATURE:
      Status = CdnspEp0HandleFeature (CdnspDevice, Ctrl, 1);
      break;
    case USB_REQ_SET_INTERFACE:
      //
      // do not support composite device return success not change interface
      //
      break;
    case USB_REQ_GET_INTERFACE:
      Status = HandleCh9Request (Ctrl);
      break;
    case USB_REQ_GET_CONFIG:
      Status = HandleCh9Request (Ctrl);
      break;
  }

  return Status;
}

STATIC
VOID
CdnspEp0Stall (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_EP       *Ep0 = &CdnspDevice->Eps[0];
  CDNSP_REQUEST  *CdnspRequest;

  if (IsListEmpty (&Ep0->PendingList)) {
    CdnspRequest = NULL;
  } else {
    CdnspRequest = (CDNSP_REQUEST *)GetFirstNode (&Ep0->PendingList);
  }

  if (CdnspDevice->ThreeStageSetup) {
    CdnspHaltEndpoint (CdnspDevice, Ep0, TRUE);
    if (CdnspRequest) {
      CdnspGiveback (Ep0, CdnspRequest, ENDPOINT_CONNRESET);
    }
  } else {
    Ep0->EpState |= EP0_HALTED_STATUS;
    if (CdnspRequest) {
      RemoveEntryList (&CdnspRequest->Request.Link);
    }

    CdnspStatusState (CdnspDevice);
  }
}

VOID
CdnspSetupAnalyze (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  USB_DEVICE_REQUEST  *Ctrl    = &CdnspDevice->Setup;
  EFI_STATUS          Status   = EFI_SUCCESS;
  UINT16              Length   = 0;
  USB_REQUEST         *Request = NULL;

  if (GetDeviceState () == USB_STATE_NOTATTACHED) {
    DEBUG ((DEBUG_ERROR, "Setup Detected In Unattached State\n"));
    Status = EFI_NOT_READY;
    goto Out;
  }

  if (CdnspDevice->Eps[0].EpState & EP_HALTED) {
    CdnspHaltEndpoint (CdnspDevice, &CdnspDevice->Eps[0], 0);
  }

  if (!IsListEmpty (&CdnspDevice->Eps[0].PendingList)) {
    DEBUG ((DEBUG_ERROR, "Remove Old Request\n"));
    Request = (USB_REQUEST *)GetFirstNode (&CdnspDevice->Eps[0].PendingList);
    CdnspEpDequeueRequest (&CdnspDevice->Eps[0].Endpoint, Request);
  }

  Length = Ctrl->Length;
  if (!Length) {
    CdnspDevice->ThreeStageSetup = FALSE;
    CdnspDevice->Ep0ExpectIn     = FALSE;
  } else {
    CdnspDevice->ThreeStageSetup = TRUE;
    CdnspDevice->Ep0ExpectIn     = !!(Ctrl->RequestType & USB_DIR_IN);
  }

  if ((Ctrl->RequestType & USB_TYPE_MASK) == USB_REQ_TYPE_STANDARD) {
    Status = CdnspEp0StdRequest (CdnspDevice, Ctrl);
  } else {
    Status = HandleEp0NoStdRequest (Ctrl, CdnspDevice->SetupBuf);
  }

Out:
  if (EFI_ERROR (Status)) {
    CdnspEp0Stall (CdnspDevice);
  } else if (!Length) {
    CdnspStatusState (CdnspDevice);
  }
}
