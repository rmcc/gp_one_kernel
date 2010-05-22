#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/compile.h>

#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

/* FIH, AudiPCHuang, 2009/06/05 { */
/* [ADQ.B-1440], For getting image version from splash.img*/
static char adq_image_version[32];

void set_image_version(char* img_ver)
{
	snprintf(adq_image_version, 25, "%s", img_ver);
}
EXPORT_SYMBOL(set_image_version);
/* } FIH, AudiPCHuang, 2009/06/05 */

static int proc_calc_metrics(char *page, char **start, off_t off,
				 int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int build_version_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;

	len = snprintf(page, PAGE_SIZE, "%s\n", /*ADQ_IMAGE_VERSION*/adq_image_version);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int device_model_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;
	int HWID = FIH_READ_HWID_FROM_SMEM();
	char ver[24];
	
	switch (HWID){
	case CMCS_HW_VER_EVB1:
		strcpy(ver, "EVB1");
		break; 
	case CMCS_HW_VER_EVB2:
		strcpy(ver, "EVB2");
		break; 
	case CMCS_HW_VER_EVB3:
		strcpy(ver, "EVB3");
		break; 
	case CMCS_HW_VER_EVB4:
		strcpy(ver, "EVB4");
		break; 
	case CMCS_HW_VER_EVB5:
		strcpy(ver, "EVB5");
		break; 
	case CMCS_HW_VER_EVB6:
		strcpy(ver, "EVB6");
		break; 
	case CMCS_HW_VER_EVB7:
		strcpy(ver, "EVB7");
		break; 
	case CMCS_HW_VER_EVB8:
		strcpy(ver, "EVB8");
		break; 
	case CMCS_HW_VER_EVB9:
		strcpy(ver, "EVB9");
		break; 
	case CMCS_HW_VER_IVT:
		strcpy(ver, "IVT");
		break; 
	case CMCS_HW_VER_EVT1:
		strcpy(ver, "EVT1");
		break;    
	case CMCS_HW_VER_EVT2:
		strcpy(ver, "EVT2");
		break; 
	case CMCS_HW_VER_EVT3:
		strcpy(ver, "EVT3");
		break; 
	case CMCS_HW_VER_EVT4:
		strcpy(ver, "EVT4");
		break; 
	case CMCS_HW_VER_EVT5:
		strcpy(ver, "EVT5");
		break; 
	case CMCS_HW_VER_DVT1:
		strcpy(ver, "DVT1");
		break;
	case CMCS_HW_VER_DVT2:
		strcpy(ver, "DVT2");
		break;
	case CMCS_HW_VER_DVT3:
		strcpy(ver, "DVT3");
		break;
	case CMCS_HW_VER_DVT4:
		strcpy(ver, "DVT4");
		break;
	case CMCS_HW_VER_DVT5:
		strcpy(ver, "DVT5");
		break;
	case CMCS_HW_VER_PVT:
		strcpy(ver, "PVT");
		break;
	case CMCS_HW_VER_MP: 
		strcpy(ver, "MP");
		break;
	default:
		strcpy(ver, "Unkonwn Device Model");
		break;
	}

	len = snprintf(page, PAGE_SIZE, "%s\n",
		ver);
		
	return proc_calc_metrics(page, start, off, count, eof, len);	
}

static int baseband_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;
	char baseband[24];

#if defined(CONFIG_MACH_MSM7X25_SURF) || defined(CONFIG_MACH_MSM7X25_SURF)
	strcpy(baseband, "QUALCOMM MSM7X25");
#elif defined(CONFIG_MACH_MSM7201_SURF) || defined(CONFIG_MACH_MSM7601A_SURF) || defined(CONFIG_MACH_MSM7201A_FFA)  || defined(CONFIG_MACH_MSM7601A_FFA)
	strcpy(baseband, "QUALCOMM MSM7X01A");
#else
	strcpy(baseband, "Unknown Baseband Type");
#endif

	len = snprintf(page, PAGE_SIZE, "%s\n",
		baseband);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

static struct {
		char *name;
		int (*read_proc)(char*,char**,off_t,int,int*,void*);
} *p, adq_info[] = {
	{"socinfo",	build_version_read_proc},
	{"devmodel",	device_model_read_proc},
	{"baseband",	baseband_read_proc},
	{NULL,},
};

void adq_info_init(void)
{	
	for (p = adq_info; p->name; p++)
		create_proc_read_entry(p->name, 0, NULL, p->read_proc, NULL);
}
EXPORT_SYMBOL(adq_info_init);

void adq_info_remove(void)
{
	for (p = adq_info; p->name; p++)
		remove_proc_entry(p->name, NULL);
}
EXPORT_SYMBOL(adq_info_remove);
