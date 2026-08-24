/** @file
 *
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 **/

#ifndef _CDNSP_H
#define _CDNSP_H

#include "Plat.h"
#include <UsbCommon.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/UsbFunctionIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/BaseLib.h>
#include <Protocol/UsbIo.h>
#include <Library/UncachedMemoryAllocationLib.h>
#include <Protocol/BoardInitProtocol.h>
#include <Protocol/EFIUsbDeviceControl.h>
#include <Library/CixPostCodeLib.h>
#include <Library/PcdLib.h>

#if __SIZEOF_POINTER__ == 8
#define BITS_PER_LONG  64
#elif __SIZEOF_POINTER__ == 4
#define BITS_PER_LONG  32
#else
  #error "Unexpected __SIZEOF_POINTER__"
#endif

#define DEBUG_LOG_NONE  0

#define ENABLE_COMMON_LOG    0
#define ENABLE_REGISTER_LOG  0
#define ENABLE_TRB_LOG       0

#if ENABLE_COMMON_LOG == 1
#define DEBUG_COMMON_LOG  DEBUG_ERROR
#else
#define DEBUG_COMMON_LOG  DEBUG_LOG_NONE
#endif

#if ENABLE_REGISTER_LOG == 1
#define DEBUG_REGISTER_LOG  DEBUG_ERROR
#else
#define DEBUG_REGISTER_LOG  DEBUG_LOG_NONE
#endif

#if ENABLE_EVENT_LOG == 1
#define DEBUG_EVENT_LOG  DEBUG_ERROR
#else
#define DEBUG_EVENT_LOG  DEBUG_LOG_NONE
#endif

#if ENABLE_TRB_LOG == 1
#define DEBUG_TRB_LOG  DEBUG_ERROR
#else
#define DEBUG_TRB_LOG  DEBUG_LOG_NONE
#endif

#define D_XEC_PRE_REGS_CAP           0xC8
#define REG_CHICKEN_BITS_2_OFFSET    0x48
#define CHICKEN_XDMA_2_TP_CACHE_DIS  BIT(28)

#define HCC_EXT_CAPS(p)   (((p) & GENMASK(31, 16)) >> 16)
#define EXT_CAPS_ID(p)    (((p) >> 0) & GENMASK(7, 0))
#define EXT_CAPS_NEXT(p)  (((p) >> 8) & GENMASK(7, 0))

#define UPPER_32_BITS(n)  ((UINT32)(((n) >> 16) >> 16))
#define LOWER_32_BITS(n)  ((UINT32)(n))

#define DIV_ROUND_UP(n, d)      (((n) + (d) - 1) / (d))
#define IS_ALIGNED(addr, size)  (((addr) & (size - 1)) ? 0 : 1)
#define BITS_TO_LONGS(nbits)    (((nbits) + BITS_PER_LONG - 1) / \
          BITS_PER_LONG)
