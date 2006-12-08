#ifndef __REGS_2700G_
#define __REGS_2700G_

/* extern unsigned long virt_base_2700; */
/* #define __REG_2700G(x)	(*(volatile unsigned long*)((x)+virt_base_2700)) */
#define __REG_2700G(x)	((x)+virt_base_2700)

/* System Configuration Registers (0x0000_0000  0x0000_0010) */
#define SYSCFG		__REG_2700G(0x00000000)
#define PFBASE		__REG_2700G(0x00000004)
#define PFCEIL		__REG_2700G(0x00000008)
#define POLLFLAG	__REG_2700G(0x0000000c)
#define SYSRST		__REG_2700G(0x00000010)

/* Interrupt Control Registers (0x0000_0014  0x0000_002F) */
#define NINTPW		__REG_2700G(0x00000014)
#define MINTENABLE	__REG_2700G(0x00000018)
#define MINTSTAT	__REG_2700G(0x0000001c)
#define SINTENABLE	__REG_2700G(0x00000020)
#define SINTSTAT	__REG_2700G(0x00000024)
#define SINTCLR		__REG_2700G(0x00000028)

/* Clock Control Registers (0x0000_002C  0x0000_005F) */
#define SYSCLKSRC	__REG_2700G(0x0000002c)
#define PIXCLKSRC	__REG_2700G(0x00000030)
#define CLKSLEEP	__REG_2700G(0x00000034)
#define COREPLL		__REG_2700G(0x00000038)
#define DISPPLL		__REG_2700G(0x0000003c)
#define PLLSTAT		__REG_2700G(0x00000040)
#define VOVRCLK		__REG_2700G(0x00000044)
#define PIXCLK		__REG_2700G(0x00000048)
#define MEMCLK		__REG_2700G(0x0000004c)
#define M24CLK		__REG_2700G(0x00000054)
#define MBXCLK		__REG_2700G(0x00000054)
#define SDCLK		__REG_2700G(0x00000058)
#define PIXCLKDIV	__REG_2700G(0x0000005c)

/* LCD Port Control Register (0x0000_0060  0x0000_006F) */
#define LCD_CONFIG	__REG_2700G(0x00000060)

/* On-Die Frame Buffer Registers (0x0000_0064  0x0000_006B) */
#define ODFBPWR		__REG_2700G(0x00000064)
#define ODFBSTAT	__REG_2700G(0x00000068)

/* GPIO Registers (0x0000_006C  0x0000_007F) */
#define GPIOCGF		__REG_2700G(0x0000006c)
#define GPIOHI		__REG_2700G(0x00000070)
#define GPIOLO		__REG_2700G(0x00000074)
#define GPIOSTAT	__REG_2700G(0x00000078)

/* Pulse Width Modulator (PWM) Registers (0x0000_0200  0x0000_02FF) */
#define PWMRST		__REG_2700G(0x00000200)
#define PWMCFG		__REG_2700G(0x00000204)
#define PWM0DIV		__REG_2700G(0x00000210)
#define PWM0DUTY	__REG_2700G(0x00000214)
#define PWM0PER		__REG_2700G(0x00000218)
#define PWM1DIV		__REG_2700G(0x00000220)
#define PWM1DUTY	__REG_2700G(0x00000224)
#define PWM1PER		__REG_2700G(0x00000228)

/* Identification (ID) Registers (0x0000_0300  0x0000_0FFF) */
#define ID		__REG_2700G(0x00000FF0)

/* Local Memory (SDRAM) Interface Registers (0x0000_1000  0x0000_1FFF) */
#define LMRST		__REG_2700G(0x00001000)
#define LMCFG		__REG_2700G(0x00001004)
#define LMPWR		__REG_2700G(0x00001008)
#define LMPWRSTAT	__REG_2700G(0x0000100c)
#define LMCEMR		__REG_2700G(0x00001010)
#define LMTYPE		__REG_2700G(0x00001014)
#define LMTIM		__REG_2700G(0x00001018)
#define LMREFRESH	__REG_2700G(0x0000101c)
#define LMPROTMIN	__REG_2700G(0x00001020)
#define LMPROTMAX	__REG_2700G(0x00001024)
#define LMPROTCFG	__REG_2700G(0x00001028)
#define LMPROTERR	__REG_2700G(0x0000102c)

/* Plane Controller Registers (0x0000_2000  0x0000_2FFF) */
#define GSCTRL		__REG_2700G(0x00002000)
#define VSCTRL		__REG_2700G(0x00002004)
#define GBBASE		__REG_2700G(0x00002020)
#define VBBASE		__REG_2700G(0x00002024)
#define GDRCTRL		__REG_2700G(0x00002040)
#define VCMSK		__REG_2700G(0x00002044)
#define GSCADR		__REG_2700G(0x00002060)
#define VSCADR		__REG_2700G(0x00002064)
#define VUBASE		__REG_2700G(0x00002084)
#define VVBASE		__REG_2700G(0x000020a4)
#define GSADR		__REG_2700G(0x000020c0)
#define VSADR		__REG_2700G(0x000020c4)
#define HCCTRL		__REG_2700G(0x00002100)
#define HCSIZE		__REG_2700G(0x00002110)
#define HCPOS		__REG_2700G(0x00002120)
#define HCBADR		__REG_2700G(0x00002130)
#define HCCKMSK		__REG_2700G(0x00002140)
#define GPLUT		__REG_2700G(0x00002150)
#define DSCTRL		__REG_2700G(0x00002154)
#define DHT01		__REG_2700G(0x00002158)
#define DHT02		__REG_2700G(0x0000215c)
#define DHT03		__REG_2700G(0x00002160)
#define DVT01		__REG_2700G(0x00002164)
#define DVT02		__REG_2700G(0x00002168)
#define DVT03		__REG_2700G(0x0000216c)
#define DBCOL		__REG_2700G(0x00002170)
#define BGCOLOR		__REG_2700G(0x00002174)
#define DINTRS		__REG_2700G(0x00002178)
#define DINTRE		__REG_2700G(0x0000217c)
#define DINTRCNT	__REG_2700G(0x00002180)
#define DSIG		__REG_2700G(0x00002184)
#define DMCTRL		__REG_2700G(0x00002188)
#define CLIPCTRL	__REG_2700G(0x0000218c)
#define SPOCTRL		__REG_2700G(0x00002190)
#define SVCTRL		__REG_2700G(0x00002194)

