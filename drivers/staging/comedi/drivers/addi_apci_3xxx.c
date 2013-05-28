#include <linux/pci.h>

#include "../comedidev.h"
#include "comedi_fc.h"
#include "amcc_s5933.h"

#include "addi-data/addi_common.h"

#include "addi-data/addi_eeprom.c"
#include "addi-data/hwdrv_apci3xxx.c"
#include "addi-data/addi_common.c"

enum apci3xxx_boardid {
	BOARD_APCI3000_16,
	BOARD_APCI3000_8,
	BOARD_APCI3000_4,
	BOARD_APCI3006_16,
	BOARD_APCI3006_8,
	BOARD_APCI3006_4,
	BOARD_APCI3010_16,
	BOARD_APCI3010_8,
	BOARD_APCI3010_4,
	BOARD_APCI3016_16,
	BOARD_APCI3016_8,
	BOARD_APCI3016_4,
	BOARD_APCI3100_16_4,
	BOARD_APCI3100_8_4,
	BOARD_APCI3106_16_4,
	BOARD_APCI3106_8_4,
	BOARD_APCI3110_16_4,
	BOARD_APCI3110_8_4,
	BOARD_APCI3116_16_4,
	BOARD_APCI3116_8_4,
	BOARD_APCI3003,
	BOARD_APCI3002_16,
	BOARD_APCI3002_8,
	BOARD_APCI3002_4,
	BOARD_APCI3500,
};

static const struct addi_board apci3xxx_boardtypes[] = {
	[BOARD_APCI3000_16] = {
		.pc_DriverName		= "apci3000-16",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3000_8] = {
		.pc_DriverName		= "apci3000-8",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3000_4] = {
		.pc_DriverName		= "apci3000-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3006_16] = {
		.pc_DriverName		= "apci3006-16",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3006_8] = {
		.pc_DriverName		= "apci3006-8",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3006_4] = {
		.pc_DriverName		= "apci3006-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3010_16] = {
		.pc_DriverName		= "apci3010-16",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3010_8] = {
		.pc_DriverName		= "apci3010-8",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3010_4] = {
		.pc_DriverName		= "apci3010-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3016_16] = {
		.pc_DriverName		= "apci3016-16",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3016_8] = {
		.pc_DriverName		= "apci3016-8",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3016_4] = {
		.pc_DriverName		= "apci3016-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 4,
		.i_NbrAiChannelDiff	= 2,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3100_16_4] = {
		.pc_DriverName		= "apci3100-16-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3100_8_4] = {
		.pc_DriverName		= "apci3100-8-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3106_16_4] = {
		.pc_DriverName		= "apci3106-16-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3106_8_4] = {
		.pc_DriverName		= "apci3106-8-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 10000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3110_16_4] = {
		.pc_DriverName		= "apci3110-16-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3110_8_4] = {
		.pc_DriverName		= "apci3110-8-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 4095,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3116_16_4] = {
		.pc_DriverName		= "apci3116-16-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 16,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 16,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3116_8_4] = {
		.pc_DriverName		= "apci3116-8-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannel		= 8,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 8,
		.i_NbrAoChannel		= 4,
		.i_AiMaxdata		= 65535,
		.i_AoMaxdata		= 4095,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.i_NbrTTLChannel	= 24,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
	[BOARD_APCI3003] = {
		.pc_DriverName		= "apci3003",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 7,
		.ui_MinAcquisitiontimeNs = 2500,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
	},
	[BOARD_APCI3002_16] = {
		.pc_DriverName		= "apci3002-16",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 16,
		.i_AiChannelList	= 16,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
	},
	[BOARD_APCI3002_8] = {
		.pc_DriverName		= "apci3002-8",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 8,
		.i_AiChannelList	= 8,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
	},
	[BOARD_APCI3002_4] = {
		.pc_DriverName		= "apci3002-4",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAiChannelDiff	= 4,
		.i_AiChannelList	= 4,
		.i_AiMaxdata		= 65535,
		.pr_AiRangelist		= &range_apci3XXX_ai,
		.i_NbrDiChannel		= 4,
		.i_NbrDoChannel		= 4,
		.i_DoMaxdata		= 1,
		.b_AvailableConvertUnit	= 6,
		.ui_MinAcquisitiontimeNs = 5000,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ai_config		= i_APCI3XXX_InsnConfigAnalogInput,
		.ai_read		= i_APCI3XXX_InsnReadAnalogInput,
		.di_bits		= apci3xxx_di_insn_bits,
		.do_bits		= apci3xxx_do_insn_bits,
	},
	[BOARD_APCI3500] = {
		.pc_DriverName		= "apci3500",
		.i_IorangeBase1		= 256,
		.i_PCIEeprom		= ADDIDATA_NO_EEPROM,
		.pc_EepromChip		= ADDIDATA_9054,
		.i_NbrAoChannel		= 4,
		.i_AoMaxdata		= 4095,
		.pr_AoRangelist		= &range_apci3XXX_ao,
		.i_NbrTTLChannel	= 24,
		.interrupt		= v_APCI3XXX_Interrupt,
		.reset			= i_APCI3XXX_Reset,
		.ao_write		= i_APCI3XXX_InsnWriteAnalogOutput,
		.ttl_config		= i_APCI3XXX_InsnConfigInitTTLIO,
		.ttl_bits		= i_APCI3XXX_InsnBitsTTLIO,
		.ttl_read		= i_APCI3XXX_InsnReadTTLIO,
		.ttl_write		= i_APCI3XXX_InsnWriteTTLIO,
	},
};

static int apci3xxx_auto_attach(struct comedi_device *dev,
				unsigned long context)
{
	const struct addi_board *board = NULL;

	if (context < ARRAY_SIZE(apci3xxx_boardtypes))
		board = &apci3xxx_boardtypes[context];
	if (!board)
		return -ENODEV;
	dev->board_ptr = board;

	return addi_auto_attach(dev, context);
}

static struct comedi_driver apci3xxx_driver = {
	.driver_name	= "addi_apci_3xxx",
	.module		= THIS_MODULE,
	.auto_attach	= apci3xxx_auto_attach,
	.detach		= i_ADDI_Detach,
};

static int apci3xxx_pci_probe(struct pci_dev *dev,
			      const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &apci3xxx_driver, id->driver_data);
}

