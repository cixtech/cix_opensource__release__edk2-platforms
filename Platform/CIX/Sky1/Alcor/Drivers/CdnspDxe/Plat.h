/** Plat.h

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PLAT_H
#define _PLAT_H

#include <Library/IoLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UncachedMemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/ArmLib.h>
#include "Cdnsp.h"

STATIC
inline
UINT32
CdnspWrite (
  UINT32  Value,
  VOID    *Address
  )
{
  UINT32  RetValue;

  RetValue = (UINT32)MmioWrite32 ((UINTN)Address, Value);
  gBS->Stall (10);
  return RetValue;
}

STATIC
inline
UINT32
CdnspRead (
  VOID  *Address
  )
{
  UINT32  RetValue;

  RetValue = (UINT32)MmioRead32 ((UINTN)Address);
  return RetValue;
}

STATIC
inline
UINT64
CdnspRead64 (
  VOID  *Address
  )
{
  UINT32  High;
  UINT32  Low;

  Low  = (UINT32)MmioRead32 ((UINTN)Address);
  High = (UINT32)MmioRead32 ((UINTN)Address + 4);
  return (Low + (((UINT64)High) << 32));
}

STATIC
inline
VOID
CdnspWrite64 (
  UINT64  Value,
  VOID    *Address
  )
{
  MmioWrite32 ((UINTN)Address, Value);
  MmioWrite32 ((UINTN)Address + 4, Value >> 32);
  gBS->Stall (10);
}

EFI_STATUS
ReadPollTimeout (
  VOID     *Address,
  BOOLEAN  ClearOrSet,
  UINT32   BitWait,
  UINT32   DelayMs,
  UINT32   TimeoutMs
  );

#endif
