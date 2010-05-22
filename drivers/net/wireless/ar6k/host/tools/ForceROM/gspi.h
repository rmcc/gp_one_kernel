#ifndef GSPI_H
#define GSPI_H
#include <linux/types.h>
#ifdef OMAP5912_SPI
#define ZDY_PKG
#define OMAP_SPI_REV_ADDR       0xfffb0c00

#define OMAP_SPI_SCR_ADDR       0xfffb0c10
#define AUTOIDLE                0x00000001
#define SOFTRESET               0x00000002
#define EN_WAKEUP               0x00000004
#define IDLEMODE(v)             ((v) << 3)  /* [4:3] */
#define NO_IDLE                 0x1
#define SMART_IDLE              0x2

#define OMAP_SPI_SSR_ADDR       0xfffb0c14
#define RESETDONE               0x00000001

#define OMAP_SPI_ISR_ADDR       0xfffb0c18
#define OMAP_SPI_ISR_RE         0x00000001
#define OMAP_SPI_ISR_WE         0x00000002
#define OMAP_SPI_ISR_RX_OVERFLOW             0x00000004
#define OMAP_SPI_ISR_TX_OVERFLOW             0x00000008
#define OMAP_SPI_ISR_WAKEUP                  0x00000010

#define OMAP_SPI_IER_ADDR       0xfffb0c1c
#define RDINTR_MSK              0x00000001
#define WRINTR_MSK              0x00000002
#define RXOFINTR_MSK            0x00000004
#define TXOFINTR_MSK            0x00000008
#define WKPINTR_MSK             0x00000010

#define OMAP_SPI_SET1_ADDR      0xfffb0c24
#define DMA_EN                  0x00000020
#define PTV(v)                  ((v) << 1)  /* [4:1] */
#define EN_CLK                  0x00000001

#define OMAP_SPI_SET2_ADDR      0xfffb0c28
#define MASTER_MODE             0x00008000
#define CP(v)                   ((v) << 10) /* [10:14] */
#define CE(v)                   ((v) << 5)  /* [5:9] */
#define CI(v)                   (v)
#define DEV0                    0x1
#define DEV1                    0x2
#define DEV2                    0x4
#define DEV3                    0x8
#define DEV4                    0x10

#define OMAP_SPI_CTRL_ADDR      0xfffb0c2c
#define AD(v)                   ((v) << 7)   /* [9:7] */
#define EDEV0                   0x000
#define EDEV1                   0x001
#define EDEV2                   0x010
#define EDEV3                   0x011
#define EDEV4                   0x100
#define NB(v)                   ((v) << 2)  /* [6:2] */
#define NB8 			     0x7
#define NB16                    0xf
#define NB32                    0x1f
#define WR                      0x00000002
#define RD                      0x00000001
//#define WR16                    0x000000be
#define WR16                    0x000000bd
#define RD16                    0x000000bd
#define CLR_RW16                0x000000bc

#define OMAP_SPI_DSR_ADDR       0xfffb0c30
#define OMAP_SPI_DSR_TX_EMPTY                0x00000002
#define OMAP_SPI_DSR_RX_FULL                0x00000001

#define OMAP_SPI_TX_ADDR        0xfffb0c34

#define OMAP_SPI_RX_ADDR        0xfffb0c38

#define OMAP_SPI_TEST_ADDR      0xfffb0c3c

#ifdef ZDY_PKG
#define  REG7_MASK              0x00d80000 /* For N15, [23:21] = 110 */

/* R15: reg8[5:3] = 110
 * R14: reg8[2:0] = 110
 * R17: reg8[11:9] = 110
 */
#define REG8_MASK               0x00000c36

#endif
//#define OMAP_SPI_WR32(addr, val) __raw_writel(val, addr)
/*#define OMAP_SPI_RD32(addr, val) do {                             \
                                          val = __raw_readl(addr);  \
                                      } while(0)
*/
#define OMAP_SPI_WR32(addr,val) (*((volatile u32*)addr)=val)
#define OMAP_SPI_RD32(addr,val) do{                                     \
                                          val=*((volatile u32*)addr);\
                                  }while(0)
const u32 WORD_LENGTH_CONF[3]={NB(NB8),NB(NB16),NB(NB32)};
#endif

#ifdef OMAP2420_SPI
#define OMAP_MCSPI1_BASE_ADDRESS 0x48098000
#define OMAP_MCSPI2_BASE_ADDRESS 0x4809a000
#define OMAP_PRCM_CORE_ENABLE_ADDRESS 0x48008200 
#define OMAP_CONTROL_PADCONF_BASE_ADDRESS 0x48000000
#define OMAP_CONTROL_PADCONF_SIZE 0x400

#define OMAP_MCSPI_SIZE		0x100
#define OMAP_MCSPI_REVISION 		0x0
#define OMAP_MCSPI_SYSCONFIG 		0x10
#define OMAP_MCSPI_SYSSTATUS 		0x14
#define OMAP_MCSPI_IRQSTATUS 		0x18
#define OMAP_MCSPI_IRQENABLE 		0x1c
#define OMAP_MCSPI_WAKEUPENABLE 	0x20
#define OMAP_MCSPI_SYST			0x24
#define OMAP_MCSPI_MODULCTRL 		0x28
#define OMAP_MCSPI_CHCONF(chan) (0x2c+0x14*(chan))	//chan stand for channel num,from 0->3(for MCSPI1),0->1(for MCSPI2)
#define OMAP_MCSPI_CHSTAT(chan) (0x30+0x14*(chan))
#define OMAP_MCSPI_CHCTRL(chan) (0x34+0x14*(chan))
#define OMAP_MCSPI_TX(chan)	 (0x38+0x14*(chan))
#define OMAP_MCSPI_RX(chan)	 (0x3c+0x14*(chan))