#define BIT(nr)                 (1UL << (nr))
#define BIT_MASK(nr)            (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(bit)           ((bit) / BITS_PER_LONG)
#define BIT_WORD_OFFSET(bit)    ((bit) & (BITS_PER_LONG - 1))
#define GENMASK(h, l) \
  (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define CDNSP_IF_EP_EXIST(pdev, ep_num, dir) \
        (CdnspRead(&(pdev)->RevCap->EpSupported) & \
        (BIT(ep_num) << ((dir) ? 0 : 16)))

// OTG
/* CDNS_RID - bitmasks */
#define CDNS_RID(p)  ((p) & GENMASK(15, 0))
/* CDNS_VID - bitmasks */
#define CDNS_DID(p)  ((p) & GENMASK(31, 0))
/* OTGCMD - bitmasks */
/* "Request the bus for Device mode. */
#define OTGCMD_DEV_BUS_REQ  BIT(0)
/* Request the bus for Host mode */
#define OTGCMD_HOST_BUS_REQ  BIT(1)
/* Enable OTG mode. */
#define OTGCMD_OTG_EN  BIT(2)
/* Disable OTG mode */
#define OTGCMD_OTG_DIS  BIT(3)
/*"Configure OTG as A-Device. */
#define OTGCMD_A_DEV_EN  BIT(4)
/*"Configure OTG as A-Device. */
#define OTGCMD_A_DEV_DIS  BIT(5)
/* Drop the bus for Device mod  e. */
#define OTGCMD_DEV_BUS_DROP  BIT(8)
/* Drop the bus for Host mode*/
#define OTGCMD_HOST_BUS_DROP  BIT(9)
/* OTGIEN - bitmasks */
/* ID change interrupt enable */
#define OTGIEN_ID_CHANGE_INT  BIT(0)
/* Vbusvalid fall detected interrupt enable.*/
#define OTGIEN_VBUSVALID_RISE_INT  BIT(4)
/* Vbusvalid fall detected interrupt enable */
#define OTGIEN_VBUSVALID_FALL_INT  BIT(5)
/* OTGSTS - bitmasks */

/*
 * Current value of the ID pin. It is only valid when idpullup in
 *  OTGCTRL1_TYPE register is set to '1'.
 */
#define OTGSTS_ID_VALUE  BIT(0)
/* Current value of the vbus_valid */
#define OTGSTS_VBUS_VALID  BIT(1)
/* Current value of the b_sess_vld */
#define OTGSTS_SESSION_VALID  BIT(2)
/*Device mode is active*/
#define OTGSTS_DEV_ACTIVE  BIT(3)
/* Host mode is active. */
#define OTGSTS_HOST_ACTIVE  BIT(4)
/* OTG Controller not ready. */
#define OTGSTS_OTG_NRDY_MASK  BIT(11)
#define OTGSTS_OTG_NRDY(p)  ((p) & OTGSTS_OTG_NRDY_MASK)

/*
 * Value of the strap pins for:
 * CDNSP:
 * 000 - No default configuration.
 * 010 - Controller initiall configured as Host.
 * 100 - Controller initially configured as Device.
 */
#define OTGSTS_STRAP(p)  (((p) & GENMASK(14, 12)) >> 12)
#define OTGSTS_STRAP_NO_DEFAULT_CFG  0x00
#define OTGSTS_STRAP_HOST_OTG        0x01
#define OTGSTS_STRAP_HOST            0x02
#define OTGSTS_STRAP_GADGET          0x04
#define OTGSTS_CDNSP_STRAP_HOST      0x01
#define OTGSTS_CDNSP_STRAP_GADGET    0x02
/* Host mode is turned on. */
#define OTGSTS_CDNSP_XHCI_READY  BIT(27)
/* "Device mode is turned on .*/
#define OTGSTS_CDNSP_DEV_READY  BIT(26)
/* OTGSTATE- bitmasks */
#define OTGSTATE_DEV_STATE_MASK        GENMASK(2, 0)
#define OTGSTATE_HOST_STATE_MASK       GENMASK(5, 3)
#define OTGSTATE_HOST_STATE_IDLE       0x0
#define OTGSTATE_HOST_STATE_VBUS_FALL  0x7
#define OTGSTATE_HOST_STATE(p)  (((p) & OTGSTATE_HOST_STATE_MASK) >> 3)
/* OTGREFCLK - bitmasks */
#define OTGREFCLK_STB_CLK_SWITCH_EN  BIT(31)
/* OVERRIDE - bitmasks */
#define OVERRIDE_IDPULLUP  BIT(0)

typedef struct _CDNSP_DEVICE  CDNSP_DEVICE;
typedef struct _CDNSP_TD      CDNSP_TD;

typedef struct {
  UINT32    Did;
  UINT32    Rid;
  UINT32    Cfg1;
  UINT32    Cfg2;
  UINT32    Cmd;
  UINT32    Sts;
  UINT32    State;
  UINT32    IEn;
  UINT32    IVect;
  UINT32    Tmr;
  UINT32    Simulate;
  UINT32    AdpbcSts;
  UINT32    AdpRampTime;
  UINT32    AdpbcCtrl1;
  UINT32    AdpbcCtrl2;
  /* Vbusvalid/Sesvalid override select. */
  #define OVERRIDE_SESS_VLD_SEL  BIT(10)
  UINT32    Override;
  UINT32    VbusvalidDbncCfg;
  UINT32    SessvalidDbncCfg;
  UINT32    SuspTimingCtrl;
} CDNSP_OTG_REGS;

#define CAP_REGS_OFFSET  0x4000

typedef struct {
  //
  // CAPLENTH(8bit) Rsvd(8bit) HCIVERSION(32bit) interface Version Number
  // Base + 00h, Base + CAPLENTH = start address of Op regs
  //

  #define HC_LENGTH(p)   (((p) >> 00) & GENMASK(7, 0))
  #define HC_VERSION(p)  (((p) >> 16) & GENMASK(15, 1))

  UINT32    HcCapBase;
  UINT32    HcsParams1;
  UINT32    HcsParams2;
  UINT32    HcsParams3;
  #define HCC_64BIT_ADDR(p)      ((p) & BIT(0))
  #define HCC_64BYTE_CONTEXT(p)  ((p) & BIT(2))
  #define CTX_SIZE(_hcc)         (HCC_64BYTE_CONTEXT(_hcc) ? 64 : 32)
  #define HCC_MAX_PSA(p)         ((((p) >> 12) & 0xf) + 1)
  #define STREAM_LOG_STREAMS  4
  #define STREAM_NUM_STREAMS  BIT(STREAM_LOG_STREAMS)
  UINT32    HccParams1;
  //
  // doorbell array offset
  //
  #define DBOFF_MASK  GENMASK(31, 2)
  UINT32    DbOff;
  //
  // Runtime Register Space Offset
  //
  #define RTSOFF_MASK  GENMASK(31, 5)
  UINT32    RunRegsOff;
  UINT32    HccParams2;
  //
  // Reserved up to (CAPLENGTH - 0x1C)
  //
} CDNSP_CAP_REGS;

typedef struct {
  #define CMD_R_S     BIT(0)
  #define CMD_RESET   BIT(1)
  #define CMD_INTE    BIT(2)
  #define CMD_DSEIE   BIT(3)
  #define CMD_EWE     BIT(10)
  #define CMD_DEVEN   BIT(17)
  #define CDNSP_IRQS  (CMD_INTE | CMD_DSEIE | CMD_EWE)
  UINT32    Cmd;
  #define STS_HALT   BIT(0)
  #define STS_FATAL  BIT(2)
  #define STS_EINT   BIT(3)
  #define STS_CNR    BIT(11)
  UINT32    Sts;
  UINT32    PageSize;
  UINT32    Rvsd1;
  UINT32    Rvsd2;
  //
  // Device Notification Control
  //
  UINT32    Dnctrl;
  //
  // Command Ring Control
  //
  #define CRCR_RCS  BIT(0)
  #define CRCR_CS   BIT(1)
  #define CRCR_CA   BIT(2)
  #define CRCR_CRR  BIT(3)
  // command ring busy(RO)
  #define CRCR_RB               BIT(4)
  #define CRCR_RSVD_BITS        GENMASK(5, 0)
  #define CDNSP_CMD_TIMEOUT_MS  16
  UINT64    CRCR;
  UINT32    Rvsd3[4];
  //
  // Device Context Base Address Array Pointer
  //
  UINT64    Dcbaap;
  #define CDNSP_DEV_MAX_SLOTS  1
  #define MAX_DEVS             GENMASK(7, 0)
  #define CONFIG_U3E           BIT(8)
  UINT32    Config;
  UINT32    Rvsd4[241];
  UINT32    PortRegisterBase;
  #define NUM_PORT_REGS  4
  //
  // up tp 13FFh
  //
} CDNSP_OP_REGS;

//
// runtime registers, start at RunRegsOff + base of CDNSP_CAP_REGS
//
typedef struct {
  /* IMAN - Interrupt Management Register - irq_pending bitmasks l. */
  #define IMAN_IE  BIT(1)
  #define IMAN_IP  BIT(0)
  /* bits 2:31 need to be preserved */
  #define IMAN_IE_SET(p)    ((p) | IMAN_IE)
  #define IMAN_IE_CLEAR(p)  ((p) & ~IMAN_IE)
  UINT32    IrqPending;
  #define IMOD_INTERVAL_MASK     GENMASK(15, 0)
  #define IMOD_DEFAULT_INTERVAL  0
  UINT32    IrqControl;
  #define ERST_SIZE_MASK  GENMASK(31, 16)
  #define ERST_NUM_SEGS   1
  UINT32    ErstSz;
  UINT32    Rvsd;
  UINT64    ErstBa;
  #define ERST_DESI_MASK  GENMASK(2, 0)
  #define ERST_EHB        BIT(3)
  #define ERST_PTR_MASK   GENMASK(3, 0)
  UINT64    ErDq;
} CDNSP_INTR_REGS;

typedef struct {
  UINT32             MfIndex;
  UINT32             Rsvd[7];
  CDNSP_INTR_REGS    IrSet[128];
} CDNSP_RUN_REGS;

typedef struct {
  #define TRB_MAX_BUFF_SHIFT  16
  #define TRB_MAX_BUFF_SIZE   BIT(TRB_MAX_BUFF_SHIFT)
  #define TRB_BUFF_LEN_UP_TO_BOUNDARY(addr)  (TRB_MAX_BUFF_SIZE -\
            ((addr) & (TRB_MAX_BUFF_SIZE - 1)))
  UINT64    Buffer;
  #define TRB_INTR_TARGET(p)  (((p) << 22) & GENMASK(31, 22))
  #define EVENT_TRB_LEN(p)    ((p) & GENMASK(23, 0))
  UINT32    TransferLength;
  #define TRB_SETUPID_BITMASK  GENMASK(9, 8)
  #define TRB_SETUPID(p)          ((p) << 8)
  #define TRB_SETUPID_TO_TYPE(p)  (((p) & TRB_SETUPID_BITMASK) >> 8)
  #define TRB_SETUP_SPEEDID(p)    ((p) & (1 << 7))
  #define TRB_SETUP_SPEEDID_USB3  0x1
  #define TRB_SETUP_SPEEDID_USB2  0x0
  /* Invalidate event after disabling endpoint. */
  #define TRB_EVENT_INVALIDATE  8
  UINT32    Flags;
} CDNSP_TRANSFER_EVENT;