/* 0x0000_2198 */
/* 0x0000_21A8 VSCOEFF[0:4] Video Scalar Vertical Coefficient [0:4] 4.14.5 */
#define VSCOEFF0	__REG_2700G(0x00002198)
#define VSCOEFF1	__REG_2700G(0x0000219c)
#define VSCOEFF2	__REG_2700G(0x000021a0)
#define VSCOEFF3	__REG_2700G(0x000021a4)
#define VSCOEFF4	__REG_2700G(0x000021a8)

#define SHCTRL		__REG_2700G(0x000021b0)

/* 0x0000_21B4 */
/* 0x0000_21D4 HSCOEFF[0:8] Video Scalar Horizontal Coefficient [0:8] 4.14.7 */
#define HSCOEFF0	__REG_2700G(0x000021b4)
#define HSCOEFF1	__REG_2700G(0x000021b8)
#define HSCOEFF2	__REG_2700G(0x000021bc)
#define HSCOEFF3	__REG_2700G(0x000021c0)
#define HSCOEFF4	__REG_2700G(0x000021c4)
#define HSCOEFF5	__REG_2700G(0x000021c8)
#define HSCOEFF6	__REG_2700G(0x000021cc)
#define HSCOEFF7	__REG_2700G(0x000021d0)
#define HSCOEFF8	__REG_2700G(0x000021d4)

#define SSSIZE		__REG_2700G(0x000021D8)

/* 0x0000_2200 */
/* 0x0000_2240 VIDGAM[0:16] Video Gamma LUT Index [0:16] 4.15.2 */
#define VIDGAM0		__REG_2700G(0x00002200)
#define VIDGAM1		__REG_2700G(0x00002204)
#define VIDGAM2		__REG_2700G(0x00002208)
#define VIDGAM3		__REG_2700G(0x0000220c)
#define VIDGAM4		__REG_2700G(0x00002210)
#define VIDGAM5		__REG_2700G(0x00002214)
#define VIDGAM6		__REG_2700G(0x00002218)
#define VIDGAM7		__REG_2700G(0x0000221c)
#define VIDGAM8		__REG_2700G(0x00002220)
#define VIDGAM9		__REG_2700G(0x00002224)
#define VIDGAM10	__REG_2700G(0x00002228)
#define VIDGAM11	__REG_2700G(0x0000222c)
#define VIDGAM12	__REG_2700G(0x00002230)
#define VIDGAM13	__REG_2700G(0x00002234)
#define VIDGAM14	__REG_2700G(0x00002238)
#define VIDGAM15	__REG_2700G(0x0000223c)
#define VIDGAM16	__REG_2700G(0x00002240)

/* 0x0000_2250 */
/* 0x0000_2290 GFXGAM[0:16] Graphics Gamma LUT Index [0:16] 4.15.3 */
#define GFXGAM0		__REG_2700G(0x00002250)
#define GFXGAM1		__REG_2700G(0x00002254)
#define GFXGAM2		__REG_2700G(0x00002258)
#define GFXGAM3		__REG_2700G(0x0000225c)
#define GFXGAM4		__REG_2700G(0x00002260)
#define GFXGAM5		__REG_2700G(0x00002264)
#define GFXGAM6		__REG_2700G(0x00002268)
#define GFXGAM7		__REG_2700G(0x0000226c)
#define GFXGAM8		__REG_2700G(0x00002270)
#define GFXGAM9		__REG_2700G(0x00002274)
#define GFXGAM10	__REG_2700G(0x00002278)
#define GFXGAM11	__REG_2700G(0x0000227c)
#define GFXGAM12	__REG_2700G(0x00002280)
#define GFXGAM13	__REG_2700G(0x00002284)
#define GFXGAM14	__REG_2700G(0x00002288)
#define GFXGAM15	__REG_2700G(0x0000228c)
#define GFXGAM16	__REG_2700G(0x00002290)

#define DLSTS		__REG_2700G(0x00002300)
#define DLLCTRL		__REG_2700G(0x00002304)
#define DVLNUM		__REG_2700G(0x00002308)
#define DUCTRL		__REG_2700G(0x0000230c)
#define DVECTRL		__REG_2700G(0x00002310)
#define DHDET		__REG_2700G(0x00002314)
#define DVDET		__REG_2700G(0x00002318)
#define DODMSK		__REG_2700G(0x0000231c)
#define CSC01		__REG_2700G(0x00002330)
#define CSC02		__REG_2700G(0x00002334)
#define CSC03		__REG_2700G(0x00002338)
#define CSC04		__REG_2700G(0x0000233c)
#define CSC05		__REG_2700G(0x00002340)

#define FB_MEMORY_START	__REG_2700G(0x00060000)

#endif /* __REGS_2700G_ */
