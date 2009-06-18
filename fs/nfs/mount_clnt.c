/*
 * In-kernel MOUNT protocol client
 *
 * Copyright (C) 1997, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/uio.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/sched.h>
#include <linux/nfs_fs.h>
#include "internal.h"

#ifdef RPC_DEBUG
# define NFSDBG_FACILITY	NFSDBG_MOUNT
#endif

/*
 * Defined by RFC 1094, section A.3; and RFC 1813, section 5.1.4
 */
#define MNTPATHLEN		(1024)

/*
 * XDR data type sizes
 */
#define encode_dirpath_sz	(1 + XDR_QUADLEN(MNTPATHLEN))
#define MNT_status_sz		(1)
#define MNT_fhs_status_sz	(1)
#define MNT_fhandle_sz		XDR_QUADLEN(NFS2_FHSIZE)
#define MNT_fhandle3_sz		(1 + XDR_QUADLEN(NFS3_FHSIZE))

/*
 * XDR argument and result sizes
 */
#define MNT_enc_dirpath_sz	encode_dirpath_sz

/*
 * Defined by RFC 1094, section A.5
 */
enum {
	MOUNTPROC_NULL		= 0,
	MOUNTPROC_MNT		= 1,
	MOUNTPROC_DUMP		= 2,
	MOUNTPROC_UMNT		= 3,
	MOUNTPROC_UMNTALL	= 4,
	MOUNTPROC_EXPORT	= 5,
};

/*
 * Defined by RFC 1813, section 5.2
 */
enum {
	MOUNTPROC3_NULL		= 0,
	MOUNTPROC3_MNT		= 1,
	MOUNTPROC3_DUMP		= 2,
	MOUNTPROC3_UMNT		= 3,
	MOUNTPROC3_UMNTALL	= 4,
	MOUNTPROC3_EXPORT	= 5,
};

static struct rpc_program	mnt_program;

/*
 * Defined by OpenGroup XNFS Version 3W, chapter 8
 */
enum mountstat {
	MNT_OK			= 0,
	MNT_EPERM		= 1,
	MNT_ENOENT		= 2,
	MNT_EACCES		= 13,
	MNT_EINVAL		= 22,
};

static struct {
	u32 status;
	int errno;
} mnt_errtbl[] = {
	{ .status = MNT_OK,			.errno = 0,		},
	{ .status = MNT_EPERM,			.errno = -EPERM,	},
	{ .status = MNT_ENOENT,			.errno = -ENOENT,	},
	{ .status = MNT_EACCES,			.errno = -EACCES,	},
	{ .status = MNT_EINVAL,			.errno = -EINVAL,	},
};

/*
 * Defined by RFC 1813, section 5.1.5
 */
enum mountstat3 {
	MNT3_OK			= 0,		/* no error */
	MNT3ERR_PERM		= 1,		/* Not owner */
	MNT3ERR_NOENT		= 2,		/* No such file or directory */
	MNT3ERR_IO		= 5,		/* I/O error */
	MNT3ERR_ACCES		= 13,		/* Permission denied */
	MNT3ERR_NOTDIR		= 20,		/* Not a directory */
	MNT3ERR_INVAL		= 22,		/* Invalid argument */
	MNT3ERR_NAMETOOLONG	= 63,		/* Filename too long */
	MNT3ERR_NOTSUPP		= 10004,	/* Operation not supported */
	MNT3ERR_SERVERFAULT	= 10006,	/* A failure on the server */
};

static struct {
	u32 status;
	int errno;
} mnt3_errtbl[] = {
	{ .status = MNT3_OK,			.errno = 0,		},
	{ .status = MNT3ERR_PERM,		.errno = -EPERM,	},
	{ .status = MNT3ERR_NOENT,		.errno = -ENOENT,	},
	{ .status = MNT3ERR_IO,			.errno = -EIO,		},
	{ .status = MNT3ERR_ACCES,		.errno = -EACCES,	},
	{ .status = MNT3ERR_NOTDIR,		.errno = -ENOTDIR,	},
	{ .status = MNT3ERR_INVAL,		.errno = -EINVAL,	},
	{ .status = MNT3ERR_NAMETOOLONG,	.errno = -ENAMETOOLONG,	},
	{ .status = MNT3ERR_NOTSUPP,		.errno = -ENOTSUPP,	},
	{ .status = MNT3ERR_SERVERFAULT,	.errno = -ESERVERFAULT,	},
};

