#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/switch.h>

#include <mach/gpio.h>
#include <mach/msm_i2ckbd.h>
#include <mach/i2ckbd_constants.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include <mach/rpc_pmic.h>
#include <mach/tca6507.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <linux/completion.h>
#include <linux/sched.h>

#define i2ckybd_name "stmpe1601"

static int __devinit qi2ckybd_probe(struct i2c_client *client,
				    const struct i2c_device_id *id);
extern int kpd_bl_set_intensity(int level);
extern int led_234_set_intensity(int L2, int L3, int L4);
extern int msm_set_led_intensity_proc(pm_led_intensity_type led, uint8_t val);
extern void tca6507_led_switch(bool on);
extern void tca6507_center_blink(bool blink);
extern void tca6507_center_attention(bool attention);
//extern void tca6507_center_blink_enable(bool enable);
extern bool gpio_switch_headset_insert(void) ;

static void qi2ckybd_hookswitch(struct work_struct *work);

/*
 * The i2ckybd_record structure consolates all the data/variables
 * specific to managing the single instance of the keyboard.
 */
struct i2ckybd_record {
	struct	i2c_client *mykeyboard;
	struct	input_dev *i2ckbd_idev;
	int	product_info;
	char	physinfo[QKBD_PHYSLEN];
	int	mclrpin;
	int	irqpin;
	int	volup_pin;
	int	voldn_pin;
	int	hall_sensor_pin;
	int	ring_switch_pin;
	int	hook_switch_pin;
	uint8_t kybd_exists;
	uint8_t kybd_connected;
	uint8_t kcnt;
	struct	delayed_work kb_cmdq;
	struct	work_struct qkybd_irqwork;
	struct	work_struct qkybd_volctrl;
	struct	work_struct qkybd_hallsensor;
	struct	work_struct qkybd_ringswitch;
	struct	work_struct qkybd_hookswitch;
	u32 (*xlf)(struct i2ckybd_record *kbdrec, s32 code,
		   s32 *kstate);
	bool	bHookSWIRQEnabled;

	spinlock_t stmpe1601_lock;
	struct early_suspend early_suspend;
	struct wake_lock ringer_sw_suspend_wake_lock;
	struct wake_lock hook_sw_suspend_wake_lock;
	struct wake_lock hook_sw_request_irq_wake_lock;
};

struct input_dev *kpdev		= NULL;
struct i2ckybd_record *rd	= NULL;
static int HWID			= 0;
int gControllerPowerState	= STMPE1601_NORMAL_MODE;
DECLARE_COMPLETION(headset_unplugged_comp);
DECLARE_COMPLETION(headset_unplugged_irq_rel_comp);
DECLARE_COMPLETION(headset_plugged_comp);
static pid_t thread_id;
static pid_t thread_for_request_hooksw_irq;

struct {
	uint8_t keyDown;
	uint8_t keySent;
	struct timespec holdingTime;
} HoldingKeys[5];

struct {
	bool up;
	bool dn;
	bool ring_switch;
} stateBuffer;
#define KBDIRQNO(kbdrec)  (MSM_GPIO_TO_INT(kbdrec->irqpin))

#ifdef KEY_MENU
#undef KEY_MENU
#endif

#ifdef KEY_FN
#undef KEY_FN
#endif

#define KEY_MENU	229
#define KEY_FN		KEY_RIGHTALT
#define KEY_CENTER	232
#define KEY_FOCUS	KEY_F13
#define KEY_RING_SWITCH	KEY_F14
#define KEY_HEADSETHOOK KEY_F15

//KEY_CENTER = ACTION, KEY_BACK = BACK, KEY_HOME = HOME, KEY_MENU = UNLOCK
static uint8_t FIHKeypad_set[QKBD_IN_MXKYEVTS] =
{
	KEY_Q, KEY_O, KEY_J,         KEY_V,     KEY_SPACE,    KEY_FOCUS,
	KEY_W, KEY_P, KEY_K,         KEY_B,     KEY_BACK,   KEY_CAMERA,  
	KEY_E, KEY_A, KEY_L,         KEY_N,     KEY_COMMA,    KEY_SEND,
	KEY_R, KEY_S, KEY_BACKSPACE, KEY_M,     KEY_DOT,      KEY_HOME,
	KEY_T, KEY_D, KEY_CAPSLOCK,  KEY_RIGHT, KEY_UP,     KEY_END,
	KEY_Y, KEY_F, KEY_Z,         KEY_ENTER, KEY_LEFT,     KEY_RESERVED,
	KEY_U, KEY_G, KEY_X,         KEY_FN,    KEY_DOWN,    KEY_RESERVED,
	KEY_I, KEY_H, KEY_C,         KEY_MENU,  KEY_RESERVED, KEY_RESERVED
};

/*
 * For END_KEY
 */
struct input_dev *msm_keypad_get_input_dev(void)
{	
	return kpdev;
}
EXPORT_SYMBOL(msm_keypad_get_input_dev);

struct completion* get_hook_sw_release_completion(void)
{
	return &headset_unplugged_comp;
}
EXPORT_SYMBOL(get_hook_sw_release_completion);

struct completion* get_hook_sw_request_irq_completion(void) 
{
	return &headset_plugged_comp;
}
EXPORT_SYMBOL(get_hook_sw_request_irq_completion);

struct completion* get_hook_sw_release_irq_completion(void) 
{
	return &headset_unplugged_irq_rel_comp;
}
EXPORT_SYMBOL(get_hook_sw_release_irq_completion);

bool qi2ckybd_get_hook_switch_value(void)
{	
	return (bool)gpio_get_value(rd->hook_switch_pin);
}
EXPORT_SYMBOL(qi2ckybd_get_hook_switch_value);

bool qi2ckybd_get_hook_switch_irq_status(void)
{	
	return rd->bHookSWIRQEnabled;
}
EXPORT_SYMBOL(qi2ckybd_get_hook_switch_irq_status);

static irqreturn_t qi2ckybd_irqhandler(int irq, void *dev_id)
{
	struct i2ckybd_record *kbdrec = dev_id;
	bool bHeadsetInserted;

	if (kbdrec->kybd_connected) {
		if (KBDIRQNO(kbdrec) == irq) {
			schedule_work(&kbdrec->qkybd_irqwork);
		} else if ((MSM_GPIO_TO_INT(kbdrec->volup_pin) == irq) || 
			(MSM_GPIO_TO_INT(kbdrec->voldn_pin) == irq)) {
			schedule_work(&kbdrec->qkybd_volctrl);				
		} else if ((MSM_GPIO_TO_INT(kbdrec->hall_sensor_pin) == irq)) {
			schedule_work(&kbdrec->qkybd_hallsensor);			
		} else if ((MSM_GPIO_TO_INT(kbdrec->ring_switch_pin) == irq)) {
			wake_lock(&kbdrec->ringer_sw_suspend_wake_lock);
			schedule_work(&kbdrec->qkybd_ringswitch);		
		} else if ((MSM_GPIO_TO_INT(kbdrec->hook_switch_pin) == irq)) {
			wake_lock(&kbdrec->hook_sw_suspend_wake_lock);

			bHeadsetInserted	= gpio_switch_headset_insert();
			
			if (bHeadsetInserted && &kbdrec->bHookSWIRQEnabled) {
				schedule_work(&kbdrec->qkybd_hookswitch);
			} else {
				dev_dbg(&kbdrec->mykeyboard->dev, "Discard IRQ<%d>[%s][%s]\n", 
						irq,
						bHeadsetInserted ? "HEADSET REMOVED" : "MIC DISABLED",
						&kbdrec->bHookSWIRQEnabled ? "HOOK IRQ ENABLED" : "HOOK IRQ DISABLED"
					);
			}
			
			wake_unlock(&kbdrec->hook_sw_suspend_wake_lock);	
		}
	}
	
	return IRQ_HANDLED;
}

static int qi2ckybd_irqsetup(struct i2ckybd_record *kbdrec)
{
	int rc = request_irq(KBDIRQNO(kbdrec), &qi2ckybd_irqhandler,
			     (IRQF_TRIGGER_FALLING | IRQF_SAMPLE_RANDOM),
			     i2ckybd_name, kbdrec);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", i2ckybd_name, rc);
		rc = -EIO;
	}
	
	/* Vol UP and Vol DOWN keys interrupt */
	rc = request_irq(MSM_GPIO_TO_INT(kbdrec->volup_pin), &qi2ckybd_irqhandler,
			     (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING), 
			     i2ckybd_name, kbdrec);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", i2ckybd_name, rc);
		rc = -EIO;
	}
	
	rc = request_irq(MSM_GPIO_TO_INT(kbdrec->voldn_pin), &qi2ckybd_irqhandler,
			     (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING), 
			     i2ckybd_name, kbdrec);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", i2ckybd_name, rc);
		rc = -EIO;
	}
	
	/* Hall Sensor Interrupt (Slide on/off) */
	rc = request_irq(MSM_GPIO_TO_INT(kbdrec->hall_sensor_pin), &qi2ckybd_irqhandler,
			     (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING), 
			     i2ckybd_name, kbdrec);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", i2ckybd_name, rc);
		rc = -EIO;
	}
	
	/* Ring Switch */
	rc = request_irq(MSM_GPIO_TO_INT(kbdrec->ring_switch_pin), &qi2ckybd_irqhandler,
			     (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING), 
			     i2ckybd_name, kbdrec);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", i2ckybd_name, rc);
		rc = -EIO;
	}
	
	return rc;
}

