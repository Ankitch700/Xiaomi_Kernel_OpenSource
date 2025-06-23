/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Arm Ltd.
 * Copyright (C) 2022-2023, X-Ring technologies Inc., All rights reserved.
 */

#ifndef __INCLUDE_MPAM_REGISTER_H_
#define __INCLUDE_MPAM_REGISTER_H_

#include <linux/bits.h>

/*
 * MPAM MSCs have the following register layout. See:
 * Arm Architecture Reference Manual Supplement - Memory System Resource
 * Partitioning and Monitoring (MPAM), for Armv8-A. DDI 0598A.a
 */

/* CPU Registers */
/*
 * SYS_MPAM0_EL1 and SYS_MPAM1_EL1 are defined in android13 5.15
 * But not defined in android 15 6.6
 */
#ifndef SYS_MPAM0_EL1
#define SYS_MPAM0_EL1			sys_reg(3, 0, 10, 5, 1)
#endif
#ifndef SYS_MPAM1_EL1
#define SYS_MPAM1_EL1			sys_reg(3, 0, 10, 5, 0)
#endif
#ifndef SYS_MPAMIDR_EL1
#define SYS_MPAMIDR_EL1			sys_reg(3, 0, 10, 4, 4)
#endif

#define MPAM_SYSREG_EN BIT_ULL(63)
#define MPAM_SYSREG_TRAP_IDR BIT_ULL(58)
#define MPAM_SYSREG_TRAP_MPAM0_EL1 BIT_ULL(49)
#define MPAM_SYSREG_TRAP_MPAM1_EL1 BIT_ULL(48)
#define MPAM_SYSREG_PMG_D GENMASK(47, 40)
#define MPAM_SYSREG_PMG_I GENMASK(39, 32)
#define MPAM_SYSREG_PARTID_D GENMASK(31, 16)
#define MPAM_SYSREG_PARTID_I GENMASK(15, 0)
#define MPAM_SYSREG_ARCH_MAJOR_REV GENMASK(43, 40)
#define MPAM_SYSREG_ARCH_MINOR_REV GENMASK(19, 16)

#define MPAMIDR_PMG_MAX GENMASK(40, 32)
#define MPAMIDR_PMG_MAX_SHIFT 32
#define MPAMIDR_PMG_MAX_LEN 8
#define MPAMIDR_VPMR_MAX GENMASK(20, 18)
#define MPAMIDR_VPMR_MAX_SHIFT 18
#define MPAMIDR_VPMR_MAX_LEN 3
#define MPAMIDR_HAS_HCR BIT(17)
#define MPAMIDR_HAS_HCR_SHIFT 17
#define MPAMIDR_PARTID_MAX GENMASK(15, 0)
#define MPAMIDR_PARTID_MAX_SHIFT 0
#define MPAMIDR_PARTID_MAX_LEN 15

#define MPAMHCR_EL0_VPMEN BIT_ULL(0)
#define MPAMHCR_EL1_VPMEN BIT_ULL(1)
#define MPAMHCR_GSTAPP_PLK BIT_ULL(8)
#define MPAMHCR_TRAP_MPAMIDR BIT_ULL(31)

/* Properties of the VPM registers */
#define MPAM_VPM_NUM_REGS 8
#define MPAM_VPM_PARTID_LEN 16
#define MPAM_VPM_PARTID_MASK 0xffff
#define MPAM_VPM_REG_LEN 64
#define MPAM_VPM_PARTIDS_PER_REG (MPAM_VPM_REG_LEN / MPAM_VPM_PARTID_LEN)
#define MPAM_VPM_MAX_PARTID (MPAM_VPM_NUM_REGS * MPAM_VPM_PARTIDS_PER_REG)

/* Memory mapped control pages: */
/* ID Register offsets in the memory mapped page */
#define MPAMF_IDR 0x0000 /* features id register */
#define MPAMF_MSMON_IDR 0x0080 /* performance monitoring features */
#define MPAMF_IMPL_IDR 0x0028 /* imp-def partitioning */
#define MPAMF_CPOR_IDR 0x0030 /* cache-portion partitioning */
#define MPAMF_CCAP_IDR 0x0038 /* cache-capacity partitioning */
#define MPAMF_MBW_IDR 0x0040 /* mem-bw partitioning */
#define MPAMF_PRI_IDR 0x0048 /* priority partitioning */
#define MPAMF_CSUMON_IDR 0x0088 /* cache-usage monitor */
#define MPAMF_MBWUMON_IDR 0x0090 /* mem-bw usage monitor */
#define MPAMF_PARTID_NRW_IDR 0x0050 /* partid-narrowing */
#define MPAMF_IIDR 0x0018 /* implementer id register */
#define MPAMF_AIDR 0x0020 /* architectural id register */