struct mountres {
	int errno;
	struct nfs_fh *fh;
};

struct mnt_fhstatus {
	u32 status;
	struct nfs_fh *fh;
};

/**
 * nfs_mount - Obtain an NFS file handle for the given host and path
 * @info: pointer to mount request arguments
 *
 * Uses default timeout parameters specified by underlying transport.
 */
int nfs_mount(struct nfs_mount_request *info)
{
	struct mnt_fhstatus	result = {
		.fh		= info->fh
	};
	struct rpc_message msg	= {
		.rpc_argp	= info->dirpath,
		.rpc_resp	= &result,
	};
	struct rpc_create_args args = {
		.protocol	= info->protocol,
		.address	= info->sap,
		.addrsize	= info->salen,
		.servername	= info->hostname,
		.program	= &mnt_program,
		.version	= info->version,
		.authflavor	= RPC_AUTH_UNIX,
	};
	struct rpc_clnt		*mnt_clnt;
	int			status;

	dprintk("NFS: sending MNT request for %s:%s\n",
		(info->hostname ? info->hostname : "server"),
			info->dirpath);

	if (info->noresvport)
		args.flags |= RPC_CLNT_CREATE_NONPRIVPORT;

	mnt_clnt = rpc_create(&args);
	if (IS_ERR(mnt_clnt))
		goto out_clnt_err;

	if (info->version == NFS_MNT3_VERSION)
		msg.rpc_proc = &mnt_clnt->cl_procinfo[MOUNTPROC3_MNT];
	else
		msg.rpc_proc = &mnt_clnt->cl_procinfo[MOUNTPROC_MNT];

	status = rpc_call_sync(mnt_clnt, &msg, 0);
	rpc_shutdown_client(mnt_clnt);

	if (status < 0)
		goto out_call_err;
	if (result.status != 0)
		goto out_mnt_err;

	dprintk("NFS: MNT request succeeded\n");
	status = 0;

out:
	return status;

out_clnt_err:
	status = PTR_ERR(mnt_clnt);
	dprintk("NFS: failed to create RPC client, status=%d\n", status);
	goto out;

out_call_err:
	dprintk("NFS: failed to start MNT request, status=%d\n", status);
	goto out;

out_mnt_err:
	dprintk("NFS: MNT server returned result %d\n", result.status);
	status = nfs_stat_to_errno(result.status);
	goto out;
}

/*
 * XDR encode/decode functions for MOUNT
 */

static int encode_mntdirpath(struct xdr_stream *xdr, const char *pathname)
{
	const u32 pathname_len = strlen(pathname);
	__be32 *p;

	if (unlikely(pathname_len > MNTPATHLEN))
		return -EIO;

	p = xdr_reserve_space(xdr, sizeof(u32) + pathname_len);
	if (unlikely(p == NULL))
		return -EIO;
	xdr_encode_opaque(p, pathname, pathname_len);

	return 0;
}

static int mnt_enc_dirpath(struct rpc_rqst *req, __be32 *p,
			   const char *dirpath)
{
	struct xdr_stream xdr;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	return encode_mntdirpath(&xdr, dirpath);
}

static int xdr_decode_fhstatus(struct rpc_rqst *req, __be32 *p,
			       struct mnt_fhstatus *res)
{
	struct nfs_fh *fh = res->fh;

	if ((res->status = ntohl(*p++)) == 0) {
		fh->size = NFS2_FHSIZE;
		memcpy(fh->data, p, NFS2_FHSIZE);
	}
	return 0;
}

/*
 * RFC 1094: "A non-zero status indicates some sort of error.  In this
 * case, the status is a UNIX error number."  This can be problematic
 * if the server and client use different errno values for the same
 * error.
 *
 * However, the OpenGroup XNFS spec provides a simple mapping that is
 * independent of local errno values on the server and the client.
 */
static int decode_status(struct xdr_stream *xdr, struct mountres *res)
{
	unsigned int i;
	u32 status;
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(status));
	if (unlikely(p == NULL))
		return -EIO;
	status = ntohl(*p);

	for (i = 0; i <= ARRAY_SIZE(mnt_errtbl); i++) {
		if (mnt_errtbl[i].status == status) {
			res->errno = mnt_errtbl[i].errno;
			return 0;
		}
	}

	dprintk("NFS: unrecognized MNT status code: %u\n", status);
	res->errno = -EACCES;
	return 0;
}