typedef struct {
  UINT64    SegmentPtr;
  #define TRB_LEN(p)      ((p) & GENMASK(16, 0))
  #define TRB_TD_SIZE(p)  (MIN((p), (UINT32)31) << 17)
  #define GET_TD_SIZE(p)  (((p) & GENMASK(21, 17)) >> 17)
  UINT32    IntrTarget;
  #define TRB_CYCLE         BIT(0)
  #define LINK_TOGGLE       BIT(1)
  #define TRB_ISP           BIT(2)
  #define TRB_CHAIN         BIT(4)
  #define TRB_IOC           BIT(5)
  #define TRB_TYPE_BITMASK  GENMASK(15, 10)
  #define TRB_TYPE(p)  ((p) << 10)
  #define TRB_NORMAL  1
  #define TRB_SETUP   2
  #define TRB_DATA    3
  #define TRB_STATUS  4
  #define TRB_ISOC    5
  #define TRB_LINK    6
  #define TRB_DIR_IN  BIT(16)
  #define TRB_TYPE_LINK(x)  (((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))
  // STAGE
  #define TRB_SETUPSTAT_ACK    0x1
  #define TRB_SETUPSTAT_STALL  0x0
  #define TRB_SETUPSTAT(p)  ((p) << 6)
  UINT32    Control;
} CDNSP_LINK_TRB;