/* Configuration and Status Register offsets in the memory mapped page */
#define MPAMCFG_PART_SEL 0x0100 /* partid to configure: */
#define MPAMCFG_CPBM 0x1000 /* cache-portion config */
#define MPAMCFG_CMAX 0x0108 /* cache-capacity config */
#define MPAMCFG_MBW_MIN 0x0200 /* min mem-bw config */
#define MPAMCFG_MBW_MAX 0x0208 /* max mem-bw config */
#define MPAMCFG_MBW_WINWD 0x0220 /* mem-bw accounting window config */
#define MPAMCFG_MBW_PBM 0x2000 /* mem-bw portion bitmap config */
#define MPAMCFG_PRI 0x0400 /* priority partitioning config */
#define MPAMCFG_MBW_PROP 0x0500 /* mem-bw stride config */
#define MPAMCFG_INTPARTID 0x0600 /* partid-narrowing config */

#define MSMON_CFG_MON_SEL 0x0800 /* monitor selector */
#define MSMON_CFG_CSU_FLT 0x0810 /* cache-usage monitor filter */
#define MSMON_CFG_CSU_CTL 0x0818 /* cache-usage monitor config */
#define MSMON_CFG_MBWU_FLT 0x0820 /* mem-bw monitor filter */
#define MSMON_CFG_MBWU_CTL 0x0828 /* mem-bw monitor config */
#define MSMON_CSU 0x0840 /* current cache-usage */
#define MSMON_CSU_CAPTURE 0x0848 /* last cache-usage value captured */
#define MSMON_MBWU 0x0860 /* current mem-bw usage value */
#define MSMON_MBWU_CAPTURE 0x0868 /* last mem-bw value captured */
#define MSMON_MBWU_L 0x0880 /* current long mem-bw usage value */
#define MSMON_MBWU_CAPTURE_L 0x0890 /* last long mem-bw value captured */
#define MSMON_CAPT_EVNT 0x0808 /* signal a capture event */
#define MPAMF_ESR 0x00F8 /* error status register */
#define MPAMF_ECR 0x00F0 /* error control register */

/* MPAMF_IDR - MPAM features ID register */
#define MPAMF_IDR_PARTID_MAX GENMASK_ULL(15, 0)
#define MPAMF_IDR_PMG_MAX GENMASK_ULL(23, 16)
#define MPAMF_IDR_HAS_CCAP_PART BIT(24)
#define MPAMF_IDR_HAS_CPOR_PART BIT(25)
#define MPAMF_IDR_HAS_MBW_PART BIT(26)
#define MPAMF_IDR_HAS_PRI_PART BIT(27)
#define MPAMF_IDR_HAS_EXT BIT(28)
#define MPAMF_IDR_HAS_IMPL_IDR BIT(29)
#define MPAMF_IDR_HAS_MSMON BIT(30)
#define MPAMF_IDR_HAS_PARTID_NRW BIT(31)
#define MPAMF_IDR_HAS_RIS BIT(32)
#define MPAMF_IDR_HAS_EXT_ESR BIT(38)
#define MPAMF_IDR_HAS_ESR BIT(39)
#define MPAMF_IDR_RIS_MAX GENMASK_ULL(59, 56)

/* MPAMF_MSMON_IDR - MPAM performance monitoring ID register */
#define MPAMF_MSMON_IDR_MSMON_CSU BIT(16)
#define MPAMF_MSMON_IDR_MSMON_MBWU BIT(17)
#define MPAMF_MSMON_IDR_HAS_LOCAL_CAPT_EVNT BIT(31)

/* MPAMF_CPOR_IDR - MPAM features cache portion partitioning ID register */
#define MPAMF_CPOR_IDR_CPBM_WD GENMASK(15, 0)

/* MPAMF_CCAP_IDR - MPAM features cache capacity partitioning ID register */
#define MPAMF_CCAP_IDR_CMAX_WD GENMASK(5, 0)

/* MPAMF_MBW_IDR - MPAM features memory bandwidth partitioning ID register */
#define MPAMF_MBW_IDR_BWA_WD GENMASK(5, 0)
#define MPAMF_MBW_IDR_HAS_MIN BIT(10)
#define MPAMF_MBW_IDR_HAS_MAX BIT(11)
#define MPAMF_MBW_IDR_HAS_PBM BIT(12)
#define MPAMF_MBW_IDR_HAS_PROP BIT(13)
#define MPAMF_MBW_IDR_WINDWR BIT(14)
#define MPAMF_MBW_IDR_BWPBM_WD GENMASK(28, 16)

