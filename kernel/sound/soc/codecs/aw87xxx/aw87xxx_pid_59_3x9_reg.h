/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * @Descripttion: Header file of AW87XXX_PID_59_3X9_REG
 * @version: V1.33
 * @Author: zhaozhongbo
 * @Date: 2021-03-10
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-10
 */
#ifndef __AW87XXX_PID_59_3X9_REG_H__
#define __AW87XXX_PID_59_3X9_REG_H__

#define AW87XXX_PID_59_3X9_REG_CHIPID		(0x00)
#define AW87XXX_PID_59_3X9_REG_SYSCTRL		(0x01)
#define AW87XXX_PID_59_3X9_REG_MDCRTL		(0x02)
#define AW87XXX_PID_59_3X9_REG_CPOVP		(0x03)
#define AW87XXX_PID_59_3X9_REG_CPP		(0x04)
#define AW87XXX_PID_59_3X9_REG_PAG		(0x05)
#define AW87XXX_PID_59_3X9_REG_AGC3PO		(0x06)
#define AW87XXX_PID_59_3X9_REG_AGC3PA		(0x07)
#define AW87XXX_PID_59_3X9_REG_AGC2PO		(0x08)
#define AW87XXX_PID_59_3X9_REG_AGC2PA		(0x09)
#define AW87XXX_PID_59_3X9_REG_AGC1PA		(0x0A)
#define AW87XXX_PID_59_3X9_REG_SYSST		(0x59)
#define AW87XXX_PID_59_3X9_REG_SYSINT		(0x60)
#define AW87XXX_PID_59_3X9_REG_DFT_SYSCTRL	(0x61)
#define AW87XXX_PID_59_3X9_REG_DFT_MDCTRL	(0x62)
#define AW87XXX_PID_59_3X9_REG_DFT_CPOVP2	(0x63)
#define AW87XXX_PID_59_3X9_REG_DFT_AGCPA	(0x64)
#define AW87XXX_PID_59_3X9_REG_DFT_POFR		(0x65)
#define AW87XXX_PID_59_3X9_REG_DFT_OC		(0x66)
#define AW87XXX_PID_59_3X9_REG_DFT_OTA		(0x67)
#define AW87XXX_PID_59_3X9_REG_DFT_REF		(0x68)
#define AW87XXX_PID_59_3X9_REG_DFT_LDO		(0x69)
#define AW87XXX_PID_59_3X9_REG_ENCR		(0x70)

#define AW87XXX_PID_59_3X9_ENCR_DEFAULT		(0x00)

/********************************************
 * soft control info
 * If you need to update this file, add this information manually
 *******************************************/
unsigned char aw87xxx_pid_59_3x9_softrst_access[2] = {0x00, 0xaa};

/********************************************
 * Register Access
 *******************************************/
#define AW87XXX_PID_59_3X9_REG_MAX			(0x71)

#define REG_NONE_ACCESS		(0)
#define REG_RD_ACCESS		(1 << 0)
#define REG_WR_ACCESS		(1 << 1)

const unsigned char aw87xxx_pid_59_3x9_reg_access[AW87XXX_PID_59_3X9_REG_MAX] = {
	[AW87XXX_PID_59_3X9_REG_CHIPID]		= (REG_RD_ACCESS),
	[AW87XXX_PID_59_3X9_REG_SYSCTRL]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_MDCRTL]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_CPOVP]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_CPP]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_PAG]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_AGC3PO]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_AGC3PA]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_AGC2PO]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_AGC2PA]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_AGC1PA]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_SYSST]		= (REG_RD_ACCESS),
	[AW87XXX_PID_59_3X9_REG_SYSINT]		= (REG_RD_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_SYSCTRL]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_MDCTRL]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_CPOVP2]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_AGCPA]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_POFR]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_OC]		= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_OTA]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_REF]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_DFT_LDO]	= (REG_RD_ACCESS | REG_WR_ACCESS),
	[AW87XXX_PID_59_3X9_REG_ENCR]		= (REG_RD_ACCESS | REG_WR_ACCESS),
};

/* SPK_MODE bit 2 (MDCRTL 0x02) */
#define AW87XXX_PID_59_3X9_SPK_MODE_START_BIT	(2)
#define AW87XXX_PID_59_3X9_SPK_MODE_BITS_LEN	(1)
#define AW87XXX_PID_59_3X9_SPK_MODE_MASK	\
	(~(((1<<AW87XXX_PID_59_3X9_SPK_MODE_BITS_LEN)-1) << AW87XXX_PID_59_3X9_SPK_MODE_START_BIT))

#define AW87XXX_PID_59_3X9_SPK_MODE_DISABLE	(0)
#define AW87XXX_PID_59_3X9_SPK_MODE_DISABLE_VALUE	\
	(AW87XXX_PID_59_3X9_SPK_MODE_DISABLE << AW87XXX_PID_59_3X9_SPK_MODE_START_BIT)

#define AW87XXX_PID_59_3X9_SPK_MODE_ENABLE	(1)
#define AW87XXX_PID_59_3X9_SPK_MODE_ENABLE_VALUE	\
	(AW87XXX_PID_59_3X9_SPK_MODE_ENABLE << AW87XXX_PID_59_3X9_SPK_MODE_START_BIT)

#endif
