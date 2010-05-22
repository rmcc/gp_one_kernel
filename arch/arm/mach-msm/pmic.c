/*
 * Copyright (c) 2009 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include "smd_rpcrouter.h"
#include "pmic.h"

#define TRACE_SPEAKER 0

#if TRACE_SPEAKER
#define SPEAKER(x...) printk(KERN_INFO "[SPEAKER] " x)
#else
#define SPEAKER(x...) do {} while (0)
#endif

/* rpc related */
#define SPEAKER_PDEV_NAME	"rs00010001:00000000"
#define SPEAKER_RPC_PROG	0x30000061
#define SPEAKER_RPC_VER		0x00010001

#define SPEAKER_CMD_PROC 20
#define SET_SPEAKER_GAIN_PROC 21
#define SPKR_EN_RIGHT_CHAN_PROC 32
#define SPKR_IS_RIGHT_CHAN_EN_PROC 33
#define SPKR_EN_LEFT_CHAN_PROC 34
#define SPKR_IS_LEFT_CHAN_EN_PROC 35
#define SPKR_GET_GAIN_PROC 38
#define SPKR_IS_EN_PROC 39

struct std_rpc_reply {
	struct rpc_reply_hdr hdr;
	uint32_t result;
};

struct enable_chan_req {
	struct rpc_request_hdr hdr;
	uint32_t enable;
};

struct is_chan_enabled_req {
	struct rpc_request_hdr req;
	uint32_t output_pointer_not_null;
};

struct is_chan_enabled_rep {
	struct std_rpc_reply reply_hdr;
	uint32_t MoreData;
	uint32_t enabled;
};

struct spkr_gain_req {
	struct rpc_request_hdr hdr;
	uint32_t left_right; /* no enums, rpc expects uint32_t */
	uint32_t output_pointer_not_null;
};

struct spkr_gain_rep {
	struct std_rpc_reply reply_hdr;
	uint32_t MoreData;
	uint32_t gain; /* no enums, rpc expects uint32_t */
};

struct set_gain_req {
	struct rpc_request_hdr hdr;
	uint32_t gain; /* no enums, rpc expects uint32_t */
};

struct speaker_cmd_req {
	struct rpc_request_hdr hdr;
	uint32_t speaker_cmd; /* no enums, rpc expects uint32_t */
};

static struct msm_rpc_endpoint *endpoint;

static int check_and_connect(void)
{
	/* make fast path here the one where it is already connected */
	if (likely(endpoint))
		return 0;

	endpoint = msm_rpc_connect(SPEAKER_RPC_PROG,
				   SPEAKER_RPC_VER,
				   0);
	if (unlikely(!endpoint))
		return -ENODEV;
	else if (IS_ERR(endpoint)) {
		int rc = PTR_ERR(endpoint);
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __func__, PTR_ERR(endpoint));
		endpoint = 0;
		return rc;
	}
	return 0;
}

int spkr_en_right_chan(unsigned char enable)
{
	struct enable_chan_req req;
	struct std_rpc_reply rep;
	int rc;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.enable = cpu_to_be32(enable);
	rc = msm_rpc_call_reply(endpoint, SPKR_EN_RIGHT_CHAN_PROC,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5*HZ);
	if (rc < 0)
		return rc;

	return (int)be32_to_cpu(rep.result);
}
EXPORT_SYMBOL(spkr_en_right_chan);

int spkr_is_right_chan_en(unsigned char *enabled)
{
	struct is_chan_enabled_req req;
	struct is_chan_enabled_rep rep;
	int rc;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.output_pointer_not_null = enabled ? cpu_to_be32(1) : 0;

	rc = msm_rpc_call_reply(endpoint, SPKR_IS_RIGHT_CHAN_EN_PROC,
				&req, sizeof(req),
				    &rep, sizeof(rep),
				    5*HZ);
	if (rc < 0)
		return rc;

			if (!rep.MoreData)
				return -ENOMSG;

	else if (!rep.reply_hdr.result && enabled)
		*enabled = be32_to_cpu(rep.enabled);

	return (int)be32_to_cpu(rep.reply_hdr.result);
}
EXPORT_SYMBOL(spkr_is_right_chan_en);

