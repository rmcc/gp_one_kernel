#ifndef _MAX7302_H_
#define _MAX7302_H_

//port register address on chip
#define PORT_P0_REG		0x00 // Teng Rui add for MAX8831 enable address
#define PORT_P1_REG		0x01
#define PORT_P2_REG		0x02
#define PORT_P3_REG		0x03
#define PORT_P4_REG		0x04
#define PORT_P5_REG		0x05
#define PORT_P6_REG		0x06
#define PORT_P7_REG		0x07
#define PORT_P8_REG		0x08
#define PORT_P9_REG		0x09
#define KPD_BL_PORT		PORT_P2_REG		//keypad bl register
#define LCD_BL_PORT		PORT_P3_REG		//lcd bl register
#define LED1_R_PORT		PORT_P4_REG		//led2001 R
#define LED1_G_PORT		PORT_P5_REG		//led2001 G
#define LED1_B_PORT		PORT_P6_REG		//led2001 B
#define LED2_PORT		PORT_P7_REG		//led2004
#define LED3_PORT		PORT_P8_REG		//led2002
#define LED4_PORT		PORT_P9_REG		//led2005
#define PORT_P11_REG	0x0B // Teng Rui add for MAX8831 level control address
#define PORT_P12_REG	0x0C // Teng Rui add for MAX8831 level control address

//configuration register address
#define CONF_26_REG		0x26
#define CONF_27_REG		0x27

//bits of configuration regiseter 26 
#define INT_STAT_FLG_MASK	(1<<7)	//read only, 0:an interrupt, 1:no interrupt
#define TRAN_FLG_MASK		(1<<6)	//read only, 0:an transition, 1:no transition
#define BLINK_PRESCALOR(x)	((((x) < 0x08) ? (x) : 0)<<2) //Blink timer bits (bit4~2)
#define RST_TIMER(x)		((x)<<1)	// 0/1: not/to reset counter
#define RST_POR(x)			(x)		// 0/1: not/to reset register

#define CONF_26_DATA(bit4_2, bit1, bit0)   \
  						(	BLINK_PRESCALOR(bit4_2) | \
  							RST_TIMER(bit1) | \
							RST_POR(bit0) \
						)

//bits of configuration register 27
#define BUS_TIMEOUT(x)		((x)<<7)	// 0/1: enable/disable bus timeout feature
#define P3_OSCOUT(x)		((x)<<3)	// 0/1: set as output oscillator/GPIO
#define P2_OSCIN(x)			((x)<<2)	// 0/1: set as oscillator input/GPIO
#define P1_INT(x)			((x)<<1)	// 0/1: set as interrupt output/GPIO
#define INPUT_TRAN(x)		(x)			// 0: set to 0 on power-up to detect transition on inputs

#define CONF_27_DATA(bit7, bit3, bit2, bit1, bit0) \
  						(	BUS_TIMEOUT(bit7) | \
							P3_OSCOUT(bit3) | \
							P2_OSCIN(bit2) | \
							P1_INT(bit1) | \
							INPUT_TRAN(bit0) \
						)

//PWM/Blink settings value for output port
#define PWM_STATIC_LOW		0x00
#define PWM_LEVEL(x)		(x)			//Duty cycle: x/32 (1~31)
#define PWM_STATIC_HIGH		0x70
#define BLK_STATIC_LOW		(PWM_STATIC_LOW+0x20)
#define BLK_LEVEL(x)		((x)+0x20)	//Duty cycle: x/16 (1~15)
#define BLK_STATIC_HIGH		PWM_STATIC_HIGH

//Blink period
#define BLK_PERIOD(x)		(x)			//Period time: 0.125*(1<<x) sec (x:0~7)

//IO conrol definition.
#define MAX7302_MAGIC 'm'

#define MAX7302_RST			_IO(MAX7302_MAGIC, 0)
#define MAX7302_G_LCD		_IOR(MAX7302_MAGIC, 1, int)
#define MAX7302_S_LCD		_IOW(MAX7302_MAGIC, 2, int)
#define MAX7302_G_KPD		_IOR(MAX7302_MAGIC, 3, int)
#define MAX7302_S_KPD		_IOW(MAX7302_MAGIC, 4, int)
#define MAX7302_G_LED_RGB	_IOR(MAX7302_MAGIC, 5, int)
#define MAX7302_S_LED_RGB	_IOW(MAX7302_MAGIC, 6, int)
#define MAX7302_G_LED_234	_IOR(MAX7302_MAGIC, 7, int)
#define MAX7302_S_LED_234	_IOW(MAX7302_MAGIC, 8, int)

#define MAX7302_MAXNR 10



struct max7302_i2c_data
{
  	char reg;
	char data;
};

struct max7302_chip_data
{
  	char reg26;
	char reg27;
  	char reg[10];	//record pwm/blk level and config 26&27 data. reg[0] reserved.
	char blk_prd;	//record blk period
	//char blk_mode;
};

int max7302_port_set_level(int port, int level);
int lcd_bl_set_intensity(int level);
#endif