static int qi2ckybd_release_gpio(struct i2ckybd_record *kbrec)
{
	int kbd_irqpin		= kbrec->irqpin;
	int kbd_mclrpin		= kbrec->mclrpin;
	int kbd_volup_pin	= kbrec->volup_pin;
	int kbd_voldn_pin	= kbrec->voldn_pin;
	int hall_sensor_pin	= kbrec->hall_sensor_pin;
	int ring_switch_pin	= kbrec->ring_switch_pin;
	int hook_switch_pin	= kbrec->hook_switch_pin;

	dev_info(&kbrec->mykeyboard->dev,
		 "releasing keyboard gpio pins %d,%d,%d,%d,%d,%d, %d\n",
		 kbd_irqpin, kbd_mclrpin, kbd_volup_pin, kbd_voldn_pin, hall_sensor_pin, ring_switch_pin, hook_switch_pin);

	gpio_free(kbd_irqpin);
	gpio_free(kbd_mclrpin);
	gpio_free(kbd_volup_pin);
	gpio_free(kbd_voldn_pin);
	gpio_free(hall_sensor_pin);
	gpio_free(ring_switch_pin);
	gpio_free(hook_switch_pin);

	return 0;
}

/*
 * Configure the (2) external gpio pins connected to the keyboard.
 * interrupt(input), reset(output).
 */
