#include "BoardConfig.h"
#include "MemConfigBinTool.h"
#include "MemConfigurationTable.h"

MEM_CONFIG_BLOCK_CONFIG  GlobalConfigBlock_alcor_4g = {
  {
    .Signature      = MEM_CONFIG_BLOCK_CONFIG_SIGNAUTE,
    .BlockSize      = sizeof(MEM_CONFIG_BLOCK_CONFIG),
    .BoardMask      = ALCOR_4G_MASK
  },
  .MaxFreq          = DDR6000_FREQUENCY,
  .ChMask           = 0xF,
  .DdrType          = DDR_TYPE_LPDDR5,
  .DeviceDensity    = 16,
  .DeviceWidth      = 16,
  .RankNum          = 1,
};

MEM_CONFIG_BLOCK_CONFIG  GlobalConfigBlock_alcor_8g = {
  {
    .Signature      = MEM_CONFIG_BLOCK_CONFIG_SIGNAUTE,
    .BlockSize      = sizeof(MEM_CONFIG_BLOCK_CONFIG),
    .BoardMask      = ALCOR_8G_MASK
  },
  .MaxFreq          = DDR6000_FREQUENCY,
  .ChMask           = 0xF,
  .DdrType          = DDR_TYPE_LPDDR5,
  .DeviceDensity    = 16,
  .DeviceWidth      = 16,
  .RankNum          = 2,
};

MEM_CONFIG_BLOCK_CONFIG  GlobalConfigBlock_alcor_16g = {
  {
    .Signature      = MEM_CONFIG_BLOCK_CONFIG_SIGNAUTE,
    .BlockSize      = sizeof(MEM_CONFIG_BLOCK_CONFIG),
    .BoardMask      = ALCOR_16G_MASK
  },
  .MaxFreq          = DDR6000_FREQUENCY,
  .ChMask           = 0xF,
  .DdrType          = DDR_TYPE_LPDDR5,
  .DeviceDensity    = 16,
  .DeviceWidth      = 8,
  .RankNum          = 2,
};
