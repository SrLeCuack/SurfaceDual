// SPDX-License-Identifier: GPL-2.0-only
#include <linux/compat.h>
#include <linux/dma-fence.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sync_file.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include "sdsync.h"

struct sdsync_timeline {
	u64 context;
	atomic64_t seqno;
};

struct sdsync_fence {
	struct dma_fence base;
	spinlock_t lock;
	struct eventfd_ctx *event;
	wait_queue_head_t *head;
	wait_queue_entry_t wait;
	poll_table poll;
	struct work_struct work;
	unsigned long queued;
};

static const char *sdsync_name(struct dma_fence *fence)
{
	return "sdsync";
}

static void sdsync_release(struct dma_fence *fence)
{
	struct sdsync_fence *s = container_of(fence, struct sdsync_fence, base);

	/* La cola serializa el último put con un aviso que intenta tomar referencia. */
	if (s->event) {
		if (s->head)
			remove_wait_queue(s->head, &s->wait);
		eventfd_ctx_put(s->event);
	}
	dma_fence_free(fence);
}

static const struct dma_fence_ops sdsync_ops = {
	.get_driver_name = sdsync_name,
	.get_timeline_name = sdsync_name,
	.release = sdsync_release,
};

static void sdsync_signal(struct work_struct *work)
{
	struct sdsync_fence *s = container_of(work, struct sdsync_fence, work);

	remove_wait_queue(s->head, &s->wait);
	eventfd_ctx_put(s->event);
	s->event = NULL;
	/* Fuera del lock del eventfd: los callbacks de otras fences pueden reentrar. */
	dma_fence_signal(&s->base);
	dma_fence_put(&s->base);
}

static void sdsync_queue(struct sdsync_fence *s)
{
	if (!test_and_set_bit(0, &s->queued) &&
	    kref_get_unless_zero(&s->base.refcount))
		schedule_work(&s->work);
}

static int sdsync_wake(wait_queue_entry_t *wait, unsigned int mode,
		       int sync, void *key)
{
	struct sdsync_fence *s = container_of(wait, struct sdsync_fence, wait);

	if (key_to_poll(key) & EPOLLIN)
		sdsync_queue(s);
	return 0;
}

static void sdsync_poll_queue(struct file *file, wait_queue_head_t *head,
			      poll_table *poll)
{
	struct sdsync_fence *s = container_of(poll, struct sdsync_fence, poll);

	s->head = head;
	add_wait_queue(head, &s->wait);
}

static long sdsync_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sdsync_timeline *timeline = file->private_data;
	struct sdsync_desde_eventfd req;
	struct sdsync_fence *s;
	struct sync_file *sync;
	struct file *event;
	void __user *user = (void __user *)arg;
	unsigned long flags;
	int fd, ret;

	if (cmd != SDSYNC_IOC_DESDE_EVENTFD)
		return -ENOTTY;
	if (copy_from_user(&req, user, sizeof(req)))
		return -EFAULT;
	event = eventfd_fget(req.eventfd);
	if (IS_ERR(event))
		return PTR_ERR(event);
	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		fput(event);
		return -ENOMEM;
	}
	s->event = eventfd_ctx_fileget(event);
	if (IS_ERR(s->event)) {
		ret = PTR_ERR(s->event);
		kfree(s);
		fput(event);
		return ret;
	}
	spin_lock_init(&s->lock);
	dma_fence_init(&s->base, &sdsync_ops, &s->lock, timeline->context,
		       atomic64_inc_return(&timeline->seqno));
	INIT_WORK(&s->work, sdsync_signal);
	init_waitqueue_func_entry(&s->wait, sdsync_wake);
	init_poll_funcptr(&s->poll, sdsync_poll_queue);
	if (vfs_poll(event, &s->poll) & EPOLLIN) {
		spin_lock_irqsave(&s->head->lock, flags);
		sdsync_queue(s);
		spin_unlock_irqrestore(&s->head->lock, flags);
		/* El callback ya terminó de encolar antes de esperar el trabajo. */
		flush_work(&s->work);
	}
	fput(event);
	sync = sync_file_create(&s->base);
	dma_fence_put(&s->base);
	if (!sync)
		return -ENOMEM;
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		fput(sync->file);
		return fd;
	}
	req.fence = fd;
	if (copy_to_user(user, &req, sizeof(req))) {
		put_unused_fd(fd);
		fput(sync->file);
		return -EFAULT;
	}
	fd_install(fd, sync->file);
	return 0;
}

static int sdsync_open(struct inode *inode, struct file *file)
{
	struct sdsync_timeline *timeline = kzalloc(sizeof(*timeline), GFP_KERNEL);

	if (!timeline)
		return -ENOMEM;
	timeline->context = dma_fence_context_alloc(1);
	atomic64_set(&timeline->seqno, 0);
	file->private_data = timeline;
	return 0;
}

static int sdsync_close(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations sdsync_fops = {
	.owner = THIS_MODULE,
	.open = sdsync_open,
	.release = sdsync_close,
	.unlocked_ioctl = sdsync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
};

static struct miscdevice sdsync_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sdsync",
	.fops = &sdsync_fops,
	.mode = 0666,
};

static int __init sdsync_init(void)
{
	return misc_register(&sdsync_device);
}
device_initcall(sdsync_init);
MODULE_LICENSE("GPL");