static int qi2ckybd_config_gpio(struct i2ckybd_record *kbrec)
{
	struct device *kbdev	= &kbrec->mykeyboard->dev;
	int kbd_irqpin		= kbrec->irqpin;
	int kbd_mclrpin		= kbrec->mclrpin;
	int kbd_volup_pin	= kbrec->volup_pin;
	int kbd_voldn_pin	= kbrec->voldn_pin;
	int hall_sensor_pin	= kbrec->hall_sensor_pin;
	int ring_switch_pin	= kbrec->ring_switch_pin;
	int hook_switch_pin	= kbrec->hook_switch_pin;
	int rc;
	
	gpio_tlmm_config(GPIO_CFG(hook_switch_pin, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);

	rc = gpio_request(kbd_irqpin, "gpio_keybd_irq");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			kbd_irqpin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(kbd_mclrpin, "gpio_keybd_reset");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			kbd_mclrpin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(kbd_volup_pin, "gpio_keybd_volup");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			kbd_volup_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(kbd_voldn_pin, "gpio_keybd_voldn");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			kbd_voldn_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(hall_sensor_pin, "gpio_hall_sensor");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			hall_sensor_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(ring_switch_pin, "gpio_ring_switch");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			ring_switch_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(hook_switch_pin, "gpio_hook_switch");
	if (rc) {
		dev_err(kbdev, "gpio_request failed on pin %d (rc=%d)\n",
			hook_switch_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_input(kbd_irqpin);
	if (rc) {
		dev_err(kbdev, "gpio_direction_input failed on "
		       "pin %d (rc=%d)\n", kbd_irqpin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_output(kbd_mclrpin, 1);
	if (rc) {
		dev_err(kbdev, "gpio_direction_output failed on "
		       "pin %d (rc=%d)\n", kbd_mclrpin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_input(kbd_volup_pin);
	if (rc) {
		dev_err(kbdev, "gpio_direction_input failed on "
		       "pin %d (rc=%d)\n", kbd_volup_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_input(kbd_voldn_pin);
	if (rc) {
		dev_err(kbdev, "gpio_direction_input failed on "
		       "pin %d (rc=%d)\n", kbd_voldn_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_input(hall_sensor_pin);
	if (rc) {
		dev_err(kbdev, "gpio_direction_input failed on "
		       "pin %d (rc=%d)\n", hall_sensor_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_input(ring_switch_pin);
	if (rc) {
		dev_err(kbdev, "gpio_direction_input failed on "
		       "pin %d (rc=%d)\n", ring_switch_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_direction_input(hook_switch_pin);
	if (rc) {
		dev_err(kbdev, "gpio_direction_input failed on "
		       "pin %d (rc=%d)\n", hook_switch_pin, rc);
		goto err_gpioconfig;
	}

	return rc;

err_gpioconfig:
	qi2ckybd_release_gpio(kbrec);
	return rc;
}

/* read keyboard via i2c address + register offset, return # bytes read */
static int kybd_read(struct i2c_client *kbd, uint8_t regaddr,
		     uint8_t *buf, uint32_t rdlen)
{
	u8 ldat = regaddr;
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= kbd->addr,
			.flags	= 0,
			.buf	= (void *)&ldat,
			.len	= 1
		},
		[1] = {
			.addr	= kbd->addr,
			.flags	= I2C_M_RD,
			.buf	= (void *)buf,
			.len	= rdlen
		}
	};

	return (i2c_transfer(kbd->adapter, msgs, 2) < 0) ? -1 : 0;
}

/* Write the specified data to the  control reg */
static int kybd_write(struct i2c_client *kbd, uint8_t regaddr,
		     uint8_t *buf, uint32_t dlen)
{
	s16 i;
	u8 rpd[4];
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= kbd->addr,
			.flags	= 0,
			.buf	= (void *)rpd,
			.len	= (dlen + 1)
		}
	};

	rpd[0] = regaddr;
	for (i = 0; i < dlen; i++)
		rpd[i+1] = buf[i];

	return (i2c_transfer(kbd->adapter, msgs, 1) < 0) ? -1 : 0;
}

/* send the enable cmd and verify proper state in status register */
static int qi2ckybd_enablekybd(struct i2c_client *kbd)
{
	u8 rdat;
	s16 rc = -EIO;

	dev_dbg(&kbd->dev, "Enable keyboard function of STMPE1601\n");

	// Do S/W Reset
	rdat = 0x80;
	rc = kybd_write(kbd, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
	if (rc < 0) {
		dev_err(&kbd->dev, "FAILED: Write CTRL register (SOFT_RESET)(rc=%d)\n", rc);
		return rc;
	}

	//Enable AUTOSLEEP: 1024 ms delay
	rdat = 0x0F;
	rc = kybd_write(kbd, STMPE1601_SYSTEM_CONTROL_REG_2, &rdat, sizeof(rdat));

	//set scan time to 0, and no dedicated key
	rdat = 0xF0;
	rc = kybd_write(kbd, STMPE1601_KPC_CTRL_MSB_REG, &rdat, sizeof(rdat));

	//set to 128 ms of debounce time and start keypad scanning	
	rdat = 0x0F;
	rc = kybd_write(kbd, STMPE1601_KPC_CTRL_LSB_REG, &rdat, sizeof(rdat));
	
	// Enable PWM and KPC
	rdat = 0x0F;
	rc = kybd_write(kbd, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
	if (rc < 0) {
		dev_err(&kbd->dev, "FAILED: Write CTRL register (Enable PWM and KPC)(rc=%d)\n", rc);
		return rc;
	} else {
		rc = kybd_read(kbd, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
		
		if ( 0x0F != (rdat & 0x1F) ) { //SLEEP Mode turn off, enable 
			dev_err(&kbd->dev, "FAILED: Enable PWM and KPC Failed!!(rc=%d) (rdat=0x%02x)\n", rc, rdat);
			return rc;
		} else
			dev_dbg(&kbd->dev, "SUCCESS: Enable PWM and KPC Successfully!!(rc=%d) (rdat=0x%02x)\n", rc, rdat);	
	}

	//change to Keypad function
	rdat = 0x55;
	rc = kybd_write(kbd, STMPE1601_GPAFR_U_MSB_REG, &rdat, sizeof(rdat));
	
	rdat = 0x55;
	rc = kybd_write(kbd, STMPE1601_GPAFR_U_LSB_REG, &rdat, sizeof(rdat));
	
	rdat = 0x55;
	rc = kybd_write(kbd, STMPE1601_GPAFR_L_MSB_REG, &rdat, sizeof(rdat));
	
	rdat = 0x5A; // to set to KEYPAD and PWM
	rc = kybd_write(kbd, STMPE1601_GPAFR_L_LSB_REG, &rdat, sizeof(rdat));
	
	//Enable the COL outputs set in GPIO 2~7
	rdat = 0xFC;
	rc = kybd_write(kbd, STMPE1601_KPC_COL_REG, &rdat, sizeof(rdat));

	//Scan PW: 128x period of internal clock and HIB_WK: enabled
	rdat = 0xE0;
	rc = kybd_write(kbd, STMPE1601_KPC_ROW_MSB_REG, &rdat, sizeof(rdat));

	//Enable the ROW inputs set in GPIO 8~15
	rdat = 0xFF;
	rc = kybd_write(kbd, STMPE1601_KPC_ROW_LSB_REG, &rdat, sizeof(rdat));
      
	//enable interrupt
	dev_dbg(&kbd->dev, "Enable Interrupt\n");
	rdat = 0x00;
	rc = kybd_write(kbd, STMPE1601_ICR_MSB_REG, &rdat, sizeof(rdat));

	//Bit2: Active Falling, Bit1: Edge Trigger, Bit0: Enable Interrupt to Host
	rdat = 0x03;
	rc = kybd_write(kbd, STMPE1601_ICR_LSB_REG, &rdat, sizeof(rdat));
	
	rdat = 0x00;
	rc = kybd_write(kbd, STMPE1601_IER_MSB_REG, &rdat, sizeof(rdat));

	//Bit2: Enable FIFO overflow interrupt, Bit1: Enable Keypad Controller interrupt, Bit0: Enable Wake-up interrupt
	rdat = 0x06;
	rc = kybd_write(kbd, STMPE1601_IER_LSB_REG, &rdat, sizeof(rdat));

	return rc;
}

/* query sw version and product id from keyboard */
static int qi2ckybd_getkbinfo(struct i2c_client *kbd, int *info)
{
	s16 rc;
	u8 rdat[2] = { 0xFF, 0xFF };

	dev_info(&kbd->dev, "ENTRY: read keyboard id info\n");
	rc = kybd_read(kbd, STMPE1601_CHIP_ID_REG, rdat, sizeof(rdat));

	if (rc == 0) {
		*info = rdat[1] << 8 | rdat[0];

		dev_dbg(&kbd->dev, "Received keyboard ID info: "
			"VERSION ID: 0x%x CHIP ID 0x%x\n", rdat[1], rdat[0]);
	}
	
	return rc;
}

/* keyboard has been reset */
static void qi2ckybd_recoverkbd(struct work_struct *work)
{
	int rc;
	struct i2ckybd_record *kbdrec =
		container_of(work, struct i2ckybd_record, kb_cmdq.work);
	struct i2c_client *kbd = kbdrec->mykeyboard;

	dev_info(&kbd->dev, "keyboard recovery requested\n");

	rc = qi2ckybd_enablekybd(kbd);
	if (!rc) {
		qi2ckybd_irqsetup(kbdrec);
		kbdrec->kybd_connected = 1;
	} else
		dev_err(&kbd->dev,
			"recovery failed with (rc=%d)\n", rc);
}

/* use gpio output pin to toggle keyboard external reset pin */
static void qi2ckybd_hwreset(int kbd_mclrpin)
{
	gpio_direction_output(kbd_mclrpin, 0);
	udelay(100);
	gpio_direction_output(kbd_mclrpin, 1);
}

static u32 scancode_to_keycode(struct i2ckybd_record *kbdrec, s32 scancode, s32 *kevent)
{
	struct input_dev *idev	= kbdrec->i2ckbd_idev;
	u32 index		= 0;
	u32 keycode		= 0;
	u8 *tbl			= (u8*)(idev->keycode);
	u8 col;
	u8 row;
	
	*kevent	= (scancode & 0x80) ? QKBD_IN_KEYRELEASE : QKBD_IN_KEYPRESS;
		
	row	= (scancode & 0x78) >> 3;
	col	= (scancode & 0x07) - 2;
	index	= row * 6 + col;
	keycode	= tbl[index];
	
	return keycode;
}
/*
 * handler function called via work queue
 *
 * This function reads from the keyboard via I2C until the
 * keyboard's output queue of scan codes is empty.
 *
 * If running on scan set 1, we support the "RAW" keyboard mode
 * directly. The RAW mode is required if using X11. Currently,
 * RAW mode is not supported for scan set 2.
 */
static void qi2ckybd_fetchkeys(struct work_struct *work)
{
	struct i2ckybd_record *kbdrec	= container_of(work, struct i2ckybd_record, qkybd_irqwork);
	struct i2c_client *kbdcl	= kbdrec->mykeyboard;
	struct input_dev *idev		= kbdrec->i2ckbd_idev;
	s32 kevent			= QKBD_IN_KEYRELEASE;
	u32 xlkcode			= 0;
	s16 rc				= -EIO;
	u8 rdat[NUM_KPC_DATA_BYTE_REG];
	u8 i;

	dev_dbg(&kbdcl->dev, "%s\n", __func__);

	//Make sure that registering to input subsystem was finished.
	if (!kbdrec->kybd_connected) {
		return;
	}

	//Read interrupt status register to get interrupt status.
	kybd_read(kbdcl, STMPE1601_ISR_LSB_REG, &rdat[0], sizeof(rdat[0]));

	//Check wake-up interrupt status
	if (rdat[0] & 0x01) {
		spin_lock(&kbdrec->stmpe1601_lock);
		if (gControllerPowerState != STMPE1601_SLEEP_MODE) {
			gControllerPowerState = STMPE1601_NORMAL_MODE;
			dev_dbg(&kbdcl->dev, "Controller waked up from sleep mode!!\n");
		}
		spin_unlock(&kbdrec->stmpe1601_lock);
	}
	//Check KPC FIFO overflow interrupt status
	if (rdat[0] & 0x04) {
		dev_dbg(&kbdcl->dev, "KPC FIFO overflow!!\n");
	}
	//Check wheather there is any key event or not.
	if (rdat[0] & 0x02) {
		rc = kybd_read(kbdcl, STMPE1601_KPC_DATA_BYTE0_REG, rdat, sizeof(rdat));
		if ( 0 > rc ) {
			dev_err(&kbdcl->dev, "FAILED: Read Keypad Data Byte(rc=%d)\n", rc);
		} else {
			kbdrec->kcnt = 0;

			for (i = 0; i < NUM_KPC_DATA_BYTE_REG; i++) {
				dev_dbg(&kbdcl->dev, "%s: KPC_REG[%d] = 0x%02x\n", __func__, i, rdat[i]);	

				if ((3 == i) && (0xFF != rdat[i])) {
					dev_dbg(&kbdcl->dev, "%s: KPC_DATA_BYTE3 has data!!\n", __func__);	
				} else if ((4 == i) && (0x0F != rdat[i])) {
					dev_dbg(&kbdcl->dev, "%s: KPC_DATA_BYTE4 has data!!\n", __func__);					
				} else if ((4 != i) && (3 != i) && (0xF8 != rdat[i])) {
					xlkcode = kbdrec->xlf(kbdrec, rdat[i], &kevent); //kevent = press?
					if (xlkcode > KEY_RESERVED) {
						dev_dbg(&kbdcl->dev, "xlkcode = %d, row = %d, col = %d\n", xlkcode, (rdat[i] & 0x7F) >> 3, (rdat[i] & 0x07) - 2);
						input_report_key(idev, xlkcode, kevent);
						kbdrec->kcnt++;
					}
				} 
			}
		}
	}

	if (0 < kbdrec->kcnt) {
		input_sync(idev); //input report finished

		kbdrec->kcnt	= 0;
		xlkcode		= 0;
	} else if (0 == kbdrec->kcnt) {
		dev_dbg(&kbdcl->dev, "0 keys processed after interrupt\n");
	}

	//Clear Interrupt Status
	rdat[0] = 0xFF;
	rc = kybd_write(kbdcl, STMPE1601_ISR_LSB_REG, &rdat[0], sizeof(rdat[0]));
	if (0 > rc) {
		dev_err(&kbdcl->dev, "FAIL: Clear STMPE1601 Interrupt Failed\n");		
	}
}

static void qi2ckybd_volkyctrl(struct work_struct *work)
{
	struct i2ckybd_record *kbdrec	= container_of(work, struct i2ckybd_record, qkybd_volctrl);
	struct i2c_client *kbdcl	= kbdrec->mykeyboard;
	struct input_dev *idev		= kbdrec->i2ckbd_idev;
	u32 xlkcode			= 0;
	u32 i;
	bool debounceDelay		= false;
	
	bool volup_val			= (bool)gpio_get_value(kbdrec->volup_pin);
	bool voldn_val			= (bool)gpio_get_value(kbdrec->voldn_pin);
	bool ring_switch_val		= (bool)gpio_get_value(kbdrec->ring_switch_pin);	
	dev_dbg(&kbdcl->dev, "VOL UP <%d> VOL DN <%d> RING SWITCH <%d>\n", volup_val, voldn_val, ring_switch_val);

	disable_irq(MSM_GPIO_TO_INT(kbdrec->volup_pin));
	disable_irq(MSM_GPIO_TO_INT(kbdrec->voldn_pin));

	if (HWID >= CMCS_HW_VER_EVT2) {
		if (volup_val && !voldn_val) {		
			volup_val = (bool)gpio_get_value(kbdrec->volup_pin);
			
			if (volup_val) {
				dev_dbg(&kbdcl->dev, "VOL UP Press >>>>> : xlkcode = %d\n", KEY_VOLUMEUP);
				input_report_key(idev, KEY_VOLUMEUP, QKBD_IN_KEYPRESS); //report VOLUME UP pressing
				HoldingKeys[0].keyDown = 1;
			}
		} else if (!volup_val && !voldn_val) {
			for (i = 0; i < 2; i++) {
				if (1 == HoldingKeys[i].keyDown) {
					HoldingKeys[i].keyDown = 0;
					if (i == 0) {
						xlkcode = KEY_VOLUMEUP;
					} else if (i == 1) {
						xlkcode = KEY_VOLUMEDOWN;
					}
					dev_dbg(&kbdcl->dev, "VOL Key Release: xlkcode = %d\n", xlkcode);

					debounceDelay = true;
					break;	
				}
			}
			
			input_report_key(idev, xlkcode, QKBD_IN_KEYRELEASE); //report VOLUME UP or DOWN releasing	
		} else if (!volup_val && voldn_val) {		
			voldn_val = (bool)gpio_get_value(kbdrec->voldn_pin);
			
			if (voldn_val) {
				dev_dbg(&kbdcl->dev, "VOL DN Press <<<<< : xlkcode = %d\n", KEY_VOLUMEDOWN);
				input_report_key(idev, KEY_VOLUMEDOWN, QKBD_IN_KEYPRESS); //report VOLUME DOWN pressing
				HoldingKeys[1].keyDown = 1;
			}
		}
	} else if (HWID < CMCS_HW_VER_EVT2) {
		if (!volup_val && !voldn_val) { //00
			if (!stateBuffer.up && stateBuffer.dn) { //01->00
				mdelay(100); //Wait for next GPIO state

				volup_val = (bool)gpio_get_value(kbdrec->volup_pin);
				voldn_val = (bool)gpio_get_value(kbdrec->voldn_pin);
				
				if (volup_val && !voldn_val) { //01->00->10 Press VolUP
					input_report_key(idev, KEY_VOLUMEUP, QKBD_IN_KEYPRESS);
					dev_dbg(&kbdcl->dev, "VOL UP Press >>>>>\n");
				} else if (!volup_val && !voldn_val) { //01->00->00 Press VolDN
					mdelay(100);
					ring_switch_val = (bool)gpio_get_value(kbdrec->ring_switch_pin);
					if (!ring_switch_val) {
						input_report_key(idev, KEY_VOLUMEDOWN, QKBD_IN_KEYPRESS);
						dev_dbg(&kbdcl->dev, "VOL DN Press <<<<<\n");
					} else {
						dev_dbg(&kbdcl->dev, "Ringer Swith ON\n");
					}
				}
				
				stateBuffer.up = volup_val;
				stateBuffer.dn = voldn_val;
			} else if (stateBuffer.up && !stateBuffer.dn) { //10->00
				mdelay(100);

				volup_val = (bool)gpio_get_value(kbdrec->volup_pin);
				voldn_val = (bool)gpio_get_value(kbdrec->voldn_pin);
				
				if (!volup_val && voldn_val) { //10->00->01 Release VolUP
					input_report_key(idev, KEY_VOLUMEUP, QKBD_IN_KEYRELEASE);
					dev_dbg(&kbdcl->dev, "VOL UP Release <<<<<\n");
					debounceDelay = true;
				}
				
				stateBuffer.up = volup_val;
				stateBuffer.dn = voldn_val;				
			}
		} else if (!volup_val && voldn_val) { //01
			if (!stateBuffer.up && !stateBuffer.dn) { //00->01 Release VolDN
				if (stateBuffer.ring_switch) {
					dev_dbg(&kbdcl->dev, "Ringer Swith OFF\n");
					stateBuffer.ring_switch = false;					
				} else {
					input_report_key(idev, KEY_VOLUMEDOWN, QKBD_IN_KEYRELEASE);
					dev_dbg(&kbdcl->dev, "VOL DN Release >>>>>\n");
					debounceDelay = true;
				}
				
				stateBuffer.up=volup_val;
				stateBuffer.dn=voldn_val;
			}
		}
	}
	
	input_sync(idev);
	
	if (debounceDelay) {
		mdelay(QKYBD_DEBOUNCE_TIME / 8); //Debounce
	}
	
	enable_irq(MSM_GPIO_TO_INT(kbdrec->volup_pin));
	enable_irq(MSM_GPIO_TO_INT(kbdrec->voldn_pin));
}

static void qi2ckybd_hallsensor(struct work_struct *work)
{
	struct i2ckybd_record *kbdrec	= container_of(work, struct i2ckybd_record, qkybd_hallsensor);
	struct i2c_client *kbdcl	= kbdrec->mykeyboard;
	struct input_dev *idev		= kbdrec->i2ckbd_idev;
	bool hall_sensor_pin_val;

	disable_irq(MSM_GPIO_TO_INT(kbdrec->hall_sensor_pin));

	hall_sensor_pin_val = gpio_get_value(kbdrec->hall_sensor_pin);
	
	dev_dbg(&kbdcl->dev, "Slide on/off: %d\n", hall_sensor_pin_val);
	
	input_report_switch(idev, SW_LID, hall_sensor_pin_val);
	input_sync(idev);
	
	enable_irq(MSM_GPIO_TO_INT(kbdrec->hall_sensor_pin));
}

static struct switch_dev ringer_switch_dev = {
	.name = "ringer_switch",
};

static void qi2ckybd_ringswitch(struct work_struct *work)
{
	struct i2ckybd_record *kbdrec	= container_of(work, struct i2ckybd_record, qkybd_ringswitch);
	struct i2c_client *kbdcl	= kbdrec->mykeyboard;

	bool volup_val			= (bool)gpio_get_value(kbdrec->volup_pin);
	bool voldn_val			= (bool)gpio_get_value(kbdrec->voldn_pin);
	bool ring_switch_val		= (bool)gpio_get_value(kbdrec->ring_switch_pin);
	
	dev_dbg(&kbdcl->dev, "VOL UP <%d> VOL DN <%d> RING SWITCH <%d>\n", volup_val, voldn_val, ring_switch_val);
	
	mdelay(32);
	if (ring_switch_val != (bool)gpio_get_value(kbdrec->ring_switch_pin)) {
		wake_unlock(&kbdrec->ringer_sw_suspend_wake_lock);
		wake_lock_timeout(&kbdrec->ringer_sw_suspend_wake_lock, HZ * 10);
		printk(KERN_INFO "May cause error ringer switch status.\n");
		return;
	}

	switch_set_state(&ringer_switch_dev, ring_switch_val);

	if (ring_switch_val) {
		if (HWID < CMCS_HW_VER_EVT2) { //set by this irq handler, reset by vol key irq handler
			stateBuffer.ring_switch = true;
		}
	} else {
		disable_irq(MSM_GPIO_TO_INT(kbdrec->volup_pin));
		disable_irq(MSM_GPIO_TO_INT(kbdrec->voldn_pin));
		
		mdelay(QKYBD_DEBOUNCE_TIME);
		
		enable_irq(MSM_GPIO_TO_INT(kbdrec->volup_pin));
		enable_irq(MSM_GPIO_TO_INT(kbdrec->voldn_pin));
	}

	wake_unlock(&kbdrec->ringer_sw_suspend_wake_lock);
	wake_lock_timeout(&kbdrec->ringer_sw_suspend_wake_lock, HZ * 10);
}

static void qi2ckybd_hookswitch(struct work_struct *work)
{
	struct i2ckybd_record *kbdrec	= container_of(work, struct i2ckybd_record, qkybd_hookswitch);
	struct i2c_client *kbdcl	= kbdrec->mykeyboard;
	struct input_dev *idev		= kbdrec->i2ckbd_idev;

	bool hook_switch_val		= (bool)gpio_get_value(kbdrec->hook_switch_pin);
	bool debounceDelay		= false;
	bool bHeadsetInserted;
	bool valid_key			= false;
	
	wake_lock(&kbdrec->hook_sw_suspend_wake_lock);

	dev_dbg(&kbdcl->dev, "HOOK SWITCH <%d>\n", hook_switch_val);
	
	disable_irq(MSM_GPIO_TO_INT(kbdrec->hook_switch_pin));

	bHeadsetInserted	= gpio_switch_headset_insert();
	dev_dbg(&kbdcl->dev, "[CP1]HEADSET GPIO<%d>\n", bHeadsetInserted);

	if (bHeadsetInserted && kbdrec->bHookSWIRQEnabled) {

		if (HoldingKeys[2].keyDown == 0 && !hook_switch_val) {
			mdelay(QKYBD_DEBOUNCE_TIME);

			bHeadsetInserted	= gpio_switch_headset_insert();
			dev_dbg(&kbdcl->dev, "[CP2]HEADSET GPIO<%d>\n", bHeadsetInserted);
		}
		
		if (bHeadsetInserted && kbdrec->bHookSWIRQEnabled) {
			valid_key = true;

			if (hook_switch_val) {
				if (HoldingKeys[2].keyDown == 1) {
					input_report_key(idev, KEY_HEADSETHOOK, QKBD_IN_KEYRELEASE);
					input_sync(idev);

					HoldingKeys[2].keyDown = 0;
					debounceDelay = true;
					
					dev_dbg(&kbdcl->dev, "%s: KEY_HEADSETHOOK Release!! \n", __func__);
				}
			} else {
				input_report_key(idev, KEY_HEADSETHOOK, QKBD_IN_KEYPRESS);
				input_sync(idev);
			
				HoldingKeys[2].keyDown = 1;

				dev_dbg(&kbdcl->dev, "%s: KEY_HEADSETHOOK Press!! \n", __func__);
			}
		} else {
			dev_dbg(&kbdcl->dev, "%s: [CP2]KEY_HEADSETHOOK IRQ Discard<bHeadsetInserted %d>!! \n", 
					__func__,
					bHeadsetInserted
				);	
		}
	} else {
		dev_dbg(&kbdcl->dev, "%s: [CP1]KEY_HEADSETHOOK IRQ Discard<%d>!! \n",
				__func__,
				bHeadsetInserted
			);	
	}
	
	enable_irq(MSM_GPIO_TO_INT(kbdrec->hook_switch_pin));
	wake_unlock(&kbdrec->hook_sw_suspend_wake_lock);
	if (valid_key)
		wake_lock_timeout(&kbdrec->hook_sw_suspend_wake_lock, HZ * 10);
}

static int hook_switch_release_thread(void *arg)
{
	printk(KERN_INFO "%s: hook_switch_release_thread running\n", __func__);

	daemonize("hook_thread0");

	while (1) {
		wait_for_completion(&headset_unplugged_comp);
		wake_lock(&rd->hook_sw_suspend_wake_lock);
#ifdef DEBUG
		printk(KERN_INFO "%s: Got complete signal\n", __func__);
#endif

		if (HoldingKeys[2].keyDown == 1) {
			input_report_key(kpdev, KEY_HEADSETHOOK, QKBD_IN_KEYRELEASE);
			input_sync(kpdev);
		}
		wake_unlock(&rd->hook_sw_suspend_wake_lock);
	}
	return 0;
}

static int hook_switch_request_irq_thread (void* arg)
{
	int rc = 0, temp_gpio_val, retry = 0;

	printk(KERN_INFO "%s: hook_switch_request_irq_thread running\n", __func__);
	
	daemonize("hook_thread1");
	
	while (1) {
		wait_for_completion(&headset_plugged_comp);
		wake_lock(&rd->hook_sw_request_irq_wake_lock);
		
		retry = 0;
		do {
			if ((temp_gpio_val = gpio_get_value(rd->hook_switch_pin) == 1) && !rd->bHookSWIRQEnabled) {
				//GPIO 94 is raised and hook switch irq has not been reqeusted yet.
				rc = request_irq(MSM_GPIO_TO_INT(rd->hook_switch_pin), &qi2ckybd_irqhandler,
					     (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING), 
					     i2ckybd_name, rd);
				if (rc < 0) {
					printk(KERN_ERR
						"Could not register for %s interrupt "
						"(rc = %d)\n", i2ckybd_name, rc);
					rc = -EIO;
					
					if (retry > 10) { //Cannot register IRQ.
						break;
					} else {
						retry ++;
						mdelay(10);
					}
				} else {
					//Request IRQ failed, and don't let GPIO 94 be a wakeup source.
					printk(KERN_INFO 
						"%s: Request IRQ %d successfully.\n"
						, __func__, MSM_GPIO_TO_INT(rd->hook_switch_pin));
						
					enable_irq_wake(MSM_GPIO_TO_INT(rd->hook_switch_pin));
					rd->bHookSWIRQEnabled = true;
					
					wake_unlock(&rd->hook_sw_request_irq_wake_lock);	//Hook switch IRQ is enabled and free wake lock.
					wait_for_completion(&headset_unplugged_irq_rel_comp);	//Stop here to waiting for irq releasing signal.
					wake_lock(&rd->hook_sw_request_irq_wake_lock);		//Get wake lock for later procedures and then Hook switch IRQ will be disabled.
					//GPIO 94 is not raised and hook switch irq has not been free yet.
				
					printk(KERN_INFO 
						"%s: Releasing IRQ %d.\n"
						, __func__, MSM_GPIO_TO_INT(rd->hook_switch_pin));
					
					free_irq(MSM_GPIO_TO_INT(rd->hook_switch_pin), rd);
					
					disable_irq_wake(MSM_GPIO_TO_INT(rd->hook_switch_pin));
					
					rd->bHookSWIRQEnabled = false;

					break;
				}
			} else {
				printk(KERN_INFO 
					"%s: Do Nothing. Current GPIO94 = %d retry = %d\n"
					, __func__, temp_gpio_val, retry);
				
				if (retry > 10) { //no hook switch is detected.
					printk(KERN_INFO 
						"%s: This might be a headphones without a hook switch or an error occurred.\n"
						, __func__);
				
					break;
				} else {
					retry ++;
					mdelay(10);
				}
			}
		} while (1);
		wake_unlock(&rd->hook_sw_request_irq_wake_lock);
	}
	
	return 0;
}

static void qi2ckybd_shutdown(struct i2ckybd_record *rd)
{
	if (rd->kybd_connected) {
		dev_info(&rd->mykeyboard->dev, "disconnecting keyboard\n");
		rd->kybd_connected = 0;
		free_irq(KBDIRQNO(rd), rd);
		free_irq(MSM_GPIO_TO_INT(rd->volup_pin), rd);
		free_irq(MSM_GPIO_TO_INT(rd->voldn_pin), rd);
		free_irq(MSM_GPIO_TO_INT(rd->hall_sensor_pin), rd);
		free_irq(MSM_GPIO_TO_INT(rd->ring_switch_pin), rd);
		
		flush_work(&rd->qkybd_irqwork);
		flush_work(&rd->qkybd_volctrl);
		flush_work(&rd->qkybd_hallsensor);
		flush_work(&rd->qkybd_ringswitch);
		flush_work(&rd->qkybd_hookswitch);

		qi2ckybd_hwreset(rd->mclrpin);
	}
}

static int qi2ckybd_eventcb(struct input_dev *dev, unsigned int type,
			    unsigned int code, int value)
{
	int rc = -EPERM;
#if 0
	struct i2ckybd_record *kbdrec = input_get_drvdata(dev);
	struct device *kbdev = &kbdrec->mykeyboard->dev;

	switch (type) {
	case EV_MSC:
		/* raw events are forwarded to keyboard handler */
		break;
	case EV_REP:
		break;
	case EV_LED:
		break;
	default:
		dev_warn(kbdev, "rcv'd unrecognized command (%d)\n", type);
	}
#endif

	return rc;
}

static int qi2ckybd_opencb(struct input_dev *dev)
{
	int rc;
	struct i2ckybd_record *kbdrec	= input_get_drvdata(dev);
	struct i2c_client *kbd		= kbdrec->mykeyboard;

	if (HWID < CMCS_HW_VER_EVT2) {
		stateBuffer.up		= (bool)gpio_get_value(kbdrec->volup_pin);
		stateBuffer.dn		= (bool)gpio_get_value(kbdrec->voldn_pin);
		stateBuffer.ring_switch = (bool)gpio_get_value(kbdrec->ring_switch_pin);
	}

	dev_dbg(&kbd->dev, "ENTRY: input_dev open callback\n");

	qi2ckybd_getkbinfo(kbd, &kbdrec->product_info);
	rc = qi2ckybd_enablekybd(kbd);
	if (!rc) {
		dev->id.version = kbdrec->product_info & 0xFF;
		dev->id.product = (kbdrec->product_info & ~0xFF);
		kbdrec->kybd_connected = 1;
	} else
		rc = -EIO;
	return rc;
}

static void qi2ckybd_closecb(struct input_dev *idev)
{
	struct i2ckybd_record *kbdrec	= input_get_drvdata(idev);
	struct device *dev		= &kbdrec->mykeyboard->dev;

	dev_dbg(dev, "ENTRY: close callback\n");
	qi2ckybd_shutdown(kbdrec);
}

static struct input_dev *create_inputdev_instance(struct i2ckybd_record *kbdrec)
{
	struct device *dev	= &kbdrec->mykeyboard->dev;
	struct input_dev *idev	= 0;
	s16 kidx;

	idev = input_allocate_device();
	if (idev != NULL) {
		idev->name		= i2ckybd_name;
		idev->phys		= kbdrec->physinfo;
		idev->id.bustype	= BUS_I2C;
		idev->id.vendor 	= QCVENDOR_ID;
		idev->id.product	= 1;
		idev->id.version	= 1;
		idev->open		= qi2ckybd_opencb;
		idev->close		= qi2ckybd_closecb;
		idev->event		= qi2ckybd_eventcb;
		idev->keycode		= FIHKeypad_set;
		idev->keycodesize	= sizeof(uint8_t);
		idev->keycodemax	= QKBD_IN_MXKYEVTS;
		idev->evbit[0]		= BIT(EV_KEY) | BIT(EV_SW);
		
		kbdrec->xlf		= scancode_to_keycode;

		/* Set Keypad and Vol Key mapping */
		for (kidx = 0; kidx < QKBD_IN_MXKYEVTS; kidx++)
			__set_bit(FIHKeypad_set[kidx], idev->keybit);

		/* a few more misc keys */
		__set_bit(KEY_POWER, idev->keybit);
		__set_bit(KEY_VOLUMEDOWN, idev->keybit);
		__set_bit(KEY_VOLUMEUP, idev->keybit);
		__set_bit(KEY_HEADSETHOOK, idev->keybit);
		
		//Slide on/off event
		__set_bit(SW_LID, idev->swbit);
		
		HoldingKeys[0].keyDown = 0;
		HoldingKeys[1].keyDown = 0;
		HoldingKeys[2].keyDown = 0;

		input_set_drvdata(idev, kbdrec);
		kpdev = idev;
	} else {
		dev_err(dev,
			"Failed to allocate input device for %s\n",
			i2ckybd_name);
	}
	
	return idev;
}

static void qi2ckybd_connect2inputsys(struct work_struct *work)
{
	struct i2ckybd_record *kbdrec =
		container_of(work, struct i2ckybd_record, kb_cmdq.work);
	struct device *dev = &kbdrec->mykeyboard->dev;
	bool gpio_val;	

	kbdrec->i2ckbd_idev = create_inputdev_instance(kbdrec);
	if (kbdrec->i2ckbd_idev) {
		if (input_register_device(kbdrec->i2ckbd_idev) != 0) {
			dev_err(dev, "Failed to register with"
				" input system\n");
			input_free_device(kbdrec->i2ckbd_idev);
		}
	}
	
	//Initialize screen to PORTRAIT shape. Do not remove this.
	gpio_val = (bool)gpio_get_value(kbdrec->hall_sensor_pin);
	input_report_switch(kbdrec->i2ckbd_idev, SW_LID, gpio_val);
	input_sync(kbdrec->i2ckbd_idev);

	//Initialize ringer switch state
	gpio_val = (bool)gpio_get_value(kbdrec->ring_switch_pin);
	switch_set_state(&ringer_switch_dev, gpio_val);
}

/* utility function used by probe */
static int testfor_keybd(struct i2c_client *new_kbd)
{
	u8 rdat;
	int rc = 0;
	struct i2ckybd_record *rd = i2c_get_clientdata(new_kbd);

	if (!rd->kybd_exists) {
		qi2ckybd_hwreset(rd->mclrpin);
		mdelay(500);
		
		rdat = 0x80;
		rc = kybd_write(new_kbd, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
		if (rc == 0) {
			dev_info(&new_kbd->dev,
				 "Detected %s, attempting to initialize "
				 "keyboard\n", i2ckybd_name);
			snprintf(rd->physinfo, QKBD_PHYSLEN,
				 "%s/%s/event0",
				 new_kbd->adapter->dev.bus_id,
				 new_kbd->dev.bus_id);
			rd->kybd_exists = 1;
			INIT_DELAYED_WORK(&rd->kb_cmdq,
					  qi2ckybd_connect2inputsys);
			schedule_delayed_work(&rd->kb_cmdq,
					      msecs_to_jiffies(600));
		}
	}
	return rc;
}

static ssize_t brightness_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned brightness;
	
	sscanf(buf, "%3d\n", &brightness);
	
	dev_dbg(dev, "%s: %d %d\n", __func__, count, brightness);

	kpd_bl_set_intensity(brightness);

	return count;
}

static ssize_t btn_brightness_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned brightness;
	
	sscanf(buf, "%3d\n", &brightness);
	
	dev_dbg(dev, "%s: %d %d\n", __func__, count, brightness);

	if (HWID < CMCS_HW_VER_EVT2) {
		if (0 < brightness) {
			led_234_set_intensity(0, 0, 0);
		} else {
			led_234_set_intensity(32, 32, 32);
		}
	} else {
		tca6507_led_switch((brightness > 0) ? true : false);
	}
	
	dev_dbg(dev, "%s: %d %d\n", __func__, msm_set_led_intensity_proc(PM_LCD_LED, 4), msm_set_led_intensity_proc(PM_KBD_LED, 4));
	
	return count;
}

static ssize_t blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned enable;
	
	sscanf(buf, "%d\n", &enable);
	
	dev_dbg(dev, "%s: %d\n", __func__, enable);

	if (HWID < CMCS_HW_VER_EVT2) {
	} else {
		switch (enable) {
		case BTN_CENTER_NORMAL:
			//tca6507_center_blink_enable(false);
			tca6507_center_attention(false);
			break;
		case BTN_CENTER_ATTENTION:
			tca6507_center_attention(true);
			break;
		case BTN_CENTER_NOTIFICATION:
			//tca6507_center_blink_enable(true);
			tca6507_center_blink(true);
		}
	}
	
	return count;
}

#ifdef DEBUG
static ssize_t debug_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client =
		container_of(dev, struct i2c_client, dev);
	u8 loc_buf[1024];
	u8 rdat[32], i;
	int bytes_read = 0, i2c_bytes_read;

	i2c_bytes_read = 0x6F - 0x60 + 1;	
	kybd_read(client, 0x60, rdat, i2c_bytes_read);
	for (i = 0; i < i2c_bytes_read; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "0x%02x: 0x%02x\n", i + 0x60, rdat[i]);
	}
	
	i2c_bytes_read = 0x1F - 0x10 + 1;	
	kybd_read(client, 0x10, rdat, i2c_bytes_read);
	for (i = 0; i < i2c_bytes_read; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "0x%02x: 0x%02x\n", i + 0x10, rdat[i]);
	}
	
	i2c_bytes_read = STMPE1601_GPAFR_L_LSB_REG - STMPE1601_GPAFR_U_MSB_REG + 1;
	kybd_read(client, STMPE1601_GPAFR_U_MSB_REG, rdat, i2c_bytes_read);
	for (i = 0; i < i2c_bytes_read; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "0x%02x: 0x%02x\n", i + STMPE1601_GPAFR_U_MSB_REG, rdat[i]);
	}
	
	i2c_bytes_read = 0x03 - 0x02 + 1;
	kybd_read(client, 0x02, rdat, i2c_bytes_read);
	for (i = 0; i < i2c_bytes_read; i++) {
		bytes_read += sprintf(&loc_buf[bytes_read], "0x%02x: 0x%02x\n", i + 0x02, rdat[i]);
	}
	
	bytes_read = sprintf(buf, "%s", loc_buf);
	
	dev_dbg(dev, "%s: %s\n", __func__, buf);
	return bytes_read;
}

static ssize_t debug_info_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client =
		container_of(dev, struct i2c_client, dev);
	struct i2ckybd_record *kybdrd = (struct i2ckybd_record*)i2c_get_clientdata(client);	
	int rc;
	unsigned cmd_number;
	u8 rdat;
	
	sscanf(buf, "%3d\n", &cmd_number);
	
	dev_dbg(dev, "%s: COMMAND: %d\n", __func__, cmd_number);

	switch(cmd_number) {
	case 0:
		rc = kybd_read(client, 0x80, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: CHIP ID=%d\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		
		break;
	case 1:
		rc = kybd_read(client, 0x81, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: VERSION ID=%d\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		
		break;
	case 2:
		rc = kybd_read(client, 0x14, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: ISR_MSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		rc = kybd_read(client, 0x15, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: ISR_LSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		
		break;
	case 3:
		rc = kybd_read(client, 0x02, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: SYS_CTRL=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		rc = kybd_read(client, 0x03, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: SYS_CTRL_2=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		break;
	case 4:
		rc = kybd_read(client, 0x10, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: ICR_MSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		rc = kybd_read(client, 0x11, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: ICR_LSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		break;
	case 5:
		rc = kybd_read(client, 0x12, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: IER_MSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		rc = kybd_read(client, 0x13, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: IER_LSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		break;
	case 6:
		rc = kybd_read(client, 0x63, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: KCR_MSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		rc = kybd_read(client, 0x64, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: KCR_LSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		break;
	case 7:
		rdat = 0x02;
		rc = kybd_write(client, STMPE1601_ISR_LSB_REG, &rdat, sizeof(rdat));
		if ( rc < 0 ) {	
			dev_info(dev, "%s: I2C Write Failed!!\n", __func__);		
		}
		break;
	case 8:
		rc = kybd_read(client, STMPE1601_KPC_DATA_BYTE0_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "Read Keypad Data Byte (=0x%02x)\n", rdat);
		}
			
		rc = kybd_read(client, STMPE1601_KPC_DATA_BYTE1_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "Read Keypad Data Byte (=0x%02x)\n", rdat);
		}
			
		rc = kybd_read(client, STMPE1601_KPC_DATA_BYTE2_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "Read Keypad Data Byte (=0x%02x)\n", rdat);
		}
		break;
	case 9:
		qi2ckybd_hwreset(rd->mclrpin);
		break;
	case 10:
		rdat = 0x0F;
		rc = kybd_write(client, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
		if ( rc < 0 ) {	
			dev_info(dev, "%s: I2C Write Failed!!\n", __func__);		
		}
		break;
	case 11:
		qi2ckybd_enablekybd(client);
		break;
	case 12:
	//change to Keypad function
		rc = kybd_read(client, STMPE1601_GPAFR_U_MSB_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "STMPE1601_GPAFR_U_MSB_REG (=0x%02x)\n", rdat);
		}	
		rc = kybd_read(client, STMPE1601_GPAFR_U_LSB_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "STMPE1601_GPAFR_U_LSB_REG (=0x%02x)\n", rdat);
		}
		rc = kybd_read(client, STMPE1601_GPAFR_L_MSB_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "STMPE1601_GPAFR_L_MSB_REG (=0x%02x)\n", rdat);
		}
		rc = kybd_read(client, STMPE1601_GPAFR_L_LSB_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "STMPE1601_GPAFR_L_LSB_REG (=0x%02x)\n", rdat);
		}
		
		//Enable the COL outputs set in GPIO 2~7
		rc = kybd_read(client, STMPE1601_KPC_COL_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "STMPE1601_KPC_COL_REG (=0x%02x)\n", rdat);
		}
		//Enable the ROW inputs set in GPIO 8~15
		rc = kybd_read(client, STMPE1601_KPC_ROW_LSB_REG, &rdat, 1);
		if ( rc ) {
			dev_info(dev, "STMPE1601_KPC_ROW_REG (=0x%02x)\n", rdat);
		}
		break;
	case 13:
		rdat = 0xFF;
		rc = kybd_write(client, 0x15, &rdat, sizeof(rdat));
		if ( rc < 0 ) {
			dev_info(dev, "CLEAR Keypad Interrupt Failed!!\n");
		}

		break;
	case 14:
		rc = kybd_read(client, 0x18, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: GPIO_ISR_MSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}
		rc = kybd_read(client, 0x19, &rdat, sizeof(rdat));
		if ( rc >= 0 ) {	
			dev_info(dev, "%s: GPIO_ISR_LSB=0x%02x\n", __func__, rdat);		
		} else {
			dev_info(dev, "%s: I2C Read Failed!!\n", __func__);		
		}

		break;
	case 15:
		rc = gpio_get_value(kybdrd->hook_switch_pin);
		dev_info(dev, "%s: HOOK GPIO VALUE<%d>\n", __func__, rc);
		
		break;
	}

	return count;
}
#endif

static ssize_t power_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int bytes_read;
	char str[10];

	switch (gControllerPowerState) {
	case STMPE1601_NORMAL_MODE:
		sprintf(str, "%s\n", "NORMAL");
		break;
	case STMPE1601_SLEEP_MODE:
		sprintf(str, "%s\n", "SLEEP");
		break;
	case STMPE1601_HIBERNATE_MODE:
		sprintf(str, "%s\n", "HIBERNATE");
	}
	bytes_read = sprintf(buf, "%s", str);
	
	dev_dbg(dev, "%s: %s\n", __func__, buf);
	return bytes_read;
}

static ssize_t power_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client =
		container_of(dev, struct i2c_client, dev);
	int rc;
	u8 wdat;
	unsigned power_state;
	
	sscanf(buf, "%d\n", &power_state);
	
	dev_dbg(dev, "%s: %d %d\n", __func__, count, power_state);

	spin_lock(&rd->stmpe1601_lock);
	gControllerPowerState = power_state;
	spin_unlock(&rd->stmpe1601_lock);

	switch (power_state) {
	case STMPE1601_NORMAL_MODE:
		wdat = 0x0F;
		break;
	case STMPE1601_SLEEP_MODE:
		wdat = 0x1F;
		break;
	case STMPE1601_HIBERNATE_MODE:
		wdat = 0x1F;
	}

	rc = kybd_write(client, STMPE1601_SYSTEM_CONTROL_REG, &wdat, sizeof(wdat));
	if ( rc < 0 ) {	
		dev_info(dev, "%s: I2C Write Failed!!\n", __func__);		
	}

	return count;
}

static ssize_t caps_fn_leds_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client =
		container_of(dev, struct i2c_client, dev);
	int rc;
	unsigned leds_state;

	dev_dbg(dev, "%s: %d %d\n", __func__, count, leds_state);
	sscanf(buf, "%d\n", &leds_state);

	switch (leds_state) {
	case PM_LEDS_ALL_OFF: //all off
		msm_set_led_intensity_proc(PM_LCD_LED, 0);
		msm_set_led_intensity_proc(PM_KBD_LED, 0);
		
		break;
	case PM_LEDS_ALL_ON: //all on
		msm_set_led_intensity_proc(PM_LCD_LED, 4);
		msm_set_led_intensity_proc(PM_KBD_LED, 4);
		
		break;
	case PM_LEDS_LCD_ON: //LCD ON
		msm_set_led_intensity_proc(PM_LCD_LED, 4);
	
		break;
	case PM_LEDS_LCD_OFF: //LCD OFF
		msm_set_led_intensity_proc(PM_LCD_LED, 0);

		break;
	case PM_LEDS_KBD_ON: //KBD ON
		msm_set_led_intensity_proc(PM_KBD_LED, 4);

		break;
	case PM_LEDS_KBD_OFF: //KBD OFF
		msm_set_led_intensity_proc(PM_KBD_LED, 0);

		break;
	default:
		dev_err(dev, "%s: Invalid LEDs state %d\n", __func__, leds_state);
	}
	
	return count;
}

DEVICE_ATTR(brightness, 0644, NULL, brightness_store);
DEVICE_ATTR(btn_brightness, 0644, NULL, btn_brightness_store);
#ifdef DEBUG
DEVICE_ATTR(debug_info, 0644, debug_info_show, debug_info_store);
#endif
DEVICE_ATTR(blink, 0644, NULL, blink_store);
DEVICE_ATTR(power_state, 0666, power_state_show, power_state_store);
DEVICE_ATTR(caps_fn_leds, 0644, NULL, caps_fn_leds_store);

static int create_attributes(struct i2c_client *client)
{
	int rc;
	
	rc = device_create_file(&client->dev, &dev_attr_brightness);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"brightness\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}
	
	rc = device_create_file(&client->dev, &dev_attr_btn_brightness);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"btn_brightness\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}

#ifdef DEBUG
	rc = device_create_file(&client->dev, &dev_attr_debug_info);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"debug_info\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}
#endif
	
	rc = device_create_file(&client->dev, &dev_attr_blink);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"blink\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}
	
	rc = device_create_file(&client->dev, &dev_attr_power_state);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"power_state\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}

	rc = device_create_file(&client->dev, &dev_attr_caps_fn_leds);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create keyboard attribute \"caps_fn_leds\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}
	
	return rc;	
}

static int remove_attributes(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_brightness);
	device_remove_file(&client->dev, &dev_attr_btn_brightness);
#ifdef DEBUG
	device_remove_file(&client->dev, &dev_attr_debug_info);
#endif
	device_remove_file(&client->dev, &dev_attr_blink);
	device_remove_file(&client->dev, &dev_attr_power_state);	
	device_remove_file(&client->dev, &dev_attr_caps_fn_leds);

	return 0;
}

static int __devexit qi2ckybd_remove(struct i2c_client *kbd)
{
	struct i2ckybd_record *rd = i2c_get_clientdata(kbd);

	dev_info(&kbd->dev, "removing keyboard driver\n");
	device_init_wakeup(&kbd->dev, 0);

	if (rd->i2ckbd_idev) {
		dev_dbg(&kbd->dev, "deregister from input system\n");
		input_unregister_device(rd->i2ckbd_idev);
		rd->i2ckbd_idev = 0;
	}
	qi2ckybd_shutdown(rd);
	qi2ckybd_release_gpio(rd);

	remove_attributes(kbd);

	switch_dev_unregister(&ringer_switch_dev);
	
	unregister_early_suspend(&rd->early_suspend);
	
	wake_lock_destroy(&rd->ringer_sw_suspend_wake_lock);
	wake_lock_destroy(&rd->hook_sw_suspend_wake_lock);
	wake_lock_destroy(&rd->hook_sw_request_irq_wake_lock);
	
	//kill_proc_info(1, SIGKILL, thread_id);

	kfree(rd);

	return 0;
}

#ifdef CONFIG_PM
static int qi2ckybd_suspend(struct i2c_client *kbd, pm_message_t mesg)
{
	struct i2ckybd_record *kbdrec = i2c_get_clientdata(kbd);
	int rc;
	u8 rdat;
	
	dev_dbg(&kbd->dev, "%s: Enter SUSPEND Mode.\n", __func__);
	
	//DIS_32kHZ set to 1 and the controller enters hibernate mode.
	spin_lock(&kbdrec->stmpe1601_lock);
	if (gControllerPowerState == STMPE1601_SLEEP_MODE) {
		rdat = 0x1F; 
	} else {
		rdat = 0x1F;
		gControllerPowerState = STMPE1601_HIBERNATE_MODE;
	}	
	spin_unlock(&kbdrec->stmpe1601_lock);
	
	rc = kybd_write(kbd, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
	if (rc < 0) {
		dev_err(&kbd->dev, "FAILED: Write CTRL register (SUSPEND)(rc=%d)\n", rc);
		return rc;
	}
	
	//Set this if you want use IRQs to wake the system up
	if (device_may_wakeup(&kbd->dev)) {
		enable_irq_wake(MSM_GPIO_TO_INT(kbdrec->hall_sensor_pin));
		enable_irq_wake(MSM_GPIO_TO_INT(kbdrec->ring_switch_pin));
		enable_irq_wake(KBDIRQNO(kbdrec));
	}

	return 0;
}

static int qi2ckybd_resume(struct i2c_client *kbd)
{
	struct i2ckybd_record *kbdrec = i2c_get_clientdata(kbd);
	int rc;
	u8 rdat;

	dev_dbg(&kbd->dev, "%s: Leave SUSPEND Mode.\n", __func__);

	spin_lock(&kbdrec->stmpe1601_lock);
	if (gControllerPowerState == STMPE1601_SLEEP_MODE) {
		rdat = 0x1F;
	} else {
		rdat = 0x0F;
		gControllerPowerState = STMPE1601_NORMAL_MODE;
	}
	spin_unlock(&kbdrec->stmpe1601_lock);

	rc = kybd_write(kbd, STMPE1601_SYSTEM_CONTROL_REG, &rdat, sizeof(rdat));
	if (rc < 0) {
		dev_err(&kbd->dev, "FAILED: Write CTRL register (RESUME)(rc=%d)\n", rc);
		return rc;
	}

	if (device_may_wakeup(&kbd->dev)) {
		disable_irq_wake(MSM_GPIO_TO_INT(kbdrec->hall_sensor_pin));
		disable_irq_wake(MSM_GPIO_TO_INT(kbdrec->ring_switch_pin));
		disable_irq_wake(KBDIRQNO(kbdrec));
	}

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void qi2ckybd_early_suspend(struct early_suspend *h)
{
	struct i2ckybd_record *kybd;
	kybd = container_of(h, struct i2ckybd_record, early_suspend);
	qi2ckybd_suspend(kybd->mykeyboard, PMSG_SUSPEND);
	dev_dbg(&kybd->mykeyboard->dev, "%s: EARLY_SUSPEND Starting\n", __func__);
}

static void qi2ckybd_late_resume(struct early_suspend *h)
{
	struct i2ckybd_record *kybd;
	kybd = container_of(h, struct i2ckybd_record, early_suspend);
	qi2ckybd_resume(kybd->mykeyboard);
	dev_dbg(&kybd->mykeyboard->dev, "%s: LATE RESUME Finished\n", __func__);
}
#endif

#else
#define qi2ckybd_suspend NULL
#define qi2ckybd_resume  NULL
#ifdef CONFIG_HAS_EARLYSUSPEND
#define qi2ckybd_early_suspend NULL
#define qi2ckybd_late_resume NULL
#endif
#endif

static const struct i2c_device_id i2ckybd_idtable[] = {
       { i2ckybd_name, 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, i2ckybd_idtable);

static struct i2c_driver i2ckbd_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = i2ckybd_name,
	},
	.probe	  = qi2ckybd_probe,
	.remove	  = __devexit_p(qi2ckybd_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend  = qi2ckybd_suspend,
	.resume   = qi2ckybd_resume,
#endif
	.id_table = i2ckybd_idtable,
};

static int __devinit qi2ckybd_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct msm_i2ckbd_platform_data *setup_data;
	int rc					= -ENOMEM;

	if (!client->dev.platform_data) {
		dev_err(&client->dev,
			"keyboard platform device data is required\n");
		return -ENODEV;
	}

	rd = kzalloc(sizeof(struct i2ckybd_record), GFP_KERNEL);
	if (!rd) {
		dev_err(&client->dev, "i2ckybd_record memory allocation failed!!\n");
		return rc;
	}

	client->driver		= &i2ckbd_driver;
	i2c_set_clientdata(client, rd);
	rd->mykeyboard		= client;
	setup_data 		= client->dev.platform_data;
	rd->mclrpin		= setup_data->gpioreset;
	rd->irqpin		= setup_data->gpioirq;
	rd->volup_pin		= setup_data->gpio_vol_up;
	rd->voldn_pin		= setup_data->gpio_vol_dn;
	rd->hall_sensor_pin	= setup_data->gpio_hall_sensor;
	rd->ring_switch_pin	= setup_data->gpio_ring_switch;
	rd->hook_switch_pin	= setup_data->gpio_hook_switch;
	rd->bHookSWIRQEnabled	= false;
	spin_lock_init(&rd->stmpe1601_lock);

#ifdef CONFIG_HAS_EARLYSUSPEND
	rd->early_suspend.level		= EARLY_SUSPEND_LEVEL_DISABLE_QWERTY_KEYPAD;
	rd->early_suspend.suspend	= qi2ckybd_early_suspend;
	rd->early_suspend.resume	= qi2ckybd_late_resume;
	register_early_suspend(&rd->early_suspend);
#endif

	wake_lock_init(&rd->ringer_sw_suspend_wake_lock, WAKE_LOCK_SUSPEND, "ringer_sw_suspend_work");
	wake_lock_init(&rd->hook_sw_suspend_wake_lock, WAKE_LOCK_SUSPEND, "hook_sw_suspend_work");
	wake_lock_init(&rd->hook_sw_request_irq_wake_lock, WAKE_LOCK_SUSPEND, "hook_sw_request_irq_work");

	//Initialize GPIO
	rc = qi2ckybd_config_gpio(rd);
	if (rc)
		goto failexit1;

	//Initialize IRQ
	INIT_WORK(&rd->qkybd_irqwork, qi2ckybd_fetchkeys);
	INIT_WORK(&rd->qkybd_volctrl, qi2ckybd_volkyctrl);
	INIT_WORK(&rd->qkybd_hallsensor, qi2ckybd_hallsensor);
	INIT_WORK(&rd->qkybd_ringswitch, qi2ckybd_ringswitch);
	INIT_WORK(&rd->qkybd_hookswitch, qi2ckybd_hookswitch);
	rc = qi2ckybd_irqsetup(rd);
	if (rc)
		goto failexit2;

	rc = testfor_keybd(client);
	if (!rc)
		device_init_wakeup(&client->dev, 1);
	else {
		mdelay(1000);
		
		rc = testfor_keybd(client);
		
		if (rc) {
			dev_dbg(&client->dev, "%s: Initialize keyboard failed!! <%d>", __func__, rc);		
			goto failexit2;
		}
	}
	
	rc = create_attributes(client);
	if (rc < 0) {
		dev_err(&client->dev, "%s: create attributes failed!! <%d>", __func__, rc);

		return rc;
	}
	
	rc = switch_dev_register(&ringer_switch_dev);
	if (rc < 0) {
		dev_err(&client->dev, "%s: register ring switch device failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	HWID = FIH_READ_HWID_FROM_SMEM();
	dev_dbg(&client->dev, "HWID = %d\n", HWID);
	
	thread_id = kernel_thread(hook_switch_release_thread, NULL, CLONE_FS | CLONE_FILES);
	thread_for_request_hooksw_irq = kernel_thread(hook_switch_request_irq_thread, NULL, CLONE_FS | CLONE_FILES);

	return 0;

 failexit2:
	free_irq(KBDIRQNO(rd), rd);
	free_irq(MSM_GPIO_TO_INT(rd->volup_pin), rd);
	free_irq(MSM_GPIO_TO_INT(rd->voldn_pin), rd);
	free_irq(MSM_GPIO_TO_INT(rd->hall_sensor_pin), rd);
	free_irq(MSM_GPIO_TO_INT(rd->ring_switch_pin), rd);
 failexit1:
	qi2ckybd_release_gpio(rd);
	kfree(rd);
	return rc;
}

static int __init qi2ckybd_init(void)
{
	return i2c_add_driver(&i2ckbd_driver);
}

static void __exit qi2ckybd_exit(void)
{
	i2c_del_driver(&i2ckbd_driver);
}

module_init(qi2ckybd_init);
module_exit(qi2ckybd_exit);
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("I2C QWERTY keyboard driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:stmpe1601");

