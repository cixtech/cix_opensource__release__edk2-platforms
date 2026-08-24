#include "BoardConfig.h"
#include "MemConfigBinTool.h"
#include "MemConfigurationTable.h"

#define ENTRY_COUNT 1

MEM_CONFIG_TRAIN_OPTIMIZE MemTrainOptimize = {
  {
    .Signature      = MEM_CONFIG_TRAIN_OPTIMIZE_SIGNAUTE,
    .BlockSize      = sizeof(MEM_CONFIG_TRAIN_OPTIMIZE) + sizeof(MEM_CONFIG_TRAIN_OPTIMIZE_ENTRY) * ENTRY_COUNT,
    .BoardMask      = BOARD_ID_MASK_DEFAULT
  },
  {
    //MaxMemFreq        RankPerCh ChMsk  Center Shift Th
    // {DDR5500_FREQUENCY, RANK_ALL,  0xF,    1,    0,   0},
    {DDR6000_FREQUENCY, RANK_ALL,  0xF,    1,    5,   0},
    // {DDR6400_FREQUENCY, RANK_ALL,  0xF,    1,    0,   0},
  }
};