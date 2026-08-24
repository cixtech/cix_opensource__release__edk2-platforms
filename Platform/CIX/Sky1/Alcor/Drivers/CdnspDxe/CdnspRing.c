/** CdnspRing.c

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "Cdnsp.h"

STATIC
BOOLEAN
CdnspTrbIsLink (
  CDNSP_TRB  *Trb
  )
{
  return TRB_TYPE_LINK (Trb->Link.Control);
}

STATIC
BOOLEAN
CdnspTrbIsNoop (
  CDNSP_TRB  *Trb
  )
{
  return TRB_TYPE_NOOP (Trb->Generic.Filed[3]);
}

STATIC
VOID
CdnspNextTrb (
  CDNSP_RING     *CdnspRing,
  CDNSP_SEGMENT  **Seg,
  CDNSP_TRB      **Trb
  )
{
  if (CdnspTrbIsLink (*Trb)) {
    *Seg = (*Seg)->Next;
    *Trb = ((*Seg)->Trbs);
  } else {
    (*Trb)++;
  }
}

STATIC
VOID
CdnspTrbToNoop (
  CDNSP_TRB  *Trb,
  UINT32     NoopType
  )
{
  if (CdnspTrbIsLink (Trb)) {
    Trb->Link.Control &= (UINT32) ~TRB_CHAIN;
  } else {
    Trb->Generic.Filed[0]  = 0;
    Trb->Generic.Filed[1]  = 0;
    Trb->Generic.Filed[2]  = 0;
    Trb->Generic.Filed[3] &= (UINT32)TRB_CYCLE;
    Trb->Generic.Filed[3] |= (UINT32)TRB_TYPE (NoopType);
  }
}

STATIC
VOID
CdnspTdToNoop (
  CDNSP_RING  *EpRing,
  CDNSP_TD    *Td,
  BOOLEAN     FlipCycle
  )
{
  CDNSP_SEGMENT  *Seg = Td->StartSeg;
  CDNSP_TRB      *Trb = Td->FirstTrb;

  while (1) {
    CdnspTrbToNoop (Trb, TRB_TR_NOOP);

    if (FlipCycle && (Trb != Td->FirstTrb) && (Trb != Td->LastTrb)) {
      Trb->Generic.Filed[3] ^= (UINT32)TRB_CYCLE;
    }

    if (Trb == Td->LastTrb) {
      break;
    }

    CdnspNextTrb (EpRing, &Seg, &Trb);
  }
}

/*
 * Check to see if there's room to enqueue num_trbs on the ring and make sure
 * enqueue pointer will not advance into dequeue segment.
 */
STATIC
BOOLEAN
CdnspRoomOnRing (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring,
  UINT32        NumTrbs
  )
{
  INT32  NumTrbsInDeqSeg;

  if (Ring->NumTrbsFree < NumTrbs) {
    return FALSE;
  }

  if ((Ring->Type != TYPE_COMMAND) && (Ring->Type != TYPE_EVENT)) {
    NumTrbsInDeqSeg = Ring->Dequeue - Ring->DeqSeg->Trbs;

    if (Ring->NumTrbsFree < NumTrbs + NumTrbsInDeqSeg) {
      return FALSE;
    }
  }

  return TRUE;
}

EFI_STATUS
CdnspCmdStopEp (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  UINT32      EpState = GET_EP_CTX_STATE (Ep->OutCtx);
  EFI_STATUS  Status  = EFI_SUCCESS;

  if ((EpState == EP_STATE_STOPPED) || (EpState == EP_STATE_DISABLED)) {
    goto EpStopped;
  }

  CdnspQueueStopEndpoint (CdnspDevice, Ep->Index);
  CdnspRingCmdDb (CdnspDevice);
  Status = CdnspWaitForCmdCompl (CdnspDevice);

EpStopped:
  Ep->EpState |= EP_STOPPED;
  return Status;
}

VOID
CdnspQueueFlushEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  )
{
  CdnspQueueCommand (
    CdnspDevice,
    0,
    0,
    0,
    TRB_TYPE (TRB_FLUSH_ENDPOINT) |
    SLOT_ID_FOR_TRB (CdnspDevice->SlotId) |
    EP_ID_FOR_TRB (EpIndex)
    );
}

EFI_STATUS
CdnspCmdFlushEp (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *CdnspEp
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  CdnspQueueFlushEndpoint (CdnspDevice, CdnspEp->Index);
  CdnspRingCmdDb (CdnspDevice);
  Status = CdnspWaitForCmdCompl (CdnspDevice);
  return Status;
}

VOID
CdnspQueueConfigureEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT64        InCtxAddress
  )
{
  CdnspQueueCommand (
    CdnspDevice,
    LOWER_32_BITS (InCtxAddress),
    UPPER_32_BITS (InCtxAddress),
    0,
    TRB_TYPE (TRB_CONFIG_EP) | SLOT_ID_FOR_TRB (CdnspDevice->SlotId)
    );
}

VOID
CdnspQueueAddressDevice (
  CDNSP_DEVICE     *CdnspDevice,
  UINT64           InCtxAddress,
  CDNSP_SETUP_DEV  Setup
  )
{
  CdnspQueueCommand (
    CdnspDevice,
    LOWER_32_BITS (InCtxAddress),
    UPPER_32_BITS (InCtxAddress),
    0,
    TRB_TYPE (TRB_ADDR_DEV) |
    SLOT_ID_FOR_TRB (CdnspDevice->SlotId)|
    (Setup == SETUP_CONTEXT_ONLY ? TRB_BSR : 0)
    );
}

VOID
CdnspQueueResetDevice (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CdnspQueueCommand (CdnspDevice, 0, 0, 0, TRB_TYPE (TRB_RESET_DEV | SLOT_ID_FOR_TRB (CdnspDevice->SlotId)));
}

VOID
CdnspQueueStopEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  )
{
  CdnspQueueCommand (
    CdnspDevice,
    0,
    0,
    0,
    SLOT_ID_FOR_TRB (CdnspDevice->SlotId) |
    EP_ID_FOR_TRB (EpIndex) | TRB_TYPE (TRB_STOP_RING)
    );
}

VOID
CdnspQueueHaltEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  )
{
  CdnspQueueCommand (
    CdnspDevice,
    0,
    0,
    0,
    TRB_TYPE (TRB_HALT_ENDPOINT) |
    SLOT_ID_FOR_TRB (CdnspDevice->SlotId) | EP_ID_FOR_TRB (EpIndex)
    );
}

VOID
CdnspQueueResetEp (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  )
{
  CdnspQueueCommand (
    CdnspDevice,
    0,
    0,
    0,
    TRB_TYPE (TRB_RESET_EP) |
    SLOT_ID_FOR_TRB (CdnspDevice->SlotId) | EP_ID_FOR_TRB (EpIndex)
    );
}

STATIC
BOOLEAN
CdnspLinkTrbTogglesCycle (
  CDNSP_TRB  *Trb
  )
{
  return ((UINT32)Trb->Link.Control) & LINK_TOGGLE;
}