#define MCSPI_SYSCONFIG_CLOCKACTIVITY (3<<8)
#define MCSPI_SYSCONFIG_SIDLEMODE		(3<<3)
#define MCSPI_SYSCONFIG_ENAWAKEUP	(1<<2)
#define MCSPI_SYSCONFIG_SOFTRESET		(1<<1)
#define MCSPI_SYSCONFIG_AUTOIDLE		(1<<0)


#define MCSPI_SYSSTATUS_RESETDONE	(1<<0)

#define MCSPI_IRQSTATUS_WKS			(1<<16)
#define MCSPI_IRQSTATUS_RX_FULL(chan)			(2+((chan)<<2))
#define MCSPI_IRQSTATUS_TX_UNDERFLOW(chan)		(1+((chan)<<2))
#define MCSPI_IRQSTATUS_TX_EMPTY(chan)			(((chan)<<2))

#define MCSPI_IRQENABLE_RX_FULL(chan)			(2+((chan)<<2))
#define MCSPI_IRQENABLE_TX_UNDERFLOW(chan)		(1+((chan)<<2))
#define MCSPI_IRQENABLE_TX_EMPTY(chan)			(((chan)<<2))

#define MCSPI_WAKEUPENABLE_WKEN				(1<<0)

#define MCSPI_SYST_SSB					(1<<11)
#define MCSPI_SYST_SPIENDIR			(1<<10)
#define MCSPI_SYST_SPIDATDIR1			(1<<9)	//direstion of SPI.SOMI
#define MCSPI_SYST_SPIDATDIR0			(1<<8)	//direction of SPI.SIMO
#define MCSPI_SYST_WAKD				(1<<7)
#define MCSPI_SYST_SPICLK				(1<<6)
#define MCSPI_SYST_SPIDAT_1			(1<<5)	//SIMO?
#define MCSPI_SYST_SPIDAT_0			(1<<4)	//SOMI?
#define MCSPI_SYST_SPIEN(chan)		((1<<(chan)))

#define MCSPI_MODULCTRL_SYSTEM_TEST	(1<<3)
#define MCSPI_MODULCTRL_MS			(1<<2)
#define MCSPI_MODULCTRL_SINGLE		(1<<0)

#define MCSPI_CHCONF_SPIENSLV			(3<<21)
#define MCSPI_CHCONF_FORCE			(1<<20)
#define MCSPI_CHCONF_TURBO			(1<<19)
#define MCSPI_CHCONF_IS				(1<<18)
#define MCSPI_CHCONF_DPE1				(1<<17)
#define MCSPI_CHCONF_DPE0				(1<<16)
#define MCSPI_CHCONF_DMAR				(1<<15)
#define MCSPI_CHCONF_DMAW				(1<<14)
#define MCSPI_CHCONF_TRM				(3<<12)
#define MCSPI_CHCONF_WL(x)				(((x-1)<<7))

#define MCSPI_CHCONF_EPOL				(1<<6)
#define MCSPI_CHCONF_CLKD(x)			(((x)<<2)) //clock divider is 2^x
#define MCSPI_CHCONF_POL				(1<<1)
#define MCSPI_CHCONF_PHA				(1<<0)
#define MCSPI_CHSTAT_EOT				(1<<2)
#define MCSPI_CHSTAT_TXS				(1<<1)
#define MCSPI_CHSTAT_RXS				(1<<0)

#define MCSPI_CHCTRL_EN				(1<<0)
#define OMAP_SPI1_WR32(offset,val) writel(val,(MCSPI1_VIRTUAL_BASE_ADDRESS+offset))
#define OMAP_SPI1_RD32(offset,val) do{                                     \
                                          val=readl(MCSPI1_VIRTUAL_BASE_ADDRESS+offset);\
                                  }while(0)
#define OMAP_SPI2_WR32(offset,val) writel(val,(MCSPI2_VIRTUAL_BASE_ADDRESS+offset))
#define OMAP_SPI2_RD32(offset,val) do{                                     \
                                          val=readl(MCSPI2_VIRTUAL_BASE_ADDRESS+offset);\
                                  }while(0)
#define OMAP_SPI_WR32(mod,offset,val) do{	\
						if(mod==1){			\
							OMAP_SPI1_WR32(offset,val);	\
						}						\
						else{	\
							OMAP_SPI2_WR32(offset,val);	\
						}		\
					}while(0)
#define OMAP_SPI_RD32(mod,offset,val) do{	\
						if(mod==1){			\
							OMAP_SPI1_RD32(offset,val);	\
						}								\
						else{	\
							OMAP_SPI2_RD32(offset,val);	\
						}					\
					}while(0)							
const u32 WORD_LENGTH_CONF[3]={MCSPI_CHCONF_WL(8),MCSPI_CHCONF_WL(16),MCSPI_CHCONF_WL(32)};
#endif
#define  PIO_8 0
#define  PIO_16 1
#define  PIO_32 2
extern int ar_spim_init(void);
extern int ar_spim_start(void);
extern int ar_spim_txw(u32 mode,u32 data);
extern int ar_spim_rxw(u32 mode,u32 *data);

extern int ar_spim_WriteReg(u32 mode,u32 address,u32 data);
extern void ar_spim_stop(void);
extern void ar_spim_reset(void);
#endif