static DEFINE_PCI_DEVICE_TABLE(apci3xxx_pci_table) = {
	{ PCI_VDEVICE(ADDIDATA, 0x3010), BOARD_APCI3000_16 },
	{ PCI_VDEVICE(ADDIDATA, 0x300f), BOARD_APCI3000_8 },
	{ PCI_VDEVICE(ADDIDATA, 0x300e), BOARD_APCI3000_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3013), BOARD_APCI3006_16 },
	{ PCI_VDEVICE(ADDIDATA, 0x3014), BOARD_APCI3006_8 },
	{ PCI_VDEVICE(ADDIDATA, 0x3015), BOARD_APCI3006_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3016), BOARD_APCI3010_16 },
	{ PCI_VDEVICE(ADDIDATA, 0x3017), BOARD_APCI3010_8 },
	{ PCI_VDEVICE(ADDIDATA, 0x3018), BOARD_APCI3010_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3019), BOARD_APCI3016_16 },
	{ PCI_VDEVICE(ADDIDATA, 0x301a), BOARD_APCI3016_8 },
	{ PCI_VDEVICE(ADDIDATA, 0x301b), BOARD_APCI3016_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x301c), BOARD_APCI3100_16_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x301d), BOARD_APCI3100_8_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x301e), BOARD_APCI3106_16_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x301f), BOARD_APCI3106_8_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3020), BOARD_APCI3110_16_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3021), BOARD_APCI3110_8_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3022), BOARD_APCI3116_16_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3023), BOARD_APCI3116_8_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x300B), BOARD_APCI3003 },
	{ PCI_VDEVICE(ADDIDATA, 0x3002), BOARD_APCI3002_16 },
	{ PCI_VDEVICE(ADDIDATA, 0x3003), BOARD_APCI3002_8 },
	{ PCI_VDEVICE(ADDIDATA, 0x3004), BOARD_APCI3002_4 },
	{ PCI_VDEVICE(ADDIDATA, 0x3024), BOARD_APCI3500 },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, apci3xxx_pci_table);

static struct pci_driver apci3xxx_pci_driver = {
	.name		= "addi_apci_3xxx",
	.id_table	= apci3xxx_pci_table,
	.probe		= apci3xxx_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(apci3xxx_driver, apci3xxx_pci_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
