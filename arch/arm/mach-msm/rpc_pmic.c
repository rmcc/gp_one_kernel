/* arc/arm/mach-msm/rpc_pmic.c
 * Author:	AudiPCHuang
 * Date:	2009.05.14
 * Description: A driver for control PMIC-related RPC calls.
 * Mail:	AudiPCHuang@fihtdc.com
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <mach/rpc_pmic.h>

MODULE_DESCRIPTION("PMIC RPC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

struct pmic_rpc_driver_data {
	struct platform_device *pmic_dev;
};

struct msm_pmic_rpc_ids {
	unsigned long	vib_api_prog;
	unsigned long	vib_api_vers;
	unsigned long	vib_api_vers_comp;
	unsigned	vib_mot_set_volt_proc;
	unsigned	vib_mot_set_mode_proc;
	unsigned	vib_mot_set_polarity_proc;
	
	unsigned	led_set_intensity_proc;
	
	unsigned	mic_en_proc;
};

struct msm_pmic_rpc_ids pmic_rpc_ids;
struct pmic_rpc_driver_data pmic_rpc_drv_data;
static struct msm_rpc_endpoint *pmic_rpc_ep;

int msm_pmic_init_rpc_ids(void)
{
	pmic_rpc_ids.vib_api_prog		= 0x30000061;
	pmic_rpc_ids.vib_api_vers		= 0x00010001;
	pmic_rpc_ids.vib_mot_set_volt_proc	= 21;
	pmic_rpc_ids.vib_mot_set_mode_proc	= 22;
	pmic_rpc_ids.vib_mot_set_polarity_proc	= 23;

	pmic_rpc_ids.led_set_intensity_proc	= 15;
	
	pmic_rpc_ids.mic_en_proc		= 27;
	
	return 0;
}

int msm_pmic_rpc_connect(void)
{
	if (pmic_rpc_ep) {
		printk(KERN_INFO "%s: pmic_rpc_ep already connected\n", __func__);
		return 0;
	}

	/* Initialize rpc ids */
	if (msm_pmic_init_rpc_ids()) {
		printk(KERN_ERR "%s: vibrator rpc ids initialization failed\n"
			, __func__);
		return -ENODATA;
	}

	pmic_rpc_ep = msm_rpc_connect(pmic_rpc_ids.vib_api_prog, pmic_rpc_ids.vib_api_vers, 0);

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: connect failed with current VERS = %x, \
				trying again with connect_compatible API\n",
				__func__, (unsigned int)pmic_rpc_ids.vib_api_vers);
		pmic_rpc_ep = msm_rpc_connect_compatible(pmic_rpc_ids.vib_api_prog,
						PMIC_RPC_API_VERS_COMP, 0);
	}

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: connect failed VERS_COMP = %x, \
			       trying again with old version ids \n",
				__func__, PMIC_RPC_API_VERS_COMP);
	}

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: connect failed with old versions also \n",
				__func__);
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
			__func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	} else
		printk(KERN_INFO "%s: vib rpc connect success\n", __func__);

	return 0;
}

int msm_pmic_rpc_close(void)
{
	int rc = 0;

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: rpc_close failed before call, rc = %ld\n",
			__func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_close(pmic_rpc_ep);
	pmic_rpc_ep = NULL;

	if (rc < 0) {
		printk(KERN_ERR "%s: close rpc failed! rc = %d\n",
			__func__, rc);
		return -EAGAIN;
	} else
		printk(KERN_INFO "rpc close success\n");

	return rc;
}

int msm_vib_mot_set_volt_proc(uint32_t volt)
{
	int rc = 0;

	struct vib_start_req {
		struct rpc_request_hdr hdr;
		uint32_t otg_dev;
	} req;

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: set_volt rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	}
	req.otg_dev = cpu_to_be32(volt);
	rc = msm_rpc_call(pmic_rpc_ep, pmic_rpc_ids.vib_mot_set_volt_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		printk(KERN_ERR "%s: set_volt failed! rc = %d\n",
			__func__, rc);
	} else
		printk(KERN_INFO "%s\n", __func__);

	return rc;
}

