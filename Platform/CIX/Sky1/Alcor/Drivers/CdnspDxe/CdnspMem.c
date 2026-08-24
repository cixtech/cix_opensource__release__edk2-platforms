/**@file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Cdnsp.h"

CDNSP_EP_CTX *
CdnspGetEpCtx (
  CDNSP_CONTAINER_CTX  *Ctx,
  UINT32               EpIndex
  )
{
  EpIndex++;
  if (Ctx->Type == CDNSP_CTX_TYPE_INPUT) {
    EpIndex++;
  }

  return (CDNSP_EP_CTX *)(Ctx->Bytes + (EpIndex * Ctx->CtxSize));
}

CDNSP_SLOT_CTX *
CdnspGetSlotCtx (
  CDNSP_CONTAINER_CTX  *Ctx
  )
{
  if (Ctx->Type == CDNSP_CTX_TYPE_DEVICE) {
    return (CDNSP_SLOT_CTX *)Ctx->Bytes;
  }

  return (CDNSP_SLOT_CTX *)(Ctx->Bytes + Ctx->CtxSize);
}

CDNSP_INPUT_CONTROL_CTX
*
CdnspGetInputControlCtx (
  CDNSP_CONTAINER_CTX  *Ctx
  )
{
  if (Ctx->Type != CDNSP_CTX_TYPE_INPUT) {
    return NULL;
  }

  return (CDNSP_INPUT_CONTROL_CTX *)Ctx->Bytes;
}

STATIC
VOID
CdnspSegmentFree (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_SEGMENT  *Seg
  )
{
  if (Seg->Trbs) {
    UncachedSafeFreePool (Seg->Trbs);
  }

  if (Seg->BouncdBuf) {
    UncachedSafeFreePool (Seg->BouncdBuf);
  }

  FreePool (Seg);
}

STATIC
CDNSP_SEGMENT *
CdnspSegmentAlloc (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        CycleState,
  UINT32        MaxPacket
  )
{
  CDNSP_SEGMENT  *Seg;
  INT32          Index = 0;

  Seg = (CDNSP_SEGMENT *)AllocateZeroPool (sizeof (CDNSP_SEGMENT));
  if (!Seg) {
    return NULL;
  }

  Seg->Trbs = UncachedAllocateAlignedZeroPool (TRB_SEGMENT_SIZE, TRB_SEGMENT_SIZE);
  if (!Seg->Trbs) {
    FreePool (Seg);
    return NULL;
  }

  if (MaxPacket) {
    Seg->BouncdBuf = UncachedAllocateAlignedZeroPool (MaxPacket, MaxPacket);
    if (!Seg->BouncdBuf) {
      goto AllocBounceError;
    }
  }

  // TRB_CYCLE bit is used to mark the enqueue pointer location of
  // a transfer or command ring
  if (CycleState == 0) {
    for ( ; Index < TRBS_PER_SEGMENT; Index++) {
      Seg->Trbs[Index].Link.Control |= TRB_CYCLE;
    }
  }

  Seg->Next = NULL;
  return Seg;
AllocBounceError:
  UncachedSafeFreePool (Seg->Trbs);
  FreePool (Seg);
  return NULL;
}

STATIC
VOID
CdnspFreeSegmentsForRing (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_SEGMENT  *First
  )
{
  CDNSP_SEGMENT  *Seg;

  Seg = First->Next;

  while ((Seg != NULL) && (Seg != First)) {
    CDNSP_SEGMENT  *Next = Seg->Next;
    CdnspSegmentFree (CdnspDevice, Seg);
    Seg = Next;
  }

  CdnspSegmentFree (CdnspDevice, First);
}

STATIC
VOID
CdnspLinkSegments (
  CDNSP_DEVICE     *CdnspDevice,
  CDNSP_SEGMENT    *Prev,
  CDNSP_SEGMENT    *Next,
  CDNSP_RING_TYPE  Type
  )
{
  CDNSP_LINK_TRB  *Link;
  UINT32          Value;

  if (!Prev || !Next) {
    return;
  }

  Prev->Next = Next;
  if (Type != TYPE_EVENT) {
    Link             = &Prev->Trbs[TRBS_PER_SEGMENT - 1].Link;
    Link->SegmentPtr = (UINT64)Next->Trbs;
    Value            = Link->Control;
    Value           &= ~TRB_TYPE_BITMASK;
    Value           |= TRB_TYPE (TRB_LINK);
    Link->Control    = Value;
  }
}

STATIC
EFI_STATUS
CdnspAllocSegmentForRing (
  CDNSP_DEVICE     *CdnspDevice,
  CDNSP_SEGMENT    **First,
  CDNSP_SEGMENT    **Last,
  UINT32           NumSegs,
  UINT32           CycleState,
  CDNSP_RING_TYPE  Type,
  UINT32           MaxPacket
  )
{
  CDNSP_SEGMENT  *Prev;

  Prev = CdnspSegmentAlloc (CdnspDevice, CycleState, MaxPacket);
  if (!Prev) {
    return EFI_OUT_OF_RESOURCES;
  }

  NumSegs--;
  *First = Prev;

  while (NumSegs > 0) {
    CDNSP_SEGMENT  *Next;
    Next = CdnspSegmentAlloc (CdnspDevice, CycleState, MaxPacket);
    if (!Next) {
      CdnspFreeSegmentsForRing (CdnspDevice, *First);
      return EFI_OUT_OF_RESOURCES;
    }

    CdnspLinkSegments (CdnspDevice, Prev, Next, Type);
    Prev = Next;
    NumSegs--;
  }

  CdnspLinkSegments (CdnspDevice, Prev, *First, Type);
  *Last = Prev;
  return EFI_SUCCESS;
}

VOID
CdnspInitializeRingInfo (
  CDNSP_RING  *Ring
  )
{
  Ring->Enqueue = Ring->FirstSeg->Trbs;
  Ring->EnqSeg  = Ring->FirstSeg;
  Ring->DeqSeg  = Ring->EnqSeg;
  Ring->Dequeue = Ring->Enqueue;

  Ring->CycleState = 1;

  Ring->NumTrbsFree = Ring->NumSegs * (TRBS_PER_SEGMENT -1) -1;
}

STATIC
CDNSP_RING *
CdnspRingAlloc (
  CDNSP_DEVICE     *CdnspDevice,
  UINT32           NumSegs,
  CDNSP_RING_TYPE  Type,
  UINT32           MaxPacket
  )
{
  CDNSP_RING  *Ring;
  EFI_STATUS  Status = EFI_SUCCESS;

  Ring = (CDNSP_RING *)AllocateZeroPool (sizeof (CDNSP_RING));
  if (!Ring) {
    return NULL;
  }

  Ring->NumSegs         = NumSegs;
  Ring->BounceBufLength = MaxPacket;
  Ring->Type            = Type;
  InitializeListHead (&Ring->TdList);

  if (NumSegs == 0) {
    return Ring;
  }

  Status = CdnspAllocSegmentForRing (
             CdnspDevice,
             &Ring->FirstSeg,
             &Ring->LastSeg,
             NumSegs,
             1,
             Type,
             MaxPacket
             );
  if (EFI_ERROR (Status)) {
    goto Fail;
  }

  //
  // Event Ring Manager is not work as defined in 4.9.4,not use link TRB
  // LINK Toggle : when set 1, the control should toggle its interpretation of
  // the cycle bit, when cleared to 0, continue to next segment using the current
  // interpretation
  //
  if (Type != TYPE_EVENT) {
    Ring->LastSeg->Trbs[TRBS_PER_SEGMENT -1].Link.Control |= LINK_TOGGLE;
  }

  CdnspInitializeRingInfo (Ring);
  return Ring;
Fail:
  FreePool (Ring);
  return NULL;
}

STATIC
EFI_STATUS
CdnspAllocErst (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *EvtRing,
  CDNSP_ERST    *Erst
  )
{
  CDNSP_ERST_ENTRY  *Entry;
  CDNSP_SEGMENT     *Seg;
  UINT32            Value;
  UINT32            Size;

  Size          = sizeof (CDNSP_ERST_ENTRY) * EvtRing->NumSegs;
  Erst->Entries = UncachedAllocateZeroPool (Size);
  if (!Erst->Entries) {
    return EFI_OUT_OF_RESOURCES;
  }

  Erst->NumEntries = EvtRing->NumSegs;
  Seg              = EvtRing->FirstSeg;
  for (Value = 0; Value < EvtRing->NumSegs; Value++) {
    Entry          = &Erst->Entries[Value];
    Entry->SegAddr = (UINT64)Seg->Trbs;
    Entry->SegSize = (UINT32)TRBS_PER_SEGMENT;
    Entry->Rsvd    = 0;
    Seg            = Seg->Next;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
CdnspSetEventDeq (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT64  Deq;
  UINT64  Temp = 0;

  Deq   = (UINT64)CdnspDevice->EventRing->Dequeue;
  Temp  = CdnspRead64 (&CdnspDevice->IntrRegs->ErDq);
  Temp &= ERST_PTR_MASK;
  Temp &= ~ERST_EHB;

  CdnspWrite64 (((UINT64)Deq & (UINT64) ~ERST_PTR_MASK)| Temp, &CdnspDevice->IntrRegs->ErDq);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%llx)\n", &CdnspDevice->IntrRegs->ErDq, (((UINT64)Deq & (UINT64) ~ERST_PTR_MASK)| Temp)));
}

STATIC
VOID
CdnspAddInPort (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_PORT    *Port,
  UINT32        *Addr
  )
{
  UINT32  Temp, PortOffset, PortCount;

  Temp          = CdnspRead (Addr);
  Port->MajRev  = CDNSP_EXT_PORT_MAJOR (Temp);
  Port->MinRev  = CDNSP_EXT_PORT_MINOR (Temp);
  Temp          = CdnspRead (Addr + 2);
  PortOffset    = CDNSP_EXT_PORT_OFF (Temp);
  PortCount     = CDNSP_EXT_PORT_COUNT (Temp);
  Port->PortNum = PortOffset;
  Port->Exist   = 1;
}

STATIC
EFI_STATUS
CdnspSetupPortArrays (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  VOID    *Base  = NULL;
  UINT32  Offset = 0;
  UINT32  Index  = 0;
  UINT32  Temp   = 0;

  Base                    = &CdnspDevice->CapsRegs->HcCapBase;
  Offset                  = CdnspFindNextExtCap (Base, 0, EXT_CAP_CFG_DEV_20PORT_CAP_ID);
  CdnspDevice->Port20Regs = Base + Offset;

  DEBUG ((DEBUG_COMMON_LOG, "Usb2.0 Port Peripheral Configuration(0x%x)\n", CdnspDevice->Port20Regs));

  Offset                  = CdnspFindNextExtCap (Base, 0, D_XEC_CFG_3XPORT_CAP);
  CdnspDevice->Port3xRegs = Base + Offset;

  DEBUG ((DEBUG_COMMON_LOG, "Usb3.0 Port Peripheral Configuration(0x%x)\n", CdnspDevice->Port3xRegs));

  Offset = 0;

  for ( ; Index < 2; Index++) {
    Offset = CdnspFindNextExtCap (Base, Offset, EXT_CAPS_PROTOCOL);
    Temp   = CdnspRead (Base + Offset);

    // Usb3Port.PortNum supposed to be 2  Usb2Port.PortNum supposed to be 1
    if ((CDNSP_EXT_PORT_MAJOR (Temp) == 0x03) &&
        !CdnspDevice->Usb3Port.PortNum)
    {
      CdnspAddInPort (CdnspDevice, &CdnspDevice->Usb3Port, Base + Offset);
    }

    if ((CDNSP_EXT_PORT_MAJOR (Temp) == 0x02) &&
        !CdnspDevice->Usb2Port.PortNum)
    {
      CdnspAddInPort (CdnspDevice, &CdnspDevice->Usb2Port, Base + Offset);
    }
  }

  if (!CdnspDevice->Usb2Port.Exist || !CdnspDevice->Usb3Port.Exist) {
    DEBUG ((DEBUG_ERROR, "No Port In Usb Device Exit\n"));
    return EFI_NOT_FOUND;
  }

  CdnspDevice->Usb2Port.Regs = (CDNSP_PORT_REGS *)(&CdnspDevice->OpRegs->PortRegisterBase + NUM_PORT_REGS *
                                                   (CdnspDevice->Usb2Port.PortNum -1));

  DEBUG ((DEBUG_COMMON_LOG, "Usb2.0 Port Status And Control Register(%x)\n", CdnspDevice->Usb2Port.Regs));

  CdnspDevice->Usb3Port.Regs = (CDNSP_PORT_REGS *)(&CdnspDevice->OpRegs->PortRegisterBase + NUM_PORT_REGS *
                                                   (CdnspDevice->Usb3Port.PortNum -1));

  DEBUG ((DEBUG_COMMON_LOG, "Usb3.0 Port Status And Control Register(%x)\n", CdnspDevice->Usb3Port.Regs));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CdnspInitDeviceCtx (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32  Size = HCC_64BYTE_CONTEXT (CdnspDevice->HccParams1) ? 2048 : 1024;

  CdnspDevice->OutCtx.Type    = CDNSP_CTX_TYPE_DEVICE;
  CdnspDevice->OutCtx.Size    = Size;
  CdnspDevice->OutCtx.CtxSize = CTX_SIZE (CdnspDevice->HccParams1);
  CdnspDevice->OutCtx.Bytes   = UncachedAllocateAlignedZeroPool (Size, 64);

  if (!CdnspDevice->OutCtx.Bytes) {
    return EFI_OUT_OF_RESOURCES;
  }

  CdnspDevice->InCtx.Type    = CDNSP_CTX_TYPE_INPUT;
  CdnspDevice->InCtx.CtxSize = CdnspDevice->OutCtx.CtxSize;
  CdnspDevice->InCtx.Size    = Size + CdnspDevice->InCtx.CtxSize;
  CdnspDevice->InCtx.Bytes   = UncachedAllocateAlignedZeroPool (CdnspDevice->InCtx.Size, 64);

  if (!CdnspDevice->InCtx.Bytes) {
    UncachedSafeFreePool (CdnspDevice->OutCtx.Bytes);
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CdnspAllocPrivDevice (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  Status = CdnspInitDeviceCtx (CdnspDevice);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CdnspDevice->Eps[0].Ring = CdnspRingAlloc (CdnspDevice, 2, TYPE_CTRL, 0);
  if (!CdnspDevice->Eps[0].Ring) {
    goto Fail;
  }

  DEBUG ((DEBUG_COMMON_LOG, "Ep0 Dequeue Address(0x%llx)\n", (UINT64)CdnspDevice->Eps[0].Ring->Dequeue));

  CdnspDevice->DevContextPtr[1] = (UINT64)CdnspDevice->OutCtx.Bytes;
  CdnspDevice->Cmd.InCtx        = &CdnspDevice->InCtx;

  return Status;
Fail:
  UncachedSafeFreePool (CdnspDevice->OutCtx.Bytes);
  UncachedSafeFreePool (CdnspDevice->InCtx.Bytes);

  return Status;
}

VOID
CdnspFreePrivDevice (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspDevice->DevContextPtr[1] = 0;
  CdnspFreeEndpointRings (CdnspDevice, &CdnspDevice->Eps[0]);
  if (CdnspDevice->InCtx.Bytes) {
    UncachedSafeFreePool (CdnspDevice->InCtx.Bytes);
  }

  if (CdnspDevice->OutCtx.Bytes) {
    UncachedSafeFreePool (CdnspDevice->OutCtx.Bytes);
  }

  CdnspDevice->InCtx.Bytes  = NULL;
  CdnspDevice->OutCtx.Bytes = NULL;
}

STATIC
VOID
CdnspLinkRings (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_RING     *Ring,
  CDNSP_SEGMENT  *First,
  CDNSP_SEGMENT  *Last,
  UINT32         NumSegs
  )
{
  CDNSP_SEGMENT  *Next;

  if (!Ring || !First || !Last) {
    return;
  }

  Next = Ring->EnqSeg->Next;
  CdnspLinkSegments (CdnspDevice, Ring->EnqSeg, First, Ring->Type);
  CdnspLinkSegments (CdnspDevice, Last, Next, Ring->Type);
  Ring->NumSegs     += NumSegs;
  Ring->NumTrbsFree += (TRBS_PER_SEGMENT -1) *NumSegs;

  if ((Ring->Type != TYPE_EVENT) && (Ring->EnqSeg == Ring->LastSeg)) {
    Ring->LastSeg->Trbs[TRBS_PER_SEGMENT -1].Link.Control &= ~LINK_TOGGLE;
    Last->Trbs[TRBS_PER_SEGMENT -1].Link.Control          |= LINK_TOGGLE;
    Ring->LastSeg                                          = Last;
  }
}

EFI_STATUS
CdnspRingExpansion (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring,
  UINT32        NumTrb
  )
{
  UINT32         NumSegsNeed;
  CDNSP_SEGMENT  *First;
  CDNSP_SEGMENT  *Last;
  UINT32         NumSegs;
  EFI_STATUS     Status;

  NumSegsNeed = (NumTrb + (TRBS_PER_SEGMENT -1) -1) / (TRBS_PER_SEGMENT -1);

  NumSegs = MAX (Ring->NumSegs, NumSegsNeed);

  Status = CdnspAllocSegmentForRing (
             CdnspDevice,
             &First,
             &Last,
             NumSegs,
             Ring->CycleState,
             Ring->Type,
             Ring->BounceBufLength
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CdnspLinkRings (CdnspDevice, Ring, First, Last, NumSegs);

  return Status;
}

STATIC
VOID
CdnspFreeErst (
  CDNSP_ERST  *Erst
  )
{
  if (Erst->Entries) {
    UncachedSafeFreePool (Erst->Entries);
  }
}

STATIC
VOID
CdnspRingFree (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring
  )
{
  if (!Ring) {
    return;
  }

  if (Ring->FirstSeg) {
    CdnspFreeSegmentsForRing (CdnspDevice, Ring->FirstSeg);
  }

  FreePool (Ring);
}

VOID
CdnspMemCleanup (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspFreePrivDevice (CdnspDevice);
  CdnspFreeErst (&CdnspDevice->Erst);
  if (CdnspDevice->EventRing) {
    CdnspRingFree (CdnspDevice, CdnspDevice->EventRing);
  }

  CdnspDevice->EventRing = NULL;

  if (CdnspDevice->CommandRing) {
    CdnspRingFree (CdnspDevice, CdnspDevice->CommandRing);
  }

  CdnspDevice->CommandRing = NULL;

  UncachedSafeFreePool (CdnspDevice->DevContextPtr);
  CdnspDevice->DevContextPtr = NULL;

  CdnspDevice->Usb2Port.Exist   = 0;
  CdnspDevice->Usb3Port.Exist   = 0;
  CdnspDevice->Usb2Port.PortNum = 0;
  CdnspDevice->Usb3Port.PortNum = 0;
  CdnspDevice->ActivePort       = NULL;
}

VOID
CdnspCopyEp0DequeueIntoInputCtx (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_EP_CTX  *Ep0Ctx = CdnspDevice->Eps[0].InCtx;
  CDNSP_RING    *EpRing = CdnspDevice->Eps[0].Ring;

  Ep0Ctx->Deq = (UINT64)(EpRing->Enqueue) | EpRing->CycleState;

  DEBUG ((DEBUG_TRB_LOG, "Ep0 Deq(%llx)\n", Ep0Ctx->Deq));
}

EFI_STATUS
CdnspSetupAddressablePrivDev (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_SLOT_CTX  *SlotCtx;
  CDNSP_EP_CTX    *Ep0Ctx;
  UINT32          MaxPackets, Port;

  Ep0Ctx  = CdnspGetEpCtx (&CdnspDevice->InCtx, 0);
  SlotCtx = CdnspGetSlotCtx (&CdnspDevice->InCtx);

  SlotCtx->DevInfo |= LAST_CTX (1);

  switch (CdnspDevice->Speed) {
    case UsbBusSpeedSuperPlus:
      SlotCtx->DevInfo |= SLOT_SPEED_SSP;
      MaxPackets        = MAX_PACKET (512);
      break;
    case UsbBusSpeedSuper:
      SlotCtx->DevInfo |= SLOT_SPEED_SS;
      MaxPackets        = MAX_PACKET (512);
      break;
    case UsbBusSpeedHigh:
      SlotCtx->DevInfo |= SLOT_SPEED_HS;
      MaxPackets        = MAX_PACKET (64);
      break;
    case UsbBusSpeedFull:
      SlotCtx->DevInfo |= SLOT_SPEED_FS;
      MaxPackets        = MAX_PACKET (64);
      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  Port              = DEV_PORT (CdnspDevice->ActivePort->PortNum);
  SlotCtx->DevPort |= Port;
  SlotCtx->DevState = CdnspDevice->DeviceAddress & DEV_ADDR_MASK;

  Ep0Ctx->TxInfo   = EP_AVG_TRB_LENGTH (0x8);
  Ep0Ctx->EpInfo2  = EP_TYPE (CTRL_EP);
  Ep0Ctx->EpInfo2 |= MAX_BURST (0) | ERROR_COUNT (3) | MaxPackets;
  Ep0Ctx->Deq      = (UINT64)CdnspDevice->Eps[0].Ring->FirstSeg->Trbs |
                     CdnspDevice->Eps[0].Ring->CycleState;

  return EFI_SUCCESS;
}

STATIC
UINT32
CdnspGetEndpointType (
  USB_ENDPOINT_DESCRIPTOR  *Desc
  )
{
  INT32  DirIn = UsbEndpointDirIN (Desc);

  switch (UsbEndpointType (Desc)) {
    case USB_ENDPOINT_CONTROL:
      return CTRL_EP;
    case USB_ENDPOINT_BULK:
      return DirIn ? BULK_IN_EP : BULK_OUT_EP;
    case USB_ENDPOINT_INTERRUPT:
      return DirIn ? INT_IN_EP : INT_OUT_EP;
    case USB_ENDPOINT_ISO:
      DEBUG ((DEBUG_ERROR, "Invalid Endpoint Isoc Type\n"));
      return 0;
    default:
      DEBUG ((DEBUG_ERROR, "Unkonw Endpoint Type = %d\n", UsbEndpointType (Desc)));
  }

  return 0;
}

STATIC
UINT32
CdnspGetMaxEsitPayload (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  INT32  MaxPacket;
  INT32  MaxBurst;

  if (UsbEndpointXferControl (Ep->Endpoint.Des) ||
      UsbEndpointXferBulk (Ep->Endpoint.Des))
  {
    return 0;
  }

  if (CdnspDevice->Speed >= UsbBusSpeedSuper) {
    return Ep->Endpoint.SsCompDes->WBytesPerInterval;
  }

  MaxPacket = UsbEndpointMaxPacket (Ep->Endpoint.Des);
  MaxBurst  = UsbEndpointMaxpMult (Ep->Endpoint.Des);

  return MaxPacket * MaxBurst;
}

int
fls (
  int  x
  )
{
  int  position;
  int  i;

  if (0 != x) {
    for (i = (x >> 1), position = 0; i != 0; ++position) {
      i >>= 1;
    }
  } else {
    position = -1;
  }

  return position + 1;
}

STATIC
UINT32
CdnspGetEndpointInterval (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  UINT32  Interval = 0;

  if (UsbEndpointXferIsoc (Ep->Endpoint.Des)) {
    DEBUG ((DEBUG_ERROR, "Not Support Isoc \n"));
    return 0;
  }

  switch (CdnspDevice->Speed) {
    case UsbBusSpeedHigh:
    case UsbBusSpeedSuper:
    case UsbBusSpeedSuperPlus:
      if (UsbEndpointXferInterrupt (Ep->Endpoint.Des)) {
        if ((Ep->Endpoint.Des->Interval > 16) || (Ep->Endpoint.Des->Interval < 1)) {
          DEBUG ((DEBUG_ERROR, "Invalid Interval(%d)\n", Ep->Endpoint.Des->Interval));
          Interval = 12;
        } else {
          Interval = Ep->Endpoint.Des->Interval -1;
          if (Interval > 12) {
            Interval = 12;
          }
        }
      }

      break;
    case UsbBusSpeedFull:
      if (UsbEndpointXferInterrupt (Ep->Endpoint.Des)) {
        //
        // ms - > 125 us << 3
        //
        if ((Ep->Endpoint.Des->Interval > 255) || (Ep->Endpoint.Des->Interval < 1)) {
          Interval = 10;
        } else {
          Interval = Ep->Endpoint.Des->Interval << 3;
          Interval = fls (Interval);
          if (Interval > 10) {
            Interval = 10;
          } else if (Interval < 3) {
            Interval = 3;
          } else {
            // do nothing
          }
        }
      }

      break;
    default:
      DEBUG ((DEBUG_ERROR, "UnSupport Speed(%d)\n", CdnspDevice->Speed));
  }

  return Interval;
}

STATIC
UINT32
CdnspGetEndpointMult (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  //
  // do not support isoc
  //
  return 0;
}

STATIC
UINT32
CdnspGetEndpointMaxBurst (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  if (CdnspDevice->Speed >= UsbBusSpeedSuper) {
    return Ep->Endpoint.SsCompDes->BMaxBurst;
  }

  if (CdnspDevice->Speed == UsbBusSpeedHigh) {
    if (UsbEndpointXferInterrupt (Ep->Endpoint.Des)) {
      return UsbEndpointMaxpMult (Ep->Endpoint.Des) -1;
    }
  }

  return 0;
}

EFI_STATUS
CdnspEndpointInit (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  CDNSP_RING_TYPE  RingType;
  CDNSP_EP_CTX     *EpCtx;
  UINT32           ErrorCount = 0;
  UINT32           AvgTrbLen;
  UINT32           MaxPacket;
  UINT32           MaxBurst;
  UINT32           Interval;
  UINT32           Mult;
  UINT32           EndpointType;
  UINT32           MaxEsitPayload;

  EpCtx = Ep->InCtx;

  EndpointType = CdnspGetEndpointType (Ep->Endpoint.Des);

  if (!EndpointType) {
    return EFI_INVALID_PARAMETER;
  }

  RingType = UsbEndpointType (Ep->Endpoint.Des);

  MaxEsitPayload = CdnspGetMaxEsitPayload (CdnspDevice, Ep);
  Interval       = CdnspGetEndpointInterval (CdnspDevice, Ep);
  Mult           = CdnspGetEndpointMult (CdnspDevice, Ep);
  MaxPacket      = UsbEndpointMaxPacket (Ep->Endpoint.Des);
  MaxBurst       = CdnspGetEndpointMaxBurst (CdnspDevice, Ep);
  AvgTrbLen      = MaxEsitPayload;

  if (!UsbEndpointXferIsoc (Ep->Endpoint.Des)) {
    ErrorCount = 3;
  }

  if (UsbEndpointXferBulk (Ep->Endpoint.Des) &&
      (CdnspDevice->Speed == UsbBusSpeedHigh))
  {
    MaxPacket = 512;
  }

  if (UsbEndpointXferControl (Ep->Endpoint.Des)) {
    AvgTrbLen = 8;
  }

  Ep->Ring = CdnspRingAlloc (CdnspDevice, 2, RingType, MaxPacket);
  if (NULL == Ep->Ring) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ep->Skip = FALSE;

  EpCtx->EpInfo = EP_MAX_ESIT_PAYLOAD_HI (MaxEsitPayload) | EP_INTERVAL (Interval) |
                  EP_MULT (Mult);
  EpCtx->EpInfo2 = EP_TYPE (EndpointType) | MAX_PACKET (MaxPacket) |
                   MAX_BURST (MaxBurst) | ERROR_COUNT (ErrorCount);
  EpCtx->Deq = ((UINT64)Ep->Ring->FirstSeg->Trbs) | Ep->Ring->CycleState;

  EpCtx->TxInfo = EP_MAX_ESIT_PAYLOAD_LO (MaxEsitPayload) |
                  EP_AVG_TRB_LENGTH (AvgTrbLen);

  if (UsbEndpointXferBulk (Ep->Endpoint.Des) &&
      (CdnspDevice->Speed > UsbBusSpeedHigh))
  {
    if (UsbSsMaxStreams (Ep->Endpoint.SsCompDes)) {
      DEBUG ((DEBUG_ERROR, "Not Support Stream\n"));
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
CdnspMemInit (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  EFI_STATUS  Status  = EFI_SUCCESS;
  UINT32      Value   = 0;
  UINT64      Value64 = 0;

  Value = CdnspRead (&CdnspDevice->OpRegs->Config);

  Value |= ((Value & ~MAX_DEVS) | CDNSP_DEV_MAX_SLOTS);
  CdnspWrite (Value, &CdnspDevice->OpRegs->Config);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->OpRegs->Config, Value));

  CdnspDevice->DevContextPtr = UncachedAllocateZeroPool ((CDNSP_DEV_MAX_SLOTS + 1) * sizeof (UINT64));
  if (!CdnspDevice->DevContextPtr) {
    return EFI_OUT_OF_RESOURCES;
  }

  CdnspWrite64 ((UINT64)CdnspDevice->DevContextPtr, &CdnspDevice->OpRegs->Dcbaap);
  DEBUG ((DEBUG_COMMON_LOG, "Device Context Base Address Array Pointer(%llx)\n", &CdnspDevice->OpRegs->Dcbaap));

  CdnspDevice->CommandRing = CdnspRingAlloc (CdnspDevice, 1, TYPE_COMMAND, 0);
  if (!CdnspDevice->CommandRing) {
    goto DestoryDeviceContext;
  }

  DEBUG ((DEBUG_COMMON_LOG, "Command Ring Control(%llx)\n", &CdnspDevice->OpRegs->CRCR));

  Value64 = CdnspRead64 (&CdnspDevice->OpRegs->CRCR);
  Value64 = (Value64 & (UINT64)CRCR_RSVD_BITS) |
            (((UINT64)CdnspDevice->CommandRing->FirstSeg->Trbs) & (UINT64) ~CRCR_RSVD_BITS) |
            CdnspDevice->CommandRing->CycleState;

  CdnspWrite64 (Value64, &CdnspDevice->OpRegs->CRCR);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%llx)\n", &CdnspDevice->OpRegs->CRCR, Value64));

  Value            = CdnspRead (&CdnspDevice->CapsRegs->DbOff);
  Value           &= DBOFF_MASK;
  CdnspDevice->Dba = (VOID *)CdnspDevice->CapsRegs + Value;

  DEBUG ((DEBUG_COMMON_LOG, "Doorbell(%x)\n", CdnspDevice->Dba));

  CdnspDevice->IntrRegs  = &CdnspDevice->RunRegs->IrSet[0];
  CdnspDevice->EventRing = CdnspRingAlloc (CdnspDevice, ERST_NUM_SEGS, TYPE_EVENT, 0);

  if (!CdnspDevice->EventRing) {
    goto FreeCmdRing;
  }

  Status = CdnspAllocErst (CdnspDevice, CdnspDevice->EventRing, &CdnspDevice->Erst);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CdnspAllocErst Fail\n"));
    goto FreeEventRing;
  }

  Value  = CdnspRead (&CdnspDevice->IntrRegs->ErstSz);
  Value &= ERST_SIZE_MASK;
  Value |= ERST_NUM_SEGS;
  CdnspWrite (Value, &CdnspDevice->IntrRegs->ErstSz);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%x)\n", &CdnspDevice->IntrRegs->ErstSz, Value));

  Value64  = CdnspRead64 (&CdnspDevice->IntrRegs->ErstBa);
  Value64 &= ERST_PTR_MASK;
  Value64 |= ((UINT64)CdnspDevice->Erst.Entries & (UINT64) ~ERST_PTR_MASK);
  CdnspWrite64 (Value64, &CdnspDevice->IntrRegs->ErstBa);

  DEBUG ((DEBUG_REGISTER_LOG, "Reg(0x%x)Value(0x%llx)\n", &CdnspDevice->IntrRegs->ErstBa, Value64));

  /* Set the event ring dequeue address. */
  CdnspSetEventDeq (CdnspDevice);

  Status = CdnspSetupPortArrays (CdnspDevice);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CdnspSetupPortArrays Fail\n"));
    goto FreeErst;
  }

  Status = CdnspAllocPrivDevice (CdnspDevice);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Alloc Priv Device Fail\n"));
    goto FreeErst;
  }

  return Status;

FreeErst:
  if (NULL != CdnspDevice->Erst.Entries) {
    UncachedSafeFreePool (CdnspDevice->Erst.Entries);
    CdnspDevice->Erst.Entries = NULL;
  }

FreeEventRing:
  CdnspRingFree (CdnspDevice, CdnspDevice->EventRing);
FreeCmdRing:
  CdnspRingFree (CdnspDevice, CdnspDevice->CommandRing);
DestoryDeviceContext:
  UncachedSafeFreePool (CdnspDevice->DevContextPtr);

  CdnspReset (CdnspDevice);
  return Status;
}

VOID
CdnspFreeEndpointRings (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  CdnspRingFree (CdnspDevice, Ep->Ring);
  Ep->Ring = NULL;
  //
  // do not support stramable
  //
}

VOID
CdnspEndpointZero (
  CDNSP_EP  *CdnspEp
  )
{
  CdnspEp->InCtx->EpInfo  = 0;
  CdnspEp->InCtx->EpInfo2 = 0;
  CdnspEp->InCtx->Deq     = 0;
  CdnspEp->InCtx->TxInfo  = 0;
}