/* MPAMF_PRI_IDR - MPAM features priority partitioning ID register */
#define MPAMF_PRI_IDR_HAS_INTPRI BIT(0)
#define MPAMF_PRI_IDR_INTPRI_0_IS_LOW BIT(1)
#define MPAMF_PRI_IDR_INTPRI_WD GENMASK(9, 4)
#define MPAMF_PRI_IDR_HAS_DSPRI BIT(16)
#define MPAMF_PRI_IDR_DSPRI_0_IS_LOW BIT(17)
#define MPAMF_PRI_IDR_DSPRI_WD GENMASK(25, 20)

/* MPAMF_CSUMON_IDR - MPAM cache storage usage monitor ID register */
#define MPAMF_CSUMON_IDR_NUM_MON GENMASK(15, 0)
#define MPAMF_CSUMON_IDR_HAS_CAPTURE BIT(31)

/* MPAMF_MBWUMON_IDR - MPAM memory bandwidth usage monitor ID register */
#define MPAMF_MBWUMON_IDR_NUM_MON GENMASK(15, 0)
#define MPAMF_MBWUMON_IDR_HAS_RWBW BIT(28)
#define MPAMF_MBWUMON_IDR_LWD BIT(29)
#define MPAMF_MBWUMON_IDR_HAS_LONG BIT(30)
#define MPAMF_MBWUMON_IDR_HAS_CAPTURE BIT(31)

/* MPAMF_PARTID_NRW_IDR - MPAM PARTID narrowing ID register */
#define MPAMF_PARTID_NRW_IDR_INTPARTID_MAX GENMASK(15, 0)

/* MPAMF_IIDR - MPAM implementation ID register */
#define MPAMF_IIDR_PRODUCTID GENMASK(31, 20)
#define MPAMF_IIDR_PRODUCTID_SHIFT 20
#define MPAMF_IIDR_VARIANT GENMASK(19, 16)
#define MPAMF_IIDR_VARIANT_SHIFT 16
#define MPAMF_IIDR_REVISON GENMASK(15, 12)
#define MPAMF_IIDR_REVISON_SHIFT 12
#define MPAMF_IIDR_IMPLEMENTER GENMASK(11, 0)
#define MPAMF_IIDR_IMPLEMENTER_SHIFT 0

/* MPAMF_AIDR - MPAM architecture ID register */
#define MPAMF_AIDR_ARCH_MAJOR_REV GENMASK(7, 4)
#define MPAMF_AIDR_ARCH_MINOR_REV GENMASK(3, 0)

/* MPAMCFG_PART_SEL - MPAM partition configuration selection register */
#define MPAMCFG_PART_SEL_PARTID_SEL GENMASK(15, 0)
#define MPAMCFG_PART_SEL_INTERNAL BIT(16)
#define MPAMCFG_PART_SEL_RIS GENMASK(27, 24)

/* MPAMCFG_CMAX - MPAM cache portion bitmap partition configuration register */
#define MPAMCFG_CMAX_CMAX GENMASK(15, 0)

/*
 * MPAMCFG_MBW_MIN - MPAM memory minimum bandwidth partitioning configuration
 *                   register
 */
#define MPAMCFG_MBW_MIN_MIN GENMASK(15, 0)

/*
 * MPAMCFG_MBW_MAX - MPAM memory maximum bandwidth partitioning configuration
 *                   register
 */
#define MPAMCFG_MBW_MAX_MAX GENMASK(15, 0)
#define MPAMCFG_MBW_MAX_HARDLIM BIT(31)

/*
 * MPAMCFG_MBW_WINWD - MPAM memory bandwidth partitioning window width
 *                     register
 */
#define MPAMCFG_MBW_WINWD_US_FRAC GENMASK(7, 0)
#define MPAMCFG_MBW_WINWD_US_INT GENMASK(23, 8)

/* MPAMCFG_PRI - MPAM priority partitioning configuration register */
#define MPAMCFG_PRI_INTPRI GENMASK(15, 0)
#define MPAMCFG_PRI_DSPRI GENMASK(31, 16)

/*
 * MPAMCFG_MBW_PROP - Memory bandwidth proportional stride partitioning
 *                    configuration register
 */
#define MPAMCFG_MBW_PROP_STRIDEM1 GENMASK(15, 0)
#define MPAMCFG_MBW_PROP_EN BIT(31)

/*
 * MPAMCFG_INTPARTID - MPAM internal partition narrowing configuration register
 */
#define MPAMCFG_INTPARTID_INTPARTID GENMASK(15, 0)
#define MPAMCFG_INTPARTID_INTERNAL BIT(16)

/* MSMON_CFG_MON_SEL - Memory system performance monitor selection register */
#define MSMON_CFG_MON_SEL_MON_SEL GENMASK(7, 0)
#define MSMON_CFG_MON_SEL_RIS GENMASK(27, 24)