static int decode_fhandle(struct xdr_stream *xdr, struct mountres *res)
{
	struct nfs_fh *fh = res->fh;
	__be32 *p;

	p = xdr_inline_decode(xdr, NFS2_FHSIZE);
	if (unlikely(p == NULL))
		return -EIO;

	fh->size = NFS2_FHSIZE;
	memcpy(fh->data, p, NFS2_FHSIZE);
	return 0;
}

static int decode_fhs_status(struct xdr_stream *xdr, struct mountres *res)
{
	unsigned int i;
	u32 status;
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(status));
	if (unlikely(p == NULL))
		return -EIO;
	status = ntohl(*p);

	for (i = 0; i <= ARRAY_SIZE(mnt3_errtbl); i++) {
		if (mnt3_errtbl[i].status == status) {
			res->errno = mnt3_errtbl[i].errno;
			return 0;
		}
	}

	dprintk("NFS: unrecognized MNT3 status code: %u\n", status);
	res->errno = -EACCES;
	return 0;
}

static int decode_fhandle3(struct xdr_stream *xdr, struct mountres *res)
{
	struct nfs_fh *fh = res->fh;
	u32 size;
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(size));
	if (unlikely(p == NULL))
		return -EIO;

	size = ntohl(*p++);
	if (size > NFS3_FHSIZE || size == 0)
		return -EIO;

	p = xdr_inline_decode(xdr, size);
	if (unlikely(p == NULL))
		return -EIO;

	fh->size = size;
	memcpy(fh->data, p, size);
	return 0;
}

static int xdr_decode_fhstatus3(struct rpc_rqst *req, __be32 *p,
				struct mnt_fhstatus *res)
{
	struct nfs_fh *fh = res->fh;
	unsigned size;

	if ((res->status = ntohl(*p++)) == 0) {
		size = ntohl(*p++);
		if (size <= NFS3_FHSIZE && size != 0) {
			fh->size = size;
			memcpy(fh->data, p, size);
		} else
			res->status = -EBADHANDLE;
	}
	return 0;
}

#define MNT_fhstatus_sz		(1 + 8)
#define MNT_fhstatus3_sz	(1 + 16)

static struct rpc_procinfo mnt_procedures[] = {
	[MOUNTPROC_MNT] = {
		.p_proc		= MOUNTPROC_MNT,
		.p_encode	= (kxdrproc_t)mnt_enc_dirpath,
		.p_decode	= (kxdrproc_t) xdr_decode_fhstatus,
		.p_arglen	= MNT_enc_dirpath_sz,
		.p_replen	= MNT_fhstatus_sz,
		.p_statidx	= MOUNTPROC_MNT,
		.p_name		= "MOUNT",
	},
};

static struct rpc_procinfo mnt3_procedures[] = {
	[MOUNTPROC3_MNT] = {
		.p_proc		= MOUNTPROC3_MNT,
		.p_encode	= (kxdrproc_t)mnt_enc_dirpath,
		.p_decode	= (kxdrproc_t) xdr_decode_fhstatus3,
		.p_arglen	= MNT_enc_dirpath_sz,
		.p_replen	= MNT_fhstatus3_sz,
		.p_statidx	= MOUNTPROC3_MNT,
		.p_name		= "MOUNT",
	},
};


static struct rpc_version mnt_version1 = {
	.number		= 1,
	.nrprocs	= 2,
	.procs		= mnt_procedures,
};

static struct rpc_version mnt_version3 = {
	.number		= 3,
	.nrprocs	= 2,
	.procs		= mnt3_procedures,
};

static struct rpc_version *mnt_version[] = {
	NULL,
	&mnt_version1,
	NULL,
	&mnt_version3,
};

static struct rpc_stat mnt_stats;

static struct rpc_program mnt_program = {
	.name		= "mount",
	.number		= NFS_MNT_PROGRAM,
	.nrvers		= ARRAY_SIZE(mnt_version),
	.version	= mnt_version,
	.stats		= &mnt_stats,
};
