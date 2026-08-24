/** Plat.c

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "Plat.h"

EFI_STATUS
ReadPollTimeout (
  VOID     *Address,
  BOOLEAN  ClearOrSet,
  UINT32   BitWait,
  UINT32   DelayMs,
  UINT32   TimeoutMs
  )
{
  UINT32  n = DIV_ROUND_UP (TimeoutMs, DelayMs) + 1;
  UINT32  Value;

  for ( ; ;) {
    Value = CdnspRead (Address);
    if (ClearOrSet) {
      if (Value & BitWait) {
        break;
      }
    } else {
      if ((Value & BitWait) == 0) {
        break;
      }
    }

    gBS->Stall (DelayMs);
    n--;
    if (n == 0) {
      break;
    }
  }

  if (n == 0) {
    return EFI_TIMEOUT;
  } else {
    return EFI_SUCCESS;
  }
}
