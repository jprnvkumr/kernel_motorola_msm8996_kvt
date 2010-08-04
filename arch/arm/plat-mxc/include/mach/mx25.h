#ifndef __MACH_MX25_H__
#define __MACH_MX25_H__

#define MX25_AIPS1_BASE_ADDR		0x43f00000
#define MX25_AIPS1_BASE_ADDR_VIRT	0xfc000000
#define MX25_AIPS1_SIZE			SZ_1M
#define MX25_AIPS2_BASE_ADDR		0x53f00000
#define MX25_AIPS2_BASE_ADDR_VIRT	0xfc200000
#define MX25_AIPS2_SIZE			SZ_1M
#define MX25_AVIC_BASE_ADDR		0x68000000
#define MX25_AVIC_BASE_ADDR_VIRT	0xfc400000
#define MX25_AVIC_SIZE			SZ_1M

#define MX25_IOMUXC_BASE_ADDR		(MX25_AIPS1_BASE_ADDR + 0xac000)

#define MX25_CRM_BASE_ADDR		(MX25_AIPS2_BASE_ADDR + 0x80000)
#define MX25_GPT1_BASE_ADDR		(MX25_AIPS2_BASE_ADDR + 0x90000)
#define MX25_WDOG_BASE_ADDR		(MX25_AIPS2_BASE_ADDR + 0xdc000)

#define MX25_GPIO1_BASE_ADDR_VIRT	(MX25_AIPS2_BASE_ADDR_VIRT + 0xcc000)
#define MX25_GPIO2_BASE_ADDR_VIRT	(MX25_AIPS2_BASE_ADDR_VIRT + 0xd0000)
#define MX25_GPIO3_BASE_ADDR_VIRT	(MX25_AIPS2_BASE_ADDR_VIRT + 0xa4000)
#define MX25_GPIO4_BASE_ADDR_VIRT	(MX25_AIPS2_BASE_ADDR_VIRT + 0x9c000)

#define MX25_IO_ADDRESS(x) (					\
	IMX_IO_ADDRESS(x, MX25_AIPS1) ?:			\
	IMX_IO_ADDRESS(x, MX25_AIPS2) ?:			\
	IMX_IO_ADDRESS(x, MX25_AVIC))

#define MX25_UART1_BASE_ADDR		0x43f90000
#define MX25_UART2_BASE_ADDR		0x43f94000

#define MX25_FEC_BASE_ADDR		0x50038000
#define MX25_NFC_BASE_ADDR		0xbb000000
#define MX25_DRYICE_BASE_ADDR		0x53ffc000
#define MX25_LCDC_BASE_ADDR		0x53fbc000

#define MX25_INT_DRYICE	25
#define MX25_INT_FEC	57
#define MX25_INT_NANDFC	33
#define MX25_INT_LCDC	39

#if defined(IMX_NEEDS_DEPRECATED_SYMBOLS)
#define UART1_BASE_ADDR			MX25_UART1_BASE_ADDR
#define UART2_BASE_ADDR			MX25_UART2_BASE_ADDR
#endif

#endif /* ifndef __MACH_MX25_H__ */