int spkr_en_left_chan(unsigned char enable)
{
	struct enable_chan_req req;
	struct std_rpc_reply rep;
	int rc;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.enable = cpu_to_be32(enable);
	rc = msm_rpc_call_reply(endpoint, SPKR_EN_LEFT_CHAN_PROC,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5*HZ);
	if (rc < 0)
	return rc;

	return (int)be32_to_cpu(rep.result);
}
EXPORT_SYMBOL(spkr_en_left_chan);

int spkr_is_left_chan_en(unsigned char *enabled)
{
	struct is_chan_enabled_req req;
	struct is_chan_enabled_rep rep;
	int rc;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.output_pointer_not_null = enabled ? cpu_to_be32(1) : 0;

	rc = msm_rpc_call_reply(endpoint, SPKR_IS_LEFT_CHAN_EN_PROC,
				    &req, sizeof(req),
				    &rep, sizeof(rep),
				    5*HZ);
	if (rc < 0)
	return rc;

	if (!rep.MoreData)
		return -ENOMSG;
	else if (!rep.reply_hdr.result && enabled)
		*enabled = be32_to_cpu(rep.enabled);

	return (int)be32_to_cpu(rep.reply_hdr.result);
}
EXPORT_SYMBOL(spkr_is_left_chan_en);

int spkr_is_en(enum spkr_left_right_type left_right, unsigned char *enabled)
{
	if (left_right >= SPKR_OUT_OF_RANGE)
		return -ENOMSG;

	return left_right == LEFT_SPKR ?
		spkr_is_left_chan_en(enabled) :
		spkr_is_right_chan_en(enabled);
}
EXPORT_SYMBOL(spkr_is_en);

int spkr_get_gain(enum spkr_left_right_type left_right,
		  enum spkr_gain_type *gain)
{
	struct spkr_gain_req req;
	struct spkr_gain_rep rep;
	int rc;

	if (left_right >= SPKR_OUT_OF_RANGE)
		return -ENOMSG;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.left_right = cpu_to_be32((uint32_t)left_right);
	req.output_pointer_not_null = gain ? cpu_to_be32(1) : 0;

	rc = msm_rpc_call_reply(endpoint, SPKR_GET_GAIN_PROC,
				    &req, sizeof(req),
				    &rep, sizeof(rep),
				    5*HZ);
	if (rc < 0)
		return rc;

		if (!rep.MoreData)
			return -ENOMSG;
	else if (!rep.reply_hdr.result && gain)
		*gain = (enum spkr_gain_type)be32_to_cpu(rep.gain);

	return (int)be32_to_cpu(rep.reply_hdr.result);
}
EXPORT_SYMBOL(spkr_get_gain);

int set_speaker_gain(enum spkr_gain_type speaker_gain)
{
	struct set_gain_req req;
	struct std_rpc_reply rep;
	int rc;

	if (speaker_gain >= SPKR_GAIN_OUT_OF_RANGE)
		return -ENOMSG;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.gain = cpu_to_be32((uint32_t)speaker_gain);

	rc = msm_rpc_call_reply(endpoint, SET_SPEAKER_GAIN_PROC,
				&req, sizeof(req),
				&rep, sizeof(rep),
				    5*HZ);
	if (rc < 0)
		return rc;

	return (int)be32_to_cpu(rep.result);
}
EXPORT_SYMBOL(set_speaker_gain);

int speaker_cmd(enum spkr_cmd_type cmd)
{
	struct speaker_cmd_req req;
	struct std_rpc_reply rep;
	int rc;

	if (cmd >= SPKR_CMD_OUT_OF_RANGE)
		return -ENOMSG;

	rc = check_and_connect();
	if (rc)
		return rc;

	req.speaker_cmd = cpu_to_be32((uint32_t)cmd);

	rc = msm_rpc_call_reply(endpoint, SPEAKER_CMD_PROC,
				    &req, sizeof(req),
				    &rep, sizeof(rep),
				    5*HZ);
	if (rc < 0)
		return rc;

	return (int)be32_to_cpu(rep.result);
}
EXPORT_SYMBOL(speaker_cmd);