typedef enum {
  SETUP_CONTEXT_ONLY,
  SETUP_CONTEXT_ADDRESS,
} CDNSP_SETUP_DEV;

typedef struct {
  UINT32    DropFlags;
  #define SLOT_FLAG  BIT(0)
  #define EP0_FLAG   BIT(1)
  UINT32    AddFlags;
  UINT32    Rsvd[6];
} CDNSP_INPUT_CONTROL_CTX;

typedef struct {
  UINT64    CmdTrb;
  UINT32    Status;
  UINT32    Flags;
} CDNSP_EVENT_CMD;

#define TRB_ENDPOINT_NRDY   48
#define TRB_HALT_ENDPOINT   54
#define TRB_DRB_OVERFLOW    57
#define TRB_FLUSH_ENDPOINT  58
typedef struct {
  #define GET_PORT_ID(p)  (((p) & GENMASK(31, 24)) >> 24)
  #define SET_PORT_ID(p)  (((p) << 24) & GENMASK(31, 24))
  #define COMP_CODE_MASK  (0xff << 24)
  #define GET_COMP_CODE(p)  (((p) & COMP_CODE_MASK) >> 24)
  #define COMP_INVALID  0
  #define COMP_SUCCESS  1

  #define TRB_TO_SLOT_ID(p)   (((p) & GENMASK(31, 24)) >> 24)
  #define SLOT_ID_FOR_TRB(p)  (((p) << 24) & GENMASK(31, 24))
  /* Enable Slot Command. */
  #define TRB_ENABLE_SLOT  9
  /* Disable Slot Command. */
  #define TRB_DISABLE_SLOT  10
  #define TRB_BSR           BIT(9)
  #define TRB_TYPE_NOOP(x)      (((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_TR_NOOP))
  #define TRB_FIELD_TO_TYPE(p)  (((p) & TRB_TYPE_BITMASK) >> 10)
  UINT32    Filed[4];
} CDNSP_GENERIC_TRB;

typedef union {
  CDNSP_LINK_TRB          Link;
  CDNSP_TRANSFER_EVENT    TransferEvent;
  CDNSP_EVENT_CMD         EventCmd;
  CDNSP_GENERIC_TRB       Generic;
} CDNSP_TRB;

typedef struct _CDNSP_SEGMENT  CDNSP_SEGMENT;
typedef struct _CDNSP_REQUEST  CDNSP_REQUEST;

struct _CDNSP_SEGMENT {
  #define TRBS_PER_SEGMENT        256
  #define TRB_SEGMENT_SIZE        (TRBS_PER_SEGMENT * 16)
  #define TRBS_PER_EV_DEQ_UPDATE  100
  CDNSP_TRB        *Trbs;
  CDNSP_SEGMENT    *Next;
  VOID             *BouncdBuf;
  UINT32           BounceOffset;
  UINT32           BounceLength;
};

typedef enum {
  TYPE_CTRL = 0,
  TYPE_ISOC,
  TYPE_BULK,
  TYPE_INTR,
  TYPE_STREAM,
  TYPE_COMMAND,
  TYPE_EVENT,
} CDNSP_RING_TYPE;

typedef struct {
  CDNSP_SEGMENT      *FirstSeg;
  CDNSP_SEGMENT      *LastSeg;
  CDNSP_TRB          *Enqueue;
  CDNSP_SEGMENT      *EnqSeg;
  CDNSP_TRB          *Dequeue;
  CDNSP_SEGMENT      *DeqSeg;
  LIST_ENTRY         TdList;
  UINT32             CycleState;
  UINT32             StreamID;
  UINT32             StreamActive;
  UINT32             StreamRejected;
  INT32              NumTds;
  UINT32             NumSegs;
  UINT32             NumTrbsFree;
  UINT32             BounceBufLength;
  CDNSP_RING_TYPE    Type;
  BOOLEAN            LastTdWasShort;
} CDNSP_RING;

typedef struct {
  CDNSP_SEGMENT    *NewDeqSeg;
  CDNSP_TRB        *NewDequeue;
  INT32            NewCycleState;
} CDNSP_DEQUEUE_STATE;

typedef struct {
  UINT32    CmdDb;
  #define DB_VALUE(ep, stream)          ((((ep) + 1) & 0xff) | ((stream) << 16))
  #define DB_VALUE_EP0_OUT(ep, stream)  ((ep) & 0xff)
  #define DB_VALUE_CMD  0x00000000
  // 0 EP 0 out enqueue pointer update
  // 1 EP 0 in enqueue pointer update
  // 2 EP 1 out enqueue pointer update
  // 3 EP 1 in enqueue pointer update
  // .....
  UINT32    EpDb;
} CDNSP_DOORBELL_ARRAY;