VOID
CdnspQueueNewDequeueState (
  CDNSP_DEVICE         *CdnspDevice,
  CDNSP_EP             *CdnspEp,
  CDNSP_DEQUEUE_STATE  *DeqState
  )
{
  UINT32  TrbSlotId   = CdnspDevice->SlotId;
  UINT32  Type        = TRB_TYPE (TRB_SET_DEQ);
  UINT32  TrbSct      = 0;
  UINT64  TempAddress = (UINT64)DeqState->NewDequeue;

  CdnspQueueCommand (
    CdnspDevice,
    LOWER_32_BITS (TempAddress) | TrbSct |
    DeqState->NewCycleState,
    UPPER_32_BITS (TempAddress),
    0,
    TrbSlotId |
    EP_ID_FOR_TRB (CdnspEp->Index) | Type
    );
}

STATIC
EFI_STATUS
CdnspCmdSetDeq (
  CDNSP_DEVICE         *CdnspDevice,
  CDNSP_EP             *CdnspEp,
  CDNSP_DEQUEUE_STATE  *DeqState
  )
{
  CDNSP_RING  *EpRing = CdnspEp->Ring;
  EFI_STATUS  Status  = EFI_SUCCESS;

  if (!DeqState->NewDeqSeg || !DeqState->NewDequeue) {
    CdnspRingDoorbellForActiveRing (CdnspDevice, CdnspEp);
    return Status;
  }

  CdnspQueueNewDequeueState (CdnspDevice, CdnspEp, DeqState);
  CdnspRingCmdDb (CdnspDevice);
  Status = CdnspWaitForCmdCompl (CdnspDevice);

  if (CdnspTrbIsLink (EpRing->Dequeue)) {
    EpRing->DeqSeg  = EpRing->DeqSeg->Next;
    EpRing->Dequeue = EpRing->DeqSeg->Trbs;
  }

  while (EpRing->Dequeue != DeqState->NewDequeue) {
    EpRing->NumTrbsFree++;
    EpRing->Dequeue++;

    if (CdnspTrbIsLink (EpRing->Dequeue)) {
      if (EpRing->Dequeue == DeqState->NewDequeue) {
        break;
      }

      EpRing->DeqSeg  = EpRing->DeqSeg->Next;
      EpRing->Dequeue = EpRing->DeqSeg->Trbs;
    }
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  CdnspRingDoorbellForActiveRing (CdnspDevice, CdnspEp);

  return Status;
}

STATIC
VOID
CdnspFindNewDequeueState (
  CDNSP_EP             *CdnspEp,
  CDNSP_TD             *CurTd,
  CDNSP_DEQUEUE_STATE  *State
  )
{
  BOOLEAN        TdLastTrbFound = FALSE;
  CDNSP_SEGMENT  *NewSeg;
  CDNSP_RING     *EpRing;
  CDNSP_TRB      *NewDeq;
  BOOLEAN        CycleFound = FALSE;
  UINT64         HwDeq;

  EpRing = CdnspEp->Ring;
  if (!EpRing) {
    return;
  }

  HwDeq                = (UINT64)CdnspEp->OutCtx->Deq;
  NewSeg               = EpRing->DeqSeg;
  NewDeq               = EpRing->Dequeue;
  State->NewCycleState = HwDeq & 0x1;

  do {
    if (!CycleFound && (((UINT64)NewDeq) == (HwDeq & ~0xf))) {
      CycleFound = TRUE;
      if (TdLastTrbFound) {
        break;
      }
    }

    if (NewDeq == CurTd->LastTrb) {
      TdLastTrbFound = TRUE;
    }

    if (CycleFound && CdnspTrbIsLink (NewDeq) &&
        CdnspLinkTrbTogglesCycle (NewDeq))
    {
      State->NewCycleState ^= 0x1;
    }

    CdnspNextTrb (EpRing, &NewSeg, &NewDeq);

    if (NewDeq == CdnspEp->Ring->Dequeue) {
      DEBUG ((DEBUG_ERROR, "Failed To Find Dequeue\n"));
      State->NewDeqSeg  = NULL;
      State->NewDequeue = NULL;
      return;
    }
  } while (!CycleFound || !TdLastTrbFound);

  State->NewDeqSeg  = NewSeg;
  State->NewDequeue = NewDeq;
}

STATIC
CDNSP_SEGMENT
*
CdnspTrbInTd (
  CDNSP_SEGMENT  *StartSeg,
  CDNSP_TRB      *StartTrb,
  CDNSP_TRB      *EndTrb,
  UINT64         TrbAddress
  )
{
  CDNSP_SEGMENT  *CurSeg      = StartSeg;
  UINT64         StartAddress = (UINT64)StartTrb;
  UINT64         SegEndAddress;
  UINT64         SegStartAddress;
  UINT64         EndAddress = (UINT64)EndTrb;

  do {
    SegStartAddress = (UINT64)CurSeg->Trbs;
    SegEndAddress   = (UINT64)CurSeg->Trbs + TRBS_PER_SEGMENT * sizeof (CDNSP_TRB);

    if ((EndAddress >= SegStartAddress) && (EndAddress < SegEndAddress)) {
      if (StartAddress < EndAddress) {
        if ((TrbAddress >= StartAddress) &&
            (TrbAddress <= EndAddress))
        {
          return CurSeg;
        }
      } else {
        if (((TrbAddress >= StartAddress) &&
             (TrbAddress <= SegEndAddress)) ||
            ((TrbAddress >= SegStartAddress) &&
             (TrbAddress <= EndAddress)))
        {
          return CurSeg;
        }
      }

      return NULL;
    }

    if ((TrbAddress >= StartAddress) && (TrbAddress <= SegEndAddress)) {
      return CurSeg;
    }

    CurSeg       = CurSeg->Next;
    StartAddress = (UINT64)CurSeg->Trbs;
  } while (CurSeg != StartSeg);

  return NULL;
}

EFI_STATUS
CdnspRemoveRequest (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_REQUEST  *CdnspRequest,
  CDNSP_EP       *CdnspEp
  )
{
  CDNSP_DEQUEUE_STATE  DeqState;
  CDNSP_TD             *CurTd = NULL;
  CDNSP_RING           *EpRing;
  UINT64               HwDeq;
  CDNSP_SEGMENT        *Seg;
  ENDPOINT_STATUS      EStatus = ENDPOINT_CONNRESET;
  EFI_STATUS           Status  = EFI_SUCCESS;

  gBS->SetMem (&DeqState, sizeof (DeqState), 0);

  CurTd  = &CdnspRequest->Td;
  EpRing = CdnspEp->Ring;

  HwDeq  = (UINT64)CdnspEp->OutCtx->Deq;
  HwDeq &= ~0xf;

  Seg = CdnspTrbInTd (CurTd->StartSeg, CurTd->FirstTrb, CurTd->LastTrb, HwDeq);

  if (Seg && (CdnspEp->EpState & EP_ENABLED)) {
    CdnspFindNewDequeueState (CdnspEp, CurTd, &DeqState);
  } else {
    CdnspTdToNoop (EpRing, CurTd, FALSE);
  }

  RemoveEntryList (&CurTd->TdList);
  EpRing->NumTds--;

  if (CdnspDevice->CdnspState & CDNSP_STATE_DISCONNECT_PENDING) {
    EStatus = ENDPOINT_SHUTDONE;
    Status  = CdnspCmdSetDeq (CdnspDevice, CdnspEp, &DeqState);
  }

  if (!CdnspEp->Direction) {
    if (NULL != Seg) {
      gBS->CopyMem ((((UINT8 *)CdnspRequest->Request.Buf) + Seg->BounceOffset), Seg->BouncdBuf, Seg->BounceLength);
      Seg->BounceLength = 0;
      Seg->BounceOffset = 0;
    }
  }

  CdnspGiveback (CdnspEp, CurTd->Preq, EStatus);
  return Status;
}

VOID
CdnspRingDoorbellForActiveRing (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  )
{
  if (Ep->EpState & EP_DIS_IN_RROGRESS) {
    return;
  }

  if (!(Ep->EpState & EP_HAS_STREAMS) && Ep->Number) {
    if (Ep->Ring && !IsListEmpty (&Ep->Ring->TdList)) {
      CdnspRingEpDoorbell (CdnspDevice, Ep, 0);
    }
  } else {
    DEBUG ((DEBUG_ERROR, "Not Support Stream ep(%d), state(%d)\n", Ep->Number, Ep->EpState));
  }

  return;
}

STATIC
EFI_STATUS
CdnspPrepareRing (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *EpRing,
  UINT32        EpState,
  UINT32        NumTrbs
  )
{
  UINT32      NumTrbsNeed;
  EFI_STATUS  Status = EFI_SUCCESS;

  switch (EpState) {
    case EP_STATE_STOPPED:
    case EP_STATE_RUNNING:
    case EP_STATE_HALTED:
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Incorrect endpoint state\n"));
      return EFI_INVALID_PARAMETER;
  }

  while (1) {
    if (CdnspRoomOnRing (CdnspDevice, EpRing, NumTrbs)) {
      break;
    }

    //
    // TODO check the expansion operation does want to just double the size
    // when the NumTrbs < EpRing->NumTrbsFree but will advance in the dequeue segment
    //
    if (NumTrbs > EpRing->NumTrbsFree) {
      NumTrbsNeed = NumTrbs - EpRing->NumTrbsFree;
    } else {
      //
      // when the enqueue pointer will advance in the dequeue segment
      // alloc one more segment for the transfer ring
      //
      NumTrbsNeed = TRBS_PER_SEGMENT - 1;
    }

    Status = CdnspRingExpansion (CdnspDevice, EpRing, NumTrbsNeed);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Ring expand fail\n"));
      return EFI_OUT_OF_RESOURCES;
    }
  }

  while (CdnspTrbIsLink (EpRing->Enqueue)) {
    EpRing->Enqueue->Link.Control |= TRB_CHAIN;
    ArmDataSynchronizationBarrier ();
    EpRing->Enqueue->Link.Control ^= TRB_CYCLE;

    if (CdnspLinkTrbTogglesCycle (EpRing->Enqueue)) {
      EpRing->CycleState ^= 1;
    }

    EpRing->EnqSeg  = EpRing->EnqSeg->Next;
    EpRing->Enqueue = EpRing->EnqSeg->Trbs;
  }

  return Status;
}

