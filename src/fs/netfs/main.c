// SPDX-License-Identifier: GPL-2.0-or-later
/* Miscellaneous bits for the netfs support library.
 *
 * Copyright (C) 2022 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#include <linux/module.h>
#include <linux/export.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "internal.h"
#define CREATE_TRACE_POINTS
#include <trace/events/netfs.h>

MODULE_DESCRIPTION("Network fs support");
MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL");

EXPORT_TRACEPOINT_SYMBOL(netfs_sreq);

unsigned netfs_debug;
module_param_named(debug, netfs_debug, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(netfs_debug, "Netfs support debugging mask");

#ifdef CONFIG_PROC_FS
LIST_HEAD(netfs_io_requests);
DEFINE_SPINLOCK(netfs_proc_lock);

static const char *netfs_origins[nr__netfs_io_origin] = {
	[NETFS_READAHEAD]		= "RA",
	[NETFS_READPAGE]		= "RP",
	[NETFS_READ_FOR_WRITE]		= "RW",
	[NETFS_WRITEBACK]		= "WB",
	[NETFS_WRITETHROUGH]		= "WT",
	[NETFS_LAUNDER_WRITE]		= "LW",
	[NETFS_UNBUFFERED_WRITE]	= "UW",
	[NETFS_DIO_READ]		= "DR",
	[NETFS_DIO_WRITE]		= "DW",
};

/*
 * Generate a list of I/O requests in /proc/fs/netfs/requests
 */
static int netfs_requests_seq_show(struct seq_file *m, void *v)
{
	struct netfs_io_request *rreq;

	if (v == &netfs_io_requests) {
		seq_puts(m,
			 "REQUEST  OR REF FL ERR  OPS COVERAGE\n"
			 "======== == === == ==== === =========\n"
			 );
		return 0;
	}

	rreq = list_entry(v, struct netfs_io_request, proc_link);
	seq_printf(m,
		   "%08x %s %3d %2lx %4d %3d @%04llx %zx/%zx",
		   rreq->debug_id,
		   netfs_origins[rreq->origin],
		   refcount_read(&rreq->ref),
		   rreq->flags,
		   rreq->error,
		   atomic_read(&rreq->nr_outstanding),
		   rreq->start, rreq->submitted, rreq->len);
	seq_putc(m, '\n');
	return 0;
}

static void *netfs_requests_seq_start(struct seq_file *m, loff_t *_pos)
	__acquires(rcu)
{
	rcu_read_lock();
	return seq_list_start_head(&netfs_io_requests, *_pos);
}

static void *netfs_requests_seq_next(struct seq_file *m, void *v, loff_t *_pos)
{
	return seq_list_next(v, &netfs_io_requests, _pos);
}

static void netfs_requests_seq_stop(struct seq_file *m, void *v)
	__releases(rcu)
{
	rcu_read_unlock();
}

static const struct seq_operations netfs_requests_seq_ops = {
	.start  = netfs_requests_seq_start,
	.next   = netfs_requests_seq_next,
	.stop   = netfs_requests_seq_stop,
	.show   = netfs_requests_seq_show,
};
#endif /* CONFIG_PROC_FS */

static int __init netfs_init(void)
{
	int ret = -ENOMEM;

	if (!proc_mkdir("fs/netfs", NULL))
		goto error;
	if (!proc_create_seq("fs/netfs/requests", S_IFREG | 0444, NULL,
			     &netfs_requests_seq_ops))
		goto error_proc;
#ifdef CONFIG_FSCACHE_STATS
	if (!proc_create_single("fs/netfs/stats", S_IFREG | 0444, NULL,
				netfs_stats_show))
		goto error_proc;
#endif

	ret = fscache_init();
	if (ret < 0)
		goto error_proc;
	return 0;

error_proc:
	remove_proc_entry("fs/netfs", NULL);
error:
	return ret;
}
fs_initcall(netfs_init);

static void __exit netfs_exit(void)
{
	fscache_exit();
	remove_proc_entry("fs/netfs", NULL);
}
module_exit(netfs_exit);