typedef struct {
  UINT64    SegAddr;
  UINT32    SegSize;
  UINT32    Rsvd;
} CDNSP_ERST_ENTRY;
typedef struct {
  CDNSP_ERST_ENTRY    *Entries;
  UINT32              NumEntries;
} CDNSP_ERST;

typedef struct {
  #define EXT_CAP_CFG_DEV_20PORT_CAP_ID  0xC1
  UINT32    ExtCap;
  UINT32    PortReg1;
  UINT32    PortReg2;
  UINT32    PortReg3;
  UINT32    PortReg4;
  UINT32    PortReg5;
  #define PORT_REG6_L1_L0_HW_EN  BIT(1)
  UINT32    PortReg6;
} CDNSP_20PORT_CAP;

typedef struct {
  #define D_XEC_CFG_3XPORT_CAP  0xC0
  UINT32    ExtCap;
  #define PORT_REG6_FORCE_FS      BIT(0)
  #define CFG_3XPORT_SSP_SUPPORT  BIT(31)
  UINT32    ModeAddr;
  UINT32    Rsvd[52];
  // enable clk_pipe_div_g gating during U1 state
  #define CFG_3XPORT_U1_PIPE_CLK_GATE_EN  BIT(0)
  UINT32    Mode2;
} CDNSP_3XPORT_CAP;

// usb2 offset = 0x4480 usb3 offset = 0x4490
typedef struct {
  #define PORT_CONNECT    BIT(0)
  #define PORT_PED        BIT(1)
  #define PORT_RESET      BIT(4)
  #define PORT_PLS_MASK   GENMASK(8, 5)
  #define XDEV_U0         (0x0 << 5)
  #define XDEV_U1         (0x1 << 5)
  #define XDEV_U2         (0x2 << 5)
  #define XDEV_U3         (0x3 << 5)
  #define XDEV_DISABLED   (0x4 << 5)
  #define XDEV_RXDETECT   (0x5 << 5)
  #define XDEV_INACTIVE   (0x6 << 5)
  #define XDEV_POLLING    (0x7 << 5)
  #define XDEV_RECOVERY   (0x8 << 5)
  #define XDEV_HOT_RESET  (0x9 << 5)
  #define XDEV_COMP_MODE  (0xa << 5)
  #define XDEV_TEST_MODE  (0xb << 5)
  #define XDEV_RESUME     (0xf << 5)
  #define DEV_SPEED_MASK  GENMASK(13, 10)
  #define XDEV_FS         (0x1 << 10)
  #define XDEV_HS         (0x3 << 10)
  #define XDEV_SS         (0x4 << 10)
  #define XDEV_SSP        (0x5 << 10)
  #define DEV_UNDEFSPEED(p)      (((p) & DEV_SPEED_MASK) == (0x0 << 10))
  #define DEV_FULLSPEED(p)       (((p) & DEV_SPEED_MASK) == XDEV_FS)
  #define DEV_HIGHSPEED(p)       (((p) & DEV_SPEED_MASK) == XDEV_HS)
  #define DEV_SUPERSPEED(p)      (((p) & DEV_SPEED_MASK) == XDEV_SS)
  #define DEV_SUPERSPEEDPLUS(p)  (((p) & DEV_SPEED_MASK) == XDEV_SSP)
  #define DEV_SUPERSPEED_ANY(p)  (((p) & DEV_SPEED_MASK) >= XDEV_SS)
  #define DEV_PORT_SPEED(p)      (((p) >> 10) & 0x0f)
  #define PORT_LINK_STROBE  BIT(16)
  #define PORT_CSC          BIT(17)
  #define PORT_WRC          BIT(19)
  #define PORT_RC           BIT(21)
  #define PORT_PLC          BIT(22)
  #define PORT_CEC          BIT(23)
  #define PORT_OCA          BIT(20)
  #define PORT_WKCONN_E     BIT(25)
  #define PORT_WKDISC_E     BIT(26)
  #define CDNSP_PORT_RO     (PORT_CONNECT | DEV_SPEED_MASK)
  #define CDNSP_PORT_RWS    (PORT_PLS_MASK | PORT_WKCONN_E | PORT_WKDISC_E)
  #define CDNSP_PORT_RW1CS  (PORT_PED | PORT_CSC | PORT_RC | PORT_PLC)
  #define PORT_CHANGE_BITS  (PORT_CSC | PORT_WRC | PORT_RC | PORT_PLC | PORT_CEC)
  UINT32    PortSc;
  UINT32    PortPmsc;
  UINT32    PortLi;
  UINT32    Rsvd;
} CDNSP_PORT_REGS;

