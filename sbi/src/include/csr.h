#pragma once

#define CSR_READ(var, csr) \
   asm volatile("csrr %0, " csr : "=r"(var))
#define CSR_WRITE(csr, var) \
   asm volatile("csrw " csr ", %0" :: "r"(var))


#define MRET() asm volatile("mret")
#define SRET() asm volatile("mret")
#define WFI()  asm volatile("wfi")


#define MSTATUS_MPP_USER         (0UL << 11)
#define MSTATUS_MPP_SUPERVISOR   (1UL << 11)
#define MSTATUS_MPP_HYPERVISOR   (2UL << 11)
#define MSTATUS_MPP_MACHINE      (3UL << 11)

#define SSTATUS_SPP_USER         (0UL << 8)
#define SSTATUS_SPP_SUPERVISOR   (1UL << 8)

#define MSTATUS_MPIE             (1UL << 7)

#define SSTATUS_SPIE             (1UL << 5)

// FS: floating point status bits
#define MSTATUS_FS_OFF           (0UL << 13)
#define MSTATUS_FS_INITIAL       (1UL << 13)
#define MSTATUS_FS_CLEAN         (2UL << 13)
#define MSTATUS_FS_DIRTY         (3UL << 13)

#define SSTATUS_FS_OFF           (0UL << 13)
#define SSTATUS_FS_INITIAL       (1UL << 13)
#define SSTATUS_FS_CLEAN         (2UL << 13)
#define SSTATUS_FS_DIRTY         (3UL << 13)

#define MIE_MEIE                 (1UL << 11)
#define MIE_MTIE                 (1UL << 7)
#define MIE_MSIE                 (1UL << 3)
#define MIE_STIE                 (1UL << 5)

#define MIP_SSIP                 (1UL << 1)
#define MIP_STIP                 (1UL << 5)
#define MIP_SEIP                 (1UL << 9)

#define SIE_SSIE                 (1UL << 1)
#define SIE_STIE                 (1UL << 5)
#define SIE_SEIE                 (1UL << 9)

#define SIP_SSIP                 (1UL << 1)
#define SIP_STIP                 (1UL << 5)
#define SIP_SEIP                 (1UL << 9)

#define MEDELEG_ALL              (0xB1F7UL)

#define XREG_ZERO                (0)
#define XREG_RA                  (1)
#define XREG_SP                  (2)
#define XREG_G                   (3)
#define XREG_TP                  (4)
#define XREG_T0                  (5)
#define XREG_T1                  (6)
#define XREG_T2                  (7)
#define XREG_S0                  (8)
#define XREG_S1                  (9)
#define XREG_A0                  (10)
#define XREG_A1                  (11)
#define XREG_A2                  (12)
#define XREG_A3                  (13)
#define XREG_A4                  (14)
#define XREG_A5                  (15)
#define XREG_A6                  (16)
#define XREG_A7                  (17)
#define XREG_S2                  (18)
#define XREG_S3                  (19)
#define XREG_S4                  (20)
#define XREG_S5                  (21)
#define XREG_S6                  (22)
#define XREG_S7                  (23)
#define XREG_S8                  (24)
#define XREG_S9                  (25)
#define XREG_S10                 (26)
#define XREG_S11                 (27)
#define XREG_T3                  (28)
#define XREG_T4                  (29)
#define XREG_T5                  (30)
#define XREG_T6                  (31)

#define OS_LOAD_ADDR             (0x80050000UL)