/* MPAMF_ESR - MPAM Error Status Register */
#define MPAMF_ESR_PARTID_OR_MON GENMASK_ULL(15, 0)
#define MPAMF_ESR_PMG GENMASK_ULL(23, 16)
#define MPAMF_ESR_ERRCODE GENMASK_ULL(27, 24)
#define MPAMF_ESR_OVRWR BIT(31)
#define MPAMF_ESR_RIS GENMASK_ULL(35, 32)

/* MPAMF_ECR - MPAM Error Control Register */
#define MPAMF_ECR_INTEN BIT(0)

/* Error conditions in accessing memory mapped registers */
#define MPAM_ERRCODE_NONE 0
#define MPAM_ERRCODE_PARTID_SEL_RANGE 1
#define MPAM_ERRCODE_REQ_PARTID_RANGE 2
#define MPAM_ERRCODE_MSMONCFG_ID_RANGE 3
#define MPAM_ERRCODE_REQ_PMG_RANGE 4
#define MPAM_ERRCODE_MONITOR_RANGE 5
#define MPAM_ERRCODE_INTPARTID_RANGE 6
#define MPAM_ERRCODE_UNEXPECTED_INTERNAL 7
/*
 * MSMON_CFG_CSU_FLT - Memory system performance monitor configure cache storage
 *                    usage monitor filter register
 */
#define MSMON_CFG_CSU_FLT_PARTID GENMASK(15, 0)
#define MSMON_CFG_CSU_FLT_PMG GENMASK(23, 16)

/*
 * MSMON_CFG_CSU_CTL - Memory system performance monitor configure cache storage
 *                    usage monitor control register
 * MSMON_CFG_MBWU_CTL - Memory system performance monitor configure memory
 *                     bandwidth usage monitor control register
 */
#define MSMON_CFG_x_CTL_TYPE GENMASK(7, 0)
#define MSMON_CFG_x_CTL_MATCH_PARTID BIT(16)
#define MSMON_CFG_x_CTL_MATCH_PMG BIT(17)
#define MSMON_CFG_x_CTL_SCLEN BIT(19)
#define MSMON_CFG_x_CTL_SUBTYPE GENMASK(23, 20)
#define MSMON_CFG_x_CTL_OFLOW_FRZ BIT(24)
#define MSMON_CFG_x_CTL_OFLOW_INTR BIT(25)
#define MSMON_CFG_x_CTL_OFLOW_STATUS BIT(26)
#define MSMON_CFG_x_CTL_CAPT_RESET BIT(27)
#define MSMON_CFG_x_CTL_CAPT_EVNT GENMASK(30, 28)
#define MSMON_CFG_x_CTL_EN BIT(31)

#define MSMON_CFG_MBWU_CTL_TYPE_MBWU 0x42
#define MSMON_CFG_MBWU_CTL_TYPE_CSU 0x43

#define MSMON_CFG_MBWU_CTL_SUBTYPE_NONE 0
#define MSMON_CFG_MBWU_CTL_SUBTYPE_READ 1
#define MSMON_CFG_MBWU_CTL_SUBTYPE_WRITE 2
#define MSMON_CFG_MBWU_CTL_SUBTYPE_BOTH 3

#define MSMON_CFG_MBWU_CTL_SUBTYPE_MAX 3
#define MSMON_CFG_MBWU_CTL_SUBTYPE_MASK 0x3

/*
 * MSMON_CFG_MBWU_FLT - Memory system performance monitor configure memory
 *                     bandwidth usage monitor filter register
 */
#define MSMON_CFG_MBWU_FLT_PARTID GENMASK(15, 0)
#define MSMON_CFG_MBWU_FLT_PMG GENMASK(23, 16)
#define MSMON_CFG_MBWU_FLT_RWBW GENMASK(31, 30)

/*
 * MSMON_CSU - Memory system performance monitor cache storage usage monitor
 *            register
 * MSMON_CSU_CAPTURE -  Memory system performance monitor cache storage usage
 *                     capture register
 * MSMON_MBWU  - Memory system performance monitor memory bandwidth usage
 *               monitor register
 * MSMON_MBWU_CAPTURE - Memory system performance monitor memory bandwidth usage
 *                     capture register
 */
#define MSMON___VALUE GENMASK(30, 0)
#define MSMON___NRDY BIT(31)
#define MSMON___NRDY_L BIT(63)
#define MSMON___L_VALUE GENMASK(43, 0)
#define MSMON___LWD_VALUE GENMASK(62, 0)

/*
 * MSMON_CAPT_EVNT - Memory system performance monitoring capture event
 *                  generation register
 */
#define MSMON_CAPT_EVNT_NOW BIT(0)

#endif /* __MPAM_REGISTER_H_ */