// D_XEC_SUPP_USB2_CAP0
#define EXT_CAPS_PROTOCOL  2
#define CDNSP_EXT_PORT_MAJOR(x)  (((x) >> 24) & 0xff)
#define CDNSP_EXT_PORT_MINOR(x)  (((x) >> 16) & 0xff)
// D_XEC_SUPP_USB2_CAP2
#define CDNSP_EXT_PORT_OFF(x)    ((x) & 0xff)
#define CDNSP_EXT_PORT_COUNT(x)  (((x) >> 8) & 0xff)

typedef struct {
  CDNSP_PORT_REGS    *Regs;
  UINT8              PortNum;
  UINT8              Exist;
  UINT8              MajRev;
  UINT8              MinRev;
} CDNSP_PORT;

/*
  context map
  option if input context + input control context
  slot context
  endpoint 0 context
  endpoint 1 context out
  endpoint 1 context in
*/
typedef struct {
  UINT32    Type;
  #define CDNSP_CTX_TYPE_DEVICE  0x1
  #define CDNSP_CTX_TYPE_INPUT   0x2
  UINT32    Size;
  UINT32    CtxSize;
  UINT8     *Bytes;
} CDNSP_CONTAINER_CTX;

typedef struct {
  #define EP_MULT(p)      (((p) << 8) & GENMASK(9, 8))
  #define EP_INTERVAL(p)  (((p) << 16) & GENMASK(23, 16))
  UINT32    EpInfo;
  #define EP_TYPE(p)  ((p) << 3)
  #define ISOC_OUT_EP  1
  #define BULK_OUT_EP  2
  #define INT_OUT_EP   3
  #define CTRL_EP      4
  #define ISOC_IN_EP   5
  #define BULK_IN_EP   6
  #define INT_IN_EP    7
  #define MAX_PACKET(p)  (((p) << 16) & GENMASK(31, 16))
  #define MAX_PACKET_MASK  GENMASK(31, 16)
  #define MAX_PACKET_DECODED(p)  (((p) & GENMASK(31, 16)) >> 16)
  #define MAX_BURST(p)           (((p) << 8) & GENMASK(15, 8))
  #define ERROR_COUNT(p)         (((p) & 0x3) << 1)
  UINT32    EpInfo2;
  #define EP_MAX_ESIT_PAYLOAD_LO(p)  (((p) << 16) & GENMASK(31, 16))
  #define EP_MAX_ESIT_PAYLOAD_HI(p)  ((((p) & GENMASK(23, 16)) >> 16) << 24)
  UINT64    Deq;
  #define EP_AVG_TRB_LENGTH(p)  ((p) & GENMASK(15, 0))
  UINT32    TxInfo;
  UINT32    Rsvd[3];
} CDNSP_EP_CTX;

typedef struct {
  #define SLOT_SPEED_FS   (XDEV_FS << 10)
  #define SLOT_SPEED_HS   (XDEV_HS << 10)
  #define SLOT_SPEED_SS   (XDEV_SS << 10)
  #define SLOT_SPEED_SSP  (XDEV_SSP << 10)
  #define LAST_CTX(p)  ((p) << 27)
  #define LAST_CTX_MASK  ((UINT32)GENMASK(31, 27))
  UINT32    DevInfo;
  #define DEV_PORT(p)  (((p) & 0xff) << 16)
  UINT32    DevPort;
  UINT32    IntTarget;
  #define DEV_ADDR_MASK  GENMASK(7, 0)
  #define EP_ID_FOR_TRB(p)  ((((p) + 1) << 16) & GENMASK(20, 16))
  #define SLOT_STATE  GENMASK(31, 27)
  #define GET_SLOT_STATE(p)  (((p) & SLOT_STATE) >> 27)
  #define SLOT_STATE_DISABLED    0
  #define SLOT_STATE_ENABLED     SLOT_STATE_DISABLED
  #define SLOT_STATE_DEFAULT     1
  #define SLOT_STATE_ADDRESSED   2
  #define SLOT_STATE_CONFIGURED  3
  UINT32    DevState;
  UINT32    Reserved[4];
} CDNSP_SLOT_CTX;

typedef struct {
  USB_EP          Endpoint;
  LIST_ENTRY      PendingList;
  UINT8           Number;
  UINT8           Index;
  UINT8           Direction;
  BOOLEAN         Skip;
  UINT32          Internal;
  CDNSP_EP_CTX    *InCtx;
  CDNSP_EP_CTX    *OutCtx;
  CDNSP_RING      *Ring;
  UINT32          EpState;
  #define EP_ENABLED          BIT(0)
  #define EP_DIS_IN_RROGRESS  BIT(1)
  #define EP_HALTED           BIT(2)
  #define EP_STOPPED          BIT(3)
  #define EP_WEDGE            BIT(4)
  #define EP0_HALTED_STATUS   BIT(5)
  #define EP_HAS_STREAMS      BIT(6)
  #define EP_UNCONFIGURED     BIT(7)
  USB_REQUEST    *ResidentRequest;
} CDNSP_EP;

typedef struct {
  CDNSP_CONTAINER_CTX    *InCtx;
  UINT32                 Status;
  CDNSP_TRB              *CommandTrb;
} CDNSP_COMMAND;