/*=========================================================================*/

#if defined(CONFIG_DEBUG_FS)
static int spkr_en_chan_execute(void *data, u64 val)
{
	int rc = ((enum spkr_left_right_type)data) == LEFT_SPKR ?
		spkr_en_left_chan((unsigned char)val) :
		spkr_en_right_chan((unsigned char)val);

	if (rc)
		printk(KERN_INFO "%s: val %llu, rc: %d(%0x)\n",
			__func__, val, rc, rc);
	else
		printk(KERN_ERR "%s: val %llu, rc: %d(%0x)\n",
			__func__, val, rc, rc);

	return rc;
}

static int spkr_is_chan_en_get(void *data, u64 *val)
{
	unsigned char enabled;
	int rc = ((enum spkr_left_right_type)data) == LEFT_SPKR ?
		  spkr_is_left_chan_en(&enabled) :
		  spkr_is_right_chan_en(&enabled);

	if (!rc) {
		*val = (u64)enabled;
		printk(KERN_INFO "%s: val %llu, rc: %d(%0x)\n",
			__func__, *val, rc, rc);
	} else
		printk(KERN_ERR "%s: val %llu, rc: %d(%0x)\n",
			__func__, *val, rc, rc);

	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(spkr_en_chan_fops,
			spkr_is_chan_en_get,
			spkr_en_chan_execute,
			"%llu\n");

static int spkr_get_gain_get(void *data, u64 *val)
{
	enum spkr_gain_type gain;
	int rc = spkr_get_gain((enum spkr_left_right_type)data, &gain);

	if (!rc) {
		*val = (u64)gain;
		printk(KERN_INFO "%s: val %llu, rc: %d(%0x)\n",
			__func__, *val, rc, rc);
	} else
		printk(KERN_ERR "%s: val %llu, rc: %d(%0x)\n",
			__func__, *val, rc, rc);

	return rc;
}

static int set_speaker_gain_execute(void *data, u64 val)
{
	int rc = set_speaker_gain((enum spkr_gain_type)val);

	if (rc)
		printk(KERN_INFO "%s: val %llu, rc: %d(%0x)\n",
			__func__, val, rc, rc);
	else
		printk(KERN_ERR "%s: val %llu, rc: %d(%0x)\n",
			__func__, val, rc, rc);
	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(spkr_gain_fops,
			spkr_get_gain_get,
			set_speaker_gain_execute,
			"%llu\n");

static int speaker_cmd_execute(void *data, u64 val)
{
	int rc = speaker_cmd((enum spkr_cmd_type)val);

	if (rc)
		printk(KERN_INFO "%s: val %llu, rc: %d(%0x)\n",
			__func__, val, rc, rc);
	else
		printk(KERN_ERR "%s: val %llu, rc: %d(%0x)\n",
			__func__, val, rc, rc);
	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(speaker_cmd_fops,
			NULL,
			speaker_cmd_execute,
			"%llu\n");

static int __init speaker_debugfs_init(void)
{
	struct dentry *dent = debugfs_create_dir("speaker", 0);

	if (IS_ERR(dent)) {
		printk(KERN_ERR "%s: debugfs_create_dir fail, error %ld\n",
			__func__, PTR_ERR(dent));
		return 0;
	}
	debugfs_create_file("left_en", 0644, dent, (void *)LEFT_SPKR,
				&spkr_en_chan_fops);
	debugfs_create_file("right_en", 0644, dent, (void *)RIGHT_SPKR,
				&spkr_en_chan_fops);
	debugfs_create_file("left_gain", 0644, dent, (void *)LEFT_SPKR,
				&spkr_gain_fops);
	debugfs_create_file("right_gain", 0644, dent, (void *)RIGHT_SPKR,
				&spkr_gain_fops);
	debugfs_create_file("cmd", 0644, dent, NULL, &speaker_cmd_fops);
	return 0;
}

late_initcall(speaker_debugfs_init);

static int __init speaker_init(void)
{
	/* try to connect initially, ignore any errors for now */
	check_and_connect();
	return 0;
}

device_initcall(speaker_init);
#endif