EFI_STATUS
CdnspPrepareTransfer (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest,
  UINT32         NumTrbs
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  Status = CdnspPrepareRing (
             CdnspDevice,
             CdnspEp->Ring,
             GET_EP_CTX_STATE (CdnspEp->OutCtx),
             NumTrbs
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  InitializeListHead (&CdnspRequest->Td.TdList);
  CdnspRequest->Td.Preq = CdnspRequest;

  InsertTailList (&CdnspEp->Ring->TdList, &CdnspRequest->Td.TdList);
  CdnspEp->Ring->NumTds++;

  CdnspRequest->Td.StartSeg = CdnspEp->Ring->EnqSeg;
  CdnspRequest->Td.FirstTrb = CdnspEp->Ring->Enqueue;

  return Status;
}

STATIC
VOID
CdnspIncEnq (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring,
  BOOLEAN       MoreTrbsComing
  )
{
  CDNSP_TRB  *Next;
  UINT32     Chain;

  Chain = Ring->Enqueue->Generic.Filed[3] & TRB_CHAIN;

  if (!CdnspTrbIsLink (Ring->Enqueue)) {
    Ring->NumTrbsFree--;
  }

  ++(Ring->Enqueue);
  Next = Ring->Enqueue;

  while (CdnspTrbIsLink (Next)) {
    if (!Chain && !MoreTrbsComing) {
      break;
    }

    Next->Link.Control &= (UINT32)(~TRB_CHAIN);
    Next->Link.Control |= Chain;
    ArmDataSynchronizationBarrier ();
    Next->Link.Control ^= (UINT32)TRB_CYCLE;

    if (CdnspLinkTrbTogglesCycle (Next)) {
      Ring->CycleState ^= 1;
    }

    Ring->EnqSeg  = Ring->EnqSeg->Next;
    Ring->Enqueue = Ring->EnqSeg->Trbs;
    Next          = Ring->Enqueue;
  }
}

STATIC
VOID
CdnspQueueTrb (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring,
  BOOLEAN       MoreTrbsComing,
  UINT32        Field1,
  UINT32        Field2,
  UINT32        Field3,
  UINT32        Field4
  )
{
  CDNSP_GENERIC_TRB  *Trb;

  Trb = &Ring->Enqueue->Generic;

  Trb->Filed[0] = Field1;
  Trb->Filed[1] = Field2;
  Trb->Filed[2] = Field3;
  Trb->Filed[3] = Field4;

  DEBUG ((DEBUG_TRB_LOG, "Enqueue(0x%llx):(0x%x)(0x%x)(0x%x)(0x%x)\n", &Ring->Enqueue->Generic, Trb->Filed[0], Trb->Filed[1], Trb->Filed[2], Trb->Filed[3]));

  CdnspIncEnq (CdnspDevice, Ring, MoreTrbsComing);
}

BOOLEAN
CdnspRingEpDoorbell (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep,
  UINT32        StremId
  )
{
  UINT32  *RegAddr = &CdnspDevice->Dba->EpDb;
  UINT32  EpState  = Ep->EpState;
  UINT32  DbValue;

  if (EpState & EP_HALTED || !(EpState & EP_ENABLED)) {
    DEBUG ((DEBUG_ERROR, "EpState Invalid\n"));
    return FALSE;
  }

  if (EpState & EP_HAS_STREAMS) {
    DEBUG ((DEBUG_ERROR, "Not Support Stream Ep(%d), State(%d)\n", Ep->Number, Ep->EpState));
    return FALSE;
  }

  Ep->EpState &= ~EP_STOPPED;
  // gBS->Stall (10);
  ArmDataSynchronizationBarrier ();

  if ((Ep->Index == 0) && (CdnspDevice->Ep0Stage == CDNSP_DATA_STAGE) &&
      !CdnspDevice->Ep0ExpectIn)
  {
    DbValue = DB_VALUE_EP0_OUT (Ep->Index, 0);
  } else {
    DbValue = DB_VALUE (Ep->Index, 0);
  }

  CdnspWrite (DbValue, RegAddr);

  DEBUG ((DEBUG_TRB_LOG, "Ring Doorbell(0x%llx):(0x%x)\n", RegAddr, DbValue));

  if (CdnspDevice->ActivePort == &CdnspDevice->Usb2Port) {
    CdnspSetLinkState (CdnspDevice, &CdnspDevice->Usb2Port.Regs->PortSc, XDEV_U0);
  }

  return TRUE;
}

STATIC
UINT32
CdnspCountTrbs (
  UINT64  Addr,
  UINT64  Length
  )
{
  UINT32  NumTrbs;

  //
  // should not cross 64 kb so the length add more length for the second trb align to 64kB
  //
  NumTrbs = DIV_ROUND_UP (Length + (Addr & (TRB_MAX_BUFF_SIZE - 1)), TRB_MAX_BUFF_SIZE);
  if (NumTrbs == 0) {
    NumTrbs++;
  }

  return NumTrbs;
}

STATIC
UINT32
CountTrbsNeeded (
  CDNSP_REQUEST  *CdnspRequest
  )
{
  return CdnspCountTrbs ((UINT64)CdnspRequest->Request.Buf, CdnspRequest->Request.Length);
}

STATIC
BOOLEAN
CdnspAlignTd (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_REQUEST  *CdnspRequest,
  UINT32         EnqueueLen,
  UINT32         *TrbBufferLen,
  CDNSP_SEGMENT  *Seg
  )
{
  UINT32  Unalign;
  UINT32  MaxPkt;
  UINT32  NewBuffLen;

  MaxPkt  = UsbEndpointMaxPacket (CdnspRequest->Ep->Endpoint.Des);
  Unalign = (EnqueueLen + *TrbBufferLen) % MaxPkt;

  if (Unalign == 0) {
    return FALSE;
  }

  if (*TrbBufferLen > Unalign) {
    *TrbBufferLen -= Unalign;
    return FALSE;
  }

  NewBuffLen = MaxPkt - (EnqueueLen % MaxPkt);

  if (NewBuffLen > (CdnspRequest->Request.Length - EnqueueLen)) {
    NewBuffLen = (CdnspRequest->Request.Length - EnqueueLen);
  }

  //
  // TODO test this
  //
  if (CdnspRequest->Direction) {
    gBS->CopyMem (Seg->BouncdBuf, (((UINT8 *)CdnspRequest->Request.Buf) + EnqueueLen), NewBuffLen);
  }

  DEBUG ((DEBUG_COMMON_LOG, "Using The Bounce Buff\n"));

  *TrbBufferLen     = NewBuffLen;
  Seg->BounceLength = NewBuffLen;
  Seg->BounceOffset = EnqueueLen;

  return TRUE;
}

UINT32
CdnspTdRemainder (
  CDNSP_EP  *CdnspEp,
  INT32     Transferred,
  INT32     TrbBufferLen,
  UINT32    TdTotalLen,
  BOOLEAN   MoreTrbsComing,
  BOOLEAN   Zlp
  )
{
  UINT32  MaxPacket, TotalPackektCount;

  if (Zlp) {
    return 1;
  }

  if (!MoreTrbsComing || ((Transferred == 0) && (TrbBufferLen == 0)) ||
      (TrbBufferLen == TdTotalLen))
  {
    return 0;
  }

  MaxPacket         = UsbEndpointMaxPacket (CdnspEp->Endpoint.Des);
  TotalPackektCount = DIV_ROUND_UP (TdTotalLen, MaxPacket);

  return (TotalPackektCount - ((Transferred + TrbBufferLen) / MaxPacket));
}

STATIC
EFI_STATUS
CdnspGivebackFirstTrb (
  CDNSP_DEVICE       *CdnspDevice,
  CDNSP_EP           *CdnspEp,
  INT32              StartCycle,
  CDNSP_GENERIC_TRB  *StartTrb
  )
{
  ArmDataSynchronizationBarrier ();

  if (StartCycle) {
    StartTrb->Filed[3] |= StartCycle;
  } else {
    StartTrb->Filed[3] &= ~TRB_CYCLE;
  }

  return CdnspRingEpDoorbell (CdnspDevice, CdnspEp, 0);
}

EFI_STATUS
CdnspQueueBulkTx (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_REQUEST  *CdnspRequest
  )
{
  CDNSP_RING         *EpRing = CdnspRequest->Ep->Ring;
  UINT32             FullLength, TrbBufferLen, EnqueueLen, BlockLen;
  UINT32             StartCycle, NumTrbs;
  UINT64             Addresss;
  UINT64             SendAddress;
  CDNSP_EP           *CdnspEp       = (CDNSP_EP *)CdnspRequest->Ep;
  BOOLEAN            MoreTrbsComing = TRUE;
  BOOLEAN            ZeroLenTrb     = FALSE;
  BOOLEAN            NeedZeroPacket = FALSE;
  BOOLEAN            FirstTrb       = TRUE;
  EFI_STATUS         Status         = EFI_SUCCESS;
  CDNSP_GENERIC_TRB  *StartTrb;
  UINT32             Field, LengthField, Remainder;

  if (!EpRing) {
    DEBUG ((DEBUG_ERROR, "EpRing is NULL\n"));
    return EFI_NOT_READY;
  }

  FullLength = CdnspRequest->Request.Length;
  NumTrbs    = CountTrbsNeeded (CdnspRequest);
  BlockLen   = FullLength;
  Addresss   = (UINT64)CdnspRequest->Request.Buf;

  if (CdnspRequest->Request.Zero && CdnspRequest->Request.Length &&
      IS_ALIGNED (FullLength, UsbEndpointMaxPacket (CdnspEp->Endpoint.Des)))
  {
    NeedZeroPacket = TRUE;
    NumTrbs++;
  }

  Status = CdnspPrepareTransfer (CdnspDevice, CdnspEp, CdnspRequest, NumTrbs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  StartTrb    = &EpRing->Enqueue->Generic;
  StartCycle  = EpRing->CycleState;
  SendAddress = Addresss;

  for (EnqueueLen = 0; FirstTrb || EnqueueLen < FullLength || ZeroLenTrb; EnqueueLen += TrbBufferLen) {
    Field = TRB_TYPE (TRB_NORMAL);

    TrbBufferLen = TRB_BUFF_LEN_UP_TO_BOUNDARY (Addresss);
    TrbBufferLen = MIN (TrbBufferLen, BlockLen);
    if (EnqueueLen + TrbBufferLen > FullLength) {
      TrbBufferLen = FullLength - EnqueueLen;
    }

    if (FirstTrb) {
      FirstTrb = FALSE;
      if (StartCycle == 0) {
        Field |= TRB_CYCLE;
      }
    } else {
      Field |= EpRing->CycleState;
    }

    if ((EnqueueLen + TrbBufferLen < FullLength) || NeedZeroPacket) {
      Field |= TRB_CHAIN;
      if (CdnspTrbIsLink (EpRing->Enqueue + 1)) {
        if (CdnspAlignTd (CdnspDevice, CdnspRequest, EnqueueLen, &TrbBufferLen, EpRing->EnqSeg)) {
          SendAddress = (UINT64)EpRing->EnqSeg->BouncdBuf;
          // assuming TD won't span 2 segs
          CdnspRequest->Td.BounceSeg = EpRing->EnqSeg;
        }
      }
    }

    if (EnqueueLen + TrbBufferLen >= FullLength) {
      if (NeedZeroPacket && !ZeroLenTrb) {
        ZeroLenTrb = TRUE;
      } else {
        ZeroLenTrb               = FALSE;
        Field                   &= ~TRB_CHAIN;
        Field                   |= TRB_IOC;
        MoreTrbsComing           = FALSE;
        NeedZeroPacket           = FALSE;
        CdnspRequest->Td.LastTrb = EpRing->Enqueue;
      }
    }

    if (!CdnspRequest->Direction) {
      Field |= TRB_ISP;
    }

    Remainder = CdnspTdRemainder (
                  CdnspEp,
                  EnqueueLen,
                  TrbBufferLen,
                  FullLength,
                  MoreTrbsComing,
                  ZeroLenTrb
                  );

    LengthField = TRB_LEN (TrbBufferLen) | TRB_TD_SIZE (Remainder) | TRB_INTR_TARGET (0);

    CdnspQueueTrb (
      CdnspDevice,
      EpRing,
      MoreTrbsComing,
      LOWER_32_BITS (SendAddress),
      UPPER_32_BITS (SendAddress),
      LengthField,
      Field
      );

    Addresss   += TrbBufferLen;
    BlockLen   -= TrbBufferLen;
    SendAddress = Addresss;
  }

  if (EnqueueLen != CdnspRequest->Request.Length) {
    DEBUG ((
      DEBUG_ERROR,
      "Miscalculated Enqueue length(%d), length(%d)\n",
      EnqueueLen,
      CdnspRequest->Request.Length
      ));
  }

  Status = CdnspGivebackFirstTrb (CdnspDevice, CdnspEp, StartCycle, StartTrb);

  return Status;
}

EFI_STATUS
CdnspQueueCtrlTx (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest
  )
{
  UINT32      Field, LengthFiled, MaxPacket;
  CDNSP_RING  *EpRing;
  UINT32      NumTrbs;
  EFI_STATUS  Status;
  UINT32      ZeroPacket = 0;

  EpRing = CdnspEp->Ring;

  if (!EpRing) {
    return EFI_INVALID_PARAMETER;
  }

  NumTrbs   = (CdnspDevice->ThreeStageSetup) ? 2 : 1;
  MaxPacket = UsbEndpointMaxPacket (CdnspEp->Endpoint.Des);

  if (CdnspRequest->Request.Zero && CdnspRequest->Request.Length &&
      (CdnspRequest->Request.Length % MaxPacket == 0))
  {
    NumTrbs++;
    ZeroPacket = 1;
  }

  Status = CdnspPrepareTransfer (CdnspDevice, CdnspEp, CdnspRequest, NumTrbs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Prepare Transfer Failed(%d)\n", Status));
    return Status;
  }

  if (CdnspRequest->Request.Length > 0) {
    Field = TRB_TYPE (TRB_DATA);

    if (ZeroPacket) {
      Field |= TRB_CHAIN;
    } else {
      Field |= TRB_IOC | (CdnspDevice->Ep0ExpectIn ? 0 : TRB_ISP);
    }

    if (CdnspDevice->Ep0ExpectIn) {
      Field |= TRB_DIR_IN;
    }

    LengthFiled = TRB_LEN (CdnspRequest->Request.Length) |
                  TRB_TD_SIZE (ZeroPacket) | TRB_INTR_TARGET (0);

    CdnspQueueTrb (
      CdnspDevice,
      EpRing,
      TRUE,
      LOWER_32_BITS ((UINT64)CdnspRequest->Request.Buf),
      UPPER_32_BITS ((UINT64)CdnspRequest->Request.Buf),
      LengthFiled,
      Field | EpRing->CycleState | TRB_SETUPID (CdnspDevice->SetupId) | CdnspDevice->SetupSpeed
      );

    if (ZeroPacket) {
      Field = TRB_TYPE (TRB_NORMAL) | TRB_IOC;

      if (!CdnspDevice->Ep0ExpectIn) {
        Field |= TRB_ISP;
      }

      CdnspQueueTrb (
        CdnspDevice,
        EpRing,
        TRUE,
        LOWER_32_BITS ((UINT64)CdnspRequest->Request.Buf),
        UPPER_32_BITS ((UINT64)CdnspRequest->Request.Buf),
        0,
        Field | EpRing->CycleState | TRB_SETUPID (CdnspDevice->SetupId) | CdnspDevice->SetupSpeed
        );
    }

    CdnspDevice->Ep0Stage = CDNSP_DATA_STAGE;
  }

  CdnspRequest->Td.LastTrb = EpRing->Enqueue;

  if (CdnspRequest->Request.Length == 0) {
    Field = EpRing->CycleState;
  } else {
    Field = (EpRing->CycleState ^ 1);
  }

  if ((CdnspRequest->Request.Length > 0) && CdnspDevice->Ep0ExpectIn) {
    Field |= TRB_DIR_IN;
  }

  if (CdnspEp->EpState & EP0_HALTED_STATUS) {
    CdnspEp->EpState &= ~EP0_HALTED_STATUS;
    Field            |= TRB_SETUPSTAT (TRB_SETUPSTAT_STALL);
  } else {
    Field |= TRB_SETUPSTAT (TRB_SETUPSTAT_ACK);
  }

  CdnspQueueTrb (
    CdnspDevice,
    EpRing,
    FALSE,
    0,
    0,
    TRB_INTR_TARGET (0),
    Field | TRB_IOC | TRB_SETUPID (CdnspDevice->SetupId) |
    TRB_TYPE (TRB_STATUS) | CdnspDevice->SetupSpeed
    );

  CdnspRingEpDoorbell (CdnspDevice, CdnspEp, 0);

  return Status;
}

BOOLEAN
CdnspLastTrbOnSeg (
  CDNSP_SEGMENT  *Seg,
  CDNSP_TRB      *Trb
  )
{
  return Trb == &Seg->Trbs[TRBS_PER_SEGMENT -1];
}

BOOLEAN
CdnspLastTrbOnRing (
  CDNSP_RING     *Ring,
  CDNSP_SEGMENT  *Seg,
  CDNSP_TRB      *Trb
  )
{
  return CdnspLastTrbOnSeg (Seg, Trb) && (Seg->Next == Ring->FirstSeg);
}

VOID
CdnspIncDeq (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring
  )
{
  if (Ring->Type == TYPE_EVENT) {
    if (!CdnspLastTrbOnSeg (Ring->DeqSeg, Ring->Dequeue)) {
      Ring->Dequeue++;
      return;
    }

    if (CdnspLastTrbOnRing (Ring, Ring->DeqSeg, Ring->Dequeue)) {
      Ring->CycleState ^= 1;
    }

    Ring->DeqSeg  = Ring->DeqSeg->Next;
    Ring->Dequeue = Ring->DeqSeg->Trbs;
    return;
  }

  if (!CdnspTrbIsLink (Ring->Dequeue)) {
    Ring->Dequeue++;
    Ring->NumTrbsFree++;
  }

  while (CdnspTrbIsLink (Ring->Dequeue)) {
    Ring->DeqSeg  = Ring->DeqSeg->Next;
    Ring->Dequeue = Ring->DeqSeg->Trbs;
  }
}

VOID
CdnspQueueCommand (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        Field1,
  UINT32        Field2,
  UINT32        Field3,
  UINT32        Field4
  )
{
  CdnspPrepareRing (CdnspDevice, CdnspDevice->CommandRing, EP_STATE_RUNNING, 1);

  CdnspDevice->Cmd.CommandTrb = CdnspDevice->CommandRing->Enqueue;

  CdnspQueueTrb (
    CdnspDevice,
    CdnspDevice->CommandRing,
    FALSE,
    Field1,
    Field2,
    Field3,
    Field4 | CdnspDevice->CommandRing->CycleState
    );
}

VOID
CdnspRingCmdDb (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  // 0 for command doorbell
  CdnspWrite (DB_VALUE_CMD, &CdnspDevice->Dba->CmdDb);

  DEBUG ((DEBUG_TRB_LOG, "Ring Doorbell(0x%llx):(0x%x)\n", &CdnspDevice->Dba->CmdDb, DB_VALUE_CMD));
}

VOID
CdnspQueueSlotControl (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        TrbType
  )
{
  CdnspQueueCommand (CdnspDevice, 0, 0, 0, TRB_TYPE (TrbType)| SLOT_ID_FOR_TRB (CdnspDevice->SlotId));
}

STATIC
EFI_STATUS
CdnspUpdatePortId (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        PortId
  )
{
  CDNSP_PORT  *Port   = CdnspDevice->ActivePort;
  UINT8       OldPort = 0;

  if (Port && (Port->PortNum == PortId)) {
    return EFI_SUCCESS;
  }

  if (Port) {
    OldPort = Port->PortNum;
  }

  if (PortId == CdnspDevice->Usb2Port.PortNum) {
    Port = &CdnspDevice->Usb2Port;
  } else if (PortId == CdnspDevice->Usb3Port.PortNum) {
    Port = &CdnspDevice->Usb3Port;
  } else {
    DEBUG ((DEBUG_ERROR, "Invalid Port ID = %d \n", PortId));
    return EFI_INVALID_PARAMETER;
  }

  if (PortId != OldPort) {
    CdnspDisableSlot (CdnspDevice);
    CdnspDevice->ActivePort = Port;
    CdnspEnableSlot (CdnspDevice);
  }

  return EFI_SUCCESS;
}

STATIC
VOID
CdnspHandlePortStatus (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_TRB     *Event
  )
{
  CDNSP_PORT_REGS  *PortRegs;
  UINT32           PortId;
  BOOLEAN          Port2 = FALSE;
  UINT32           PortSc;
  UINT32           LinkState;

  if (GET_COMP_CODE (Event->Generic.Filed[2]) != COMP_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Incorrect Status(0x%x)\n", Event->Generic.Filed[2]));
  }

  PortId = GET_PORT_ID (Event->Generic.Filed[0]);

  if (EFI_ERROR (CdnspUpdatePortId (CdnspDevice, PortId))) {
    goto CleanUp;
  }

  PortRegs = CdnspDevice->ActivePort->Regs;

  if (PortId == CdnspDevice->Usb2Port.PortNum) {
    Port2 = TRUE;
  }

NewEvent:
  PortSc = CdnspRead (&PortRegs->PortSc);
  CdnspWrite (CdnspPortStateToNeutral (PortSc) | (PortSc & PORT_CHANGE_BITS), &PortRegs->PortSc);

  CdnspDevice->Speed = CdnspPortSpeed (PortSc);
  LinkState          = PortSc & PORT_PLS_MASK;

  DEBUG ((DEBUG_COMMON_LOG, "PortSc(0x%x) Port2 = %d\n", PortSc, Port2));

  if (PortSc & PORT_PLC) {
    DEBUG ((DEBUG_COMMON_LOG, "Link State= 0x%x\n", LinkState));
    CdnspDevice->LinkState = LinkState;
  }

  if (PortSc & PORT_CSC) {
    if (CdnspDevice->Connected && !(PortSc & PORT_CONNECT)) {
      CdnspDeviceDisconnect (CdnspDevice);
      CdnspDevice->Connected = 0;
    }

    if ((PortSc & PORT_CONNECT)) {
      SetDeviceState (USB_STATE_ATTACHED);
      if (!Port2) {
        CdnspIrqReset (CdnspDevice);
        CdnspDevice->Connected = 1;
      }
    }
  }

  if ((PortSc & (PORT_RC | PORT_WRC)) && (PortSc & PORT_CONNECT)) {
    CdnspIrqReset (CdnspDevice);
    CdnspDevice->Connected = 1;
    CdnspDevice->MayWakeup = 0;
  }

  if (PortSc & PORT_OCA) {
    DEBUG ((DEBUG_ERROR, "Port Over Current\n"));
  }

  if (PortSc & PORT_CEC) {
    DEBUG ((DEBUG_ERROR, "Port Configure Error\n"));
  }

  if (CdnspRead (&PortRegs->PortSc) & PORT_CHANGE_BITS) {
    goto NewEvent;
  }

CleanUp:
  CdnspIncDeq (CdnspDevice, CdnspDevice->EventRing);
}

STATIC
VOID
CdnspTdCleanup (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_TD      *Td,
  CDNSP_RING    *EpRing,
  INT32         *Status
  )
{
  CDNSP_REQUEST  *CdnspRequest = Td->Preq;
  CDNSP_SEGMENT  *Seg          = Td->BounceSeg;

  if (!CdnspRequest->Direction) {
    if (NULL != Seg) {
      gBS->CopyMem ((((UINT8 *)CdnspRequest->Request.Buf) + Seg->BounceOffset), Seg->BouncdBuf, Seg->BounceLength);
      Seg->BounceLength = 0;
      Seg->BounceOffset = 0;
    }
  }

  if (CdnspRequest->Request.Actual > CdnspRequest->Request.Length) {
    CdnspRequest->Request.Actual = 0;
    *Status                      = ENDPOINT_COMPLETE;
  }

  RemoveEntryList (&Td->TdList);
  EpRing->NumTds--;

  CdnspGiveback (CdnspRequest->Ep, CdnspRequest, *Status);
}

STATIC
VOID
CdnspFinishTd (
  CDNSP_DEVICE          *CdnspDevice,
  CDNSP_TD              *Td,
  CDNSP_TRANSFER_EVENT  *Event,
  CDNSP_EP              *Ep,
  INT32                 *Status
  )
{
  CDNSP_RING  *EpRing;
  UINT32      TrbCompCode;

  EpRing      = Ep->Ring;
  TrbCompCode = GET_COMP_CODE (Event->TransferLength);

  if ((TrbCompCode == COMP_STOPPED_LENGTH_INVALID) ||
      (TrbCompCode == COMP_STOPPED) ||
      (TrbCompCode == COMP_STOPPED_SHORT_PACKET))
  {
    return;
  }

  while (EpRing->Dequeue != Td->LastTrb) {
    CdnspIncDeq (CdnspDevice, EpRing);
  }

  CdnspIncDeq (CdnspDevice, EpRing);

  CdnspTdCleanup (CdnspDevice, Td, EpRing, Status);
}

STATIC
VOID
CdnspProcessCtrlTd (
  CDNSP_DEVICE          *CdnspDevice,
  CDNSP_TD              *Td,
  CDNSP_TRB             *EventTrb,
  CDNSP_TRANSFER_EVENT  *Event,
  CDNSP_EP              *CdnspEp,
  INT32                 *Status
  )
{
  CDNSP_RING  *EpRing;
  UINT32      Remaining;
  UINT32      TrbType;

  TrbType   = TRB_FIELD_TO_TYPE (EventTrb->Generic.Filed[3]);
  EpRing    = CdnspEp->Ring;
  Remaining = EVENT_TRB_LEN (Event->TransferLength);

  if (TrbType == TRB_DATA) {
    Td->RequestLengthSet     = TRUE;
    Td->Preq->Request.Actual = Td->Preq->Request.Length - Remaining;
  }

  if (!Td->RequestLengthSet) {
    Td->Preq->Request.Actual = Td->Preq->Request.Length;
  }

  if ((CdnspDevice->Ep0Stage == CDNSP_DATA_STAGE) && (CdnspEp->Number == 0) &&
      CdnspDevice->ThreeStageSetup)
  {
    CdnspDevice->Ep0Stage = CDNSP_STATUS_STAGE;
    // ring the doorbell to handle stage trb
    CdnspGivebackFirstTrb (
      CdnspDevice,
      CdnspEp,
      EpRing->CycleState,
      &Td->LastTrb->Generic
      );

    return;
  }

  *Status = ENDPOINT_COMPLETE;

  CdnspFinishTd (CdnspDevice, Td, Event, CdnspEp, Status);
}

STATIC
UINT32
CdnspSumTrbLengths (
  CDNSP_RING  *CdnspRing,
  CDNSP_TRB   *StopTrb
  )
{
  CDNSP_SEGMENT  *Seg = CdnspRing->DeqSeg;
  CDNSP_TRB      *Trb = CdnspRing->Dequeue;
  UINT32         Sum;

  for (Sum = 0; Trb != StopTrb; CdnspNextTrb (CdnspRing, &Seg, &Trb)) {
    if (!CdnspTrbIsNoop (Trb) && !CdnspTrbIsLink (Trb)) {
      Sum += TRB_LEN (Trb->Generic.Filed[2]);
    }
  }

  return Sum;
}

STATIC
VOID
CdnspProcessBulkIntrTd (
  CDNSP_DEVICE          *CdnspDevice,
  CDNSP_TD              *Td,
  CDNSP_TRB             *EpTrb,
  CDNSP_TRANSFER_EVENT  *Event,
  CDNSP_EP              *CdnspEp,
  INT32                 *Status
  )
{
  UINT32      Remaining, Requested, EpTrbLen;
  CDNSP_RING  *EpRing;
  UINT32      TrbCompCode;

  EpRing      = CdnspEp->Ring;
  TrbCompCode = GET_COMP_CODE (Event->TransferLength);
  Remaining   = EVENT_TRB_LEN (Event->TransferLength);
  EpTrbLen    = TRB_LEN (EpTrb->Generic.Filed[2]);
  Requested   = Td->Preq->Request.Length;

  switch (TrbCompCode) {
    case COMP_SUCCESS:
    case COMP_SHORT_PACKET:
      *Status = ENDPOINT_COMPLETE;
      break;
    case COMP_STOPPED_SHORT_PACKET:
      Td->Preq->Request.Actual = Remaining;
      goto FinishTd;
    case COMP_STOPPED_LENGTH_INVALID:
      EpTrbLen  = 0;
      Remaining = 0;
      break;
  }

  if (EpTrb == Td->LastTrb) {
    EpTrbLen = Requested - Remaining;
  } else {
    EpTrbLen = CdnspSumTrbLengths (EpRing, EpTrb) + EpTrbLen - Remaining;
  }

  Td->Preq->Request.Actual = EpTrbLen;

FinishTd:
  CdnspFinishTd (CdnspDevice, Td, Event, CdnspEp, Status);
}

STATIC
EFI_STATUS
CdnspHandleTxEvent (
  CDNSP_DEVICE          *CdnspDevice,
  CDNSP_TRANSFER_EVENT  *Event
  )
{
  INT32                    Invalidate;
  UINT32                   EpIndex;
  UINT32                   TrbCompCode;
  CDNSP_EP                 *CdnspEp;
  CDNSP_RING               *CdnspRing;
  CDNSP_TRB                *EpTrb;
  EFI_STATUS               Status;
  CDNSP_TD                 *Td;
  CDNSP_SEGMENT            *EpSegment;
  USB_ENDPOINT_DESCRIPTOR  *Desc;
  INT32                    EpStatus           = ENDPOINT_INPROCESS;
  BOOLEAN                  HandlingSkippedTds = FALSE;

  //
  // TODO check this bit
  //
  Invalidate  = Event->Flags & TRB_EVENT_INVALIDATE;
  EpIndex     = TRB_TO_EP_ID (Event->Flags) - 1;
  TrbCompCode = GET_COMP_CODE (Event->TransferLength);
  CdnspEp     = &CdnspDevice->Eps[EpIndex];
  CdnspRing   = CdnspEp->Ring;

  if (Invalidate || !CdnspDevice->Connected) {
    goto CleanUp;
  }

  if (GET_EP_CTX_STATE (CdnspEp->OutCtx) == EP_STATE_DISABLED) {
    goto ErrOut;
  }

  if (!CdnspRing) {
    switch (TrbCompCode) {
      case COMP_INVALID_STREAM_ID_ERROR:
      case COMP_INVALID_STREAM_TYPE_ERROR:
      case COMP_RING_OVERRUN:
      case COMP_RING_UNDERRUN:
        goto CleanUp;
      default:
        DEBUG ((DEBUG_ERROR, "Ep%d,Event for Unkonwn CompCode(%d)\n", EpIndex, TrbCompCode));
        goto ErrOut;
    }
  }

  switch (TrbCompCode) {
    case COMP_BABBLE_DETECTED_ERROR:
      Status = EFI_BUFFER_TOO_SMALL;
      break;
    case COMP_RING_OVERRUN:
    case COMP_RING_UNDERRUN:
      goto CleanUp;
    case COMP_MISSED_SERVICE_ERROR:
      //
      // should never meet not support isoc
      //
      CdnspEp->Skip = TRUE;
      break;
  }

  do {
    if (IsListEmpty (&CdnspRing->TdList)) {
      if (CdnspEp->Skip) {
        CdnspEp->Skip = FALSE;
      }

      goto CleanUp;
    }

    Td = (CDNSP_TD *)GetFirstNode (&CdnspRing->TdList);

    EpSegment = CdnspTrbInTd (
                  CdnspRing->DeqSeg,
                  CdnspRing->Dequeue,
                  Td->LastTrb,
                  Event->Buffer
                  );

    if (!EpSegment && ((TrbCompCode == COMP_STOPPED) ||
                       (TrbCompCode == COMP_STOPPED_LENGTH_INVALID)))
    {
      CdnspEp->Skip = FALSE;
      goto CleanUp;
    }

    Desc = Td->Preq->Ep->Endpoint.Des;

    if (!EpSegment) {
      DEBUG ((DEBUG_ERROR, "Transfer Event Trb Address not part of current Td EpIndex = %d, comp_code(%d)\n", EpIndex, TrbCompCode));
      return EFI_INVALID_PARAMETER;
    }

    if (TrbCompCode == COMP_SHORT_PACKET) {
      CdnspRing->LastTdWasShort = TRUE;
    } else {
      CdnspRing->LastTdWasShort = FALSE;
    }

    EpTrb = (CDNSP_TRB *)Event->Buffer;
    if (CdnspTrbIsNoop (EpTrb)) {
      goto CleanUp;
    }

    if (UsbEndpointXferControl (Desc)) {
      CdnspProcessCtrlTd (CdnspDevice, Td, EpTrb, Event, CdnspEp, &EpStatus);
    } else {
      CdnspProcessBulkIntrTd (CdnspDevice, Td, EpTrb, Event, CdnspEp, &EpStatus);
    }

CleanUp:
    HandlingSkippedTds = CdnspEp->Skip;
    if (!HandlingSkippedTds) {
      CdnspIncDeq (CdnspDevice, CdnspDevice->EventRing);
    }
  } while (HandlingSkippedTds);

  return EFI_SUCCESS;

ErrOut:
  DEBUG ((
    DEBUG_ERROR,
    "Dequeue Addres(0x%llx), 0x%x, 0x%x, %d, 0x%x\n",
    CdnspRing->Dequeue,
    LOWER_32_BITS (Event->Buffer),
    UPPER_32_BITS (Event->Buffer),
    Event->TransferLength,
    Event->Flags
    ));

  return EFI_INVALID_PARAMETER;
}

STATIC
VOID
CdnspHandleTxNrdy (
  CDNSP_DEVICE          *CdnspDevice,
  CDNSP_TRANSFER_EVENT  *Event
  )
{
  UINT32    EpIndex;
  CDNSP_EP  *EP;

  EpIndex = TRB_TO_EP_ID (Event->Flags) - 1;
  EP      = &CdnspDevice->Eps[EpIndex];

  if (!(EP->EpState & EP_HAS_STREAMS)) {
    return;
  } else {
    DEBUG ((DEBUG_ERROR, "Ep(%d) Not Support Stream\n", EpIndex));
  }
}

STATIC
BOOLEAN
CdnspHandleEvent (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  CDNSP_TRB   *Event;
  UINT32      Flags;
  UINT32      CycleBit;
  EFI_STATUS  Status;
  BOOLEAN     UpdatePtrs = TRUE;
  UINT32      CompCode;

  Event    = CdnspDevice->EventRing->Dequeue;
  Flags    = (UINT32)Event->EventCmd.Flags;
  CycleBit = (Flags & TRB_CYCLE);

  if (CycleBit != CdnspDevice->EventRing->CycleState) {
    return FALSE;
  }

  DEBUG ((DEBUG_EVENT_LOG, "Event(0x%llx),(0x%x),(0x%x)\n", Event->EventCmd.CmdTrb, Event->EventCmd.Flags, Event->EventCmd.Status));

  ArmDataSynchronizationBarrier ();

  switch (Flags & TRB_TYPE_BITMASK) {
    case TRB_TYPE (TRB_COMPLETION):
      CdnspIncDeq (CdnspDevice, CdnspDevice->CommandRing);
      break;
    case TRB_TYPE (TRB_PORT_STATUS):
      CdnspHandlePortStatus (CdnspDevice, Event);
      UpdatePtrs = FALSE;
      break;
    case TRB_TYPE (TRB_SETUP):
      CdnspDevice->Ep0Stage   = CDNSP_SETUP_STAGE;
      CdnspDevice->SetupId    = TRB_SETUPID_TO_TYPE (Flags);
      CdnspDevice->SetupSpeed = TRB_SETUP_SPEEDID (Flags);
      CdnspDevice->Setup      = *((USB_DEVICE_REQUEST *)&Event->TransferEvent.Buffer);
      CdnspSetupAnalyze (CdnspDevice);
      break;
    case TRB_TYPE (TRB_TRANSFER):
      Status = CdnspHandleTxEvent (CdnspDevice, &Event->TransferEvent);
      if (!EFI_ERROR (Status)) {
        UpdatePtrs = FALSE;
      }

      break;
    case TRB_TYPE (TRB_ENDPOINT_NRDY):
      CdnspHandleTxNrdy (CdnspDevice, &Event->TransferEvent);
      break;
    case TRB_TYPE (TRB_HC_EVENT):
      CompCode = GET_COMP_CODE (Event->Generic.Filed[2]);
      switch (CompCode) {
        case COMP_EVENT_RING_FULL_ERROR:
          DEBUG ((DEBUG_ERROR, "Event Ring Full\n"));
          break;
        default:
          DEBUG ((DEBUG_ERROR, "Error Code(0x%x)\n", CompCode));
      }

      break;
    case TRB_TYPE (TRB_MFINDEX_WRAP):
    case TRB_TYPE (TRB_DRB_OVERFLOW):
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Unknown event type(%ld)\n", TRB_FIELD_TO_TYPE (Flags)));
  }

  if (UpdatePtrs) {
    CdnspIncDeq (CdnspDevice, CdnspDevice->EventRing);
  }

  return TRUE;
}

EFI_STATUS
CdnspEventRingHandler (
  CDNSP_DEVICE  *CdnspDevice
  )
{
  UINT32      IrqPending;
  EFI_STATUS  Status = 0;
  CDNSP_TRB   *EventRingDeq;
  INT32       Counter = 0;

  Status = CdnspRead (&CdnspDevice->OpRegs->Sts);

  if (Status  == ~(UINT32)0) {
    CdnspDied (CdnspDevice);
    Status = EFI_ACCESS_DENIED;
    goto OnExit;
  }

  if (!(Status & STS_EINT)) {
    Status = EFI_NOT_READY;
    goto OnExit;
  }

  CdnspWrite (Status | STS_EINT, &CdnspDevice->OpRegs->Sts);
  IrqPending  = CdnspRead (&CdnspDevice->IntrRegs->IrqPending);
  IrqPending |= IMAN_IP;
  CdnspWrite (IrqPending, &CdnspDevice->IntrRegs->IrqPending);

  if (Status & STS_FATAL) {
    CdnspDied (CdnspDevice);
    Status = EFI_ACCESS_DENIED;
    goto OnExit;
  }

  if (CdnspDevice->CdnspState & (CDNSP_STATE_HALTED | CDNSP_STATE_DYING)) {
    CdnspDied (CdnspDevice);
    Status = EFI_ACCESS_DENIED;
    goto OnExit;
  }

  EventRingDeq = CdnspDevice->EventRing->Dequeue;

  while (CdnspHandleEvent (CdnspDevice)) {
    if (++Counter >= TRBS_PER_EV_DEQ_UPDATE) {
      CdnspUpdateErstDequeue (CdnspDevice, EventRingDeq, 0);
      EventRingDeq = CdnspDevice->EventRing->Dequeue;
      Counter      = 0;
    }
  }

  CdnspUpdateErstDequeue (CdnspDevice, EventRingDeq, 1);

OnExit:
  return Status;
}