typedef struct {
  #define RTL_REV_CAP  0xC4
  UINT32    ExtCap;
  UINT32    RtlRevision;
  UINT32    RxBuffSize;
  UINT32    TxBuffSize;
  UINT32    EpSupported;
  UINT32    CtrlRevision;
} CDNSP_REV_CAP;

struct _CDNSP_TD {
  LIST_ENTRY       TdList;
  CDNSP_REQUEST    *Preq;
  CDNSP_SEGMENT    *StartSeg;
  CDNSP_TRB        *FirstTrb;
  CDNSP_TRB        *LastTrb;
  CDNSP_SEGMENT    *BounceSeg;
  BOOLEAN          RequestLengthSet;
};

struct _CDNSP_REQUEST {
  USB_REQUEST    Request;
  CDNSP_EP       *Ep;
  CDNSP_TD       Td;
  UINT8          EpNum;
  UINT32         Direction : 1;
};

typedef enum {
  CDNSP_SETUP_STAGE,
  CDNSP_DATA_STAGE,
  CDNSP_STATUS_STAGE,
} CDNSP_EP0_STAGE;

struct _CDNSP_DEVICE {
  // offset =0
  CDNSP_OTG_REGS          *OtgRegs;
  CDNSP_CAP_REGS          *CapsRegs;
  UINT32                  HcsParams1;
  UINT32                  HcsParams3;
  #define HCC_PARAMS_OFFSET  0x10
  UINT32                  HccParams1;
  UINT16                  HciVersion;
  CDNSP_OP_REGS           *OpRegs;
  CDNSP_RUN_REGS          *RunRegs;
  CDNSP_INTR_REGS         *IntrRegs;
  CDNSP_20PORT_CAP        *Port20Regs;
  CDNSP_3XPORT_CAP        *Port3xRegs;
  CDNSP_REV_CAP           *RevCap;
  // offset 0x7000
  CDNSP_DOORBELL_ARRAY    *Dba;
  #define CDNSP_STATE_HALTED              BIT(1)
  #define CDNSP_STATE_DYING               BIT(2)
  #define CDNSP_STATE_DISCONNECT_PENDING  BIT(3)
  #define CDNSP_WAKEUP_PENDING            BIT(4)
  UINTN                   BaseAddress;
  UINT32                  CdnspState;
  BOOLEAN                 Support64Address;
  UINT64                  *DevContextPtr;
  CDNSP_RING              *CommandRing;
  CDNSP_RING              *EventRing;
  // event ring segment table
  CDNSP_ERST              Erst;
  STD_USB_DEVICE_OPS      StdDeviceOps;
  CDNSP_PORT              Usb2Port;
  CDNSP_PORT              Usb3Port;
  CDNSP_PORT              *ActivePort;
  CDNSP_CONTAINER_CTX     OutCtx;
  CDNSP_CONTAINER_CTX     InCtx;
  #define CDNSP_ENDPOINTS_NUM  31
  CDNSP_EP                Eps[CDNSP_ENDPOINTS_NUM];
  #define CDNSP_EP0_SETUP_SIZE  512
  CDNSP_REQUEST           Ep0Preq;
  VOID                    *SetupBuf;
  UINT8                   ThreeStageSetup;
  CDNSP_EP0_STAGE         Ep0Stage;
  UINT8                   Ep0ExpectIn;
  UINT8                   SetupId;
  UINT8                   SetupSpeed;
  USB_DEVICE_REQUEST      Setup;
  CDNSP_COMMAND           Cmd;
  INT32                   SlotId;
  UINT32                  LinkState;
  INT32                   MayWakeup;

  UINT8                   Speed;
  UINT8                   MaxSpeed;
  UINT8                   DeviceAddress;
  UINT32                  SgSupport     : 1;
  UINT32                  LpmCable      : 1;
  UINT32                  IsSelfpowered : 1;
  UINT32                  Connected     : 1;
};

INT32
CdnspFindNextExtCap (
  VOID    *Base,
  UINT32  Start,
  INT32   Id
  );