int msm_vib_mot_set_mode_proc(uint32_t mode)
{
	int rc = 0;

	struct vib_start_req {
		struct rpc_request_hdr hdr;
		uint32_t otg_dev;
	} req;

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: set_mode rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	}
	req.otg_dev = cpu_to_be32(mode);
	rc = msm_rpc_call(pmic_rpc_ep, pmic_rpc_ids.vib_mot_set_mode_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		printk(KERN_ERR "%s: set_mode failed! rc = %d\n",
			__func__, rc);
	} else
		printk(KERN_INFO "%s\n", __func__);

	return rc;
}

int msm_vib_mot_set_pol_proc(uint32_t pol)
{
	int rc = 0;

	struct vib_start_req {
		struct rpc_request_hdr hdr;
		uint32_t otg_dev;
	} req;

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: set_pol rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	}
	req.otg_dev = cpu_to_be32(pol);
	rc = msm_rpc_call(pmic_rpc_ep, pmic_rpc_ids.vib_mot_set_polarity_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		printk(KERN_ERR "%s: set_pol failed! rc = %d\n",
			__func__, rc);
	} else
		printk(KERN_INFO "%s\n", __func__);

	return rc;
}

int msm_set_led_intensity_proc(pm_led_intensity_type led, uint8_t val)
{
	int rc = 0;

	struct vib_start_req {
		struct rpc_request_hdr hdr;
		pm_led_intensity_type led_dev;
		uint32_t led_val;
	} req;

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: set_ rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	}

	req.led_dev = cpu_to_be32((uint32_t)led);
	req.led_val = val << 24;
	rc = msm_rpc_call(pmic_rpc_ep, pmic_rpc_ids.led_set_intensity_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		printk(KERN_ERR "%s: set_intensity failed! rc = %d\n",
			__func__, rc);
	} else
		printk(KERN_INFO "%s\n", __func__);

	return rc;
}
EXPORT_SYMBOL(msm_set_led_intensity_proc);

int msm_mic_en_proc(bool disable_enable)
{
	int rc = 0;

	struct vib_start_req {
		struct rpc_request_hdr hdr;
		uint32_t otg_dev;
	} req;

	if (IS_ERR(pmic_rpc_ep)) {
		printk(KERN_ERR "%s: mic_en rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(pmic_rpc_ep));
		return -EAGAIN;
	}
	req.otg_dev = cpu_to_be32((uint32_t)disable_enable);
	rc = msm_rpc_call(pmic_rpc_ep, pmic_rpc_ids.mic_en_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		printk(KERN_ERR "%s: mic_en failed! rc = %d\n",
			__func__, rc);
	} else
		printk(KERN_INFO "%s\n", __func__);

	return rc;
}
EXPORT_SYMBOL(msm_mic_en_proc);

static int pmic_rpc_probe(struct platform_device *pdev)
{	
	int rc = -EIO;
	
	pmic_rpc_drv_data.pmic_dev = pdev;
	
	rc = msm_pmic_rpc_connect();
	if (rc < 0) {
		dev_err(&(pdev->dev), "%s: Connect to RPC server failed!! <%d>", __func__, rc);
		
		return rc; 	
	}
	
	return 0;
}

static int pmic_rpc_remove(struct platform_device *pdev)
{
	int rc = -EIO;
	
	rc = msm_pmic_rpc_close();
	if (rc < 0) {
		dev_err(&(pdev->dev), "%s: Disconnect to RPC server failed!! <%d>", __func__, rc);
		
		return rc;	
	}
	
	return 0;
}

static struct platform_driver pmic_rpc_driver = {
	.driver = {
		.name = "pmic_rpc",
		.owner = THIS_MODULE,
	},
	.probe = pmic_rpc_probe,
	.remove = pmic_rpc_remove,
};

static int __init pmic_rpc_init(void)
{
	return platform_driver_register(&pmic_rpc_driver);
}

static void __exit pmic_rpc_exit(void)
{
	platform_driver_unregister(&pmic_rpc_driver);
}

module_init(pmic_rpc_init);
module_exit(pmic_rpc_exit);