EFI_STATUS
CdnspMemInit (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspDied (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspRingExpansion (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring,
  UINT32        NumTrb
  );

BOOLEAN
CdnspLastTrbOnSeg (
  CDNSP_SEGMENT  *Seg,
  CDNSP_TRB      *Trb
  );

BOOLEAN
CdnspLastTrbOnRing (
  CDNSP_RING     *Ring,
  CDNSP_SEGMENT  *Seg,
  CDNSP_TRB      *Trb
  );

EFI_STATUS
CdnspDisableSlot (
  CDNSP_DEVICE  *CdnspDevice
  );

CDNSP_EP_CTX *
CdnspGetEpCtx (
  CDNSP_CONTAINER_CTX  *Ctx,
  UINT32               EpIndex
  );

CDNSP_SLOT_CTX *
CdnspGetSlotCtx (
  CDNSP_CONTAINER_CTX  *Ctx
  );

VOID
CdnspQueueSlotControl (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        TrbType
  );

VOID
CdnspSetUsb2HardwareLpm (
  CDNSP_DEVICE  *CdnspDevice,
  USB_REQUEST   *Req,
  BOOLEAN       Enable
  );

UINT32
CdnspPortStateToNeutral (
  UINT32  State
  );

UINT32
CdnspPortSpeed (
  UINT32  PortStatus
  );

VOID
CdnspDeviceDisconnect (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspWaitForCmdCompl (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspQueueHaltEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  );

VOID
CdnspRingDoorbellForActiveRing (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  );

VOID
CdnspQueueResetEp (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  );

VOID
CdnspQueueResetDevice (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspQueueConfigureEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT64        InCtxAddress
  );

VOID
CdnspRingCmdDb (
  CDNSP_DEVICE  *CdnspDevice
  );

CDNSP_INPUT_CONTROL_CTX
*
CdnspGetInputControlCtx (
  CDNSP_CONTAINER_CTX  *Ctx
  );

EFI_STATUS
CdnspSetupAddressablePrivDev (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspIrqReset (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspHaltEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep,
  INT32         Value
  );

CDNSP_DEVICE
*
CdnspGetInstance (
  VOID
  );

EFI_STATUS
CdnspReset (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspResetDevice (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspSetupDevice (
  CDNSP_DEVICE     *CdnspDevice,
  CDNSP_SETUP_DEV  Setup
  );

EFI_STATUS
CdnspEp0SetAddress (
  UINT32  Addr
  );

EFI_STATUS
CdnspEndpointInit (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  );

VOID
CdnspFreeEndpointRings (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  );

EFI_STATUS
CdnspCmdStopEp (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep
  );

EFI_STATUS
CdnspCmdFlushEp (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *CdnspEp
  );

EFI_STATUS
CdnspInternalEpEnqueue (
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest
  );

EFI_STATUS
CdnspQueueCtrlTx (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest
  );

VOID
CdnspSetupAnalyze (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspGiveback (
  CDNSP_EP       *CdnspEp,
  CDNSP_REQUEST  *CdnspRequest,
  INT32          Status
  );

EFI_STATUS
CdnspSendCtrlResponse (
  INT32  BufferLength,
  VOID   *Buffer
  );

EFI_STATUS
CdnspQueueBulkTx (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_REQUEST  *CdnspRequest
  );

VOID
CdnspUpdateErstDequeue (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_TRB     *EventRingDeq,
  UINT8         ClearEhb
  );

EFI_STATUS
CdnspRemoveRequest (
  CDNSP_DEVICE   *CdnspDevice,
  CDNSP_REQUEST  *CdnspRequest,
  CDNSP_EP       *CdnspEp
  );

EFI_STATUS
CdnspInit (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspMemCleanup (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspConfigDeviceMode (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspQueueTransfer (
  UINT8  EpIndex,
  UINTN  *BufferSize,
  VOID   *Buffer
  );

EFI_STATUS
NotifyEvent (
  EFI_USBFN_MESSAGE   Event,
  USB_EP              *Ep,
  USB_REQUEST         *Request,
  USB_DEVICE_REQUEST  *CtrRequest
  );

VOID
CdnspSetVbus (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspEpEnable (
  USB_EP                   *Ep,
  USB_ENDPOINT_DESCRIPTOR  *Desc
  );

EFI_STATUS
CdnspEpDisable (
  USB_EP  *Ep
  );

VOID
CdnspCopyEp0DequeueIntoInputCtx (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspQueueAddressDevice (
  CDNSP_DEVICE     *CdnspDevice,
  UINT64           InCtxAddress,
  CDNSP_SETUP_DEV  Setup
  );

VOID
CdnspEndpointZero (
  CDNSP_EP  *CdnspEp
  );

VOID
CdnspClearVbus (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspEpFreeRequest (
  USB_EP       *Ep,
  USB_REQUEST  *Request
  );

EFI_STATUS
CdnspEventRingHandler (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspEnableSlot (
  CDNSP_DEVICE  *CdnspDevice
  );

BOOLEAN
CdnspRingEpDoorbell (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_EP      *Ep,
  UINT32        StremId
  );

EFI_STATUS
CdnspEpDequeueRequest (
  USB_EP       *Ep,
  USB_REQUEST  *Request
  );

VOID
CdnspIncDeq (
  CDNSP_DEVICE  *CdnspDevice,
  CDNSP_RING    *Ring
  );

VOID
CdnspInitializeRingInfo (
  CDNSP_RING  *Ring
  );

VOID
CdnspDisableInterfaceEndpoint (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspDisableDeviceMode (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspDeInit (
  CDNSP_DEVICE  *CdnspDevice
  );

EFI_STATUS
CdnspHalt (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspQueueStopEndpoint (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        EpIndex
  );

VOID
CdnspQueueCommand (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        Field1,
  UINT32        Field2,
  UINT32        Field3,
  UINT32        Field4
  );

VOID
CdnspStop (
  CDNSP_DEVICE  *CdnspDevice
  );

VOID
CdnspSetLinkState (
  CDNSP_DEVICE  *CdnspDevice,
  UINT32        *PortRegs,
  UINT32        LinkState
  );

#endif
