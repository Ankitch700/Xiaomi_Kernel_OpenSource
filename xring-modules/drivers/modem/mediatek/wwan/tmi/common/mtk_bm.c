// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2022, MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "mtk_bm.h"
#include "mtk_debug.h"
#include "mtk_debugfs.h"

#ifdef CONFIG_TX00_UT_BM
#include "ut_bm_fake.h"
#endif

#define TAG "BM"
#define MTK_RELOAD_TH 3
#define MTK_WQ_NAME_LEN 48

static int mtk_bm_page_pool_create(struct mtk_bm_pool *pool)
{
	INIT_LIST_HEAD(&pool->list.buff_list);

	return 0;
}

static void mtk_bm_page_pool_destroy(struct mtk_bm_pool *pool)
{
	struct mtk_buff *mb, *next;

	list_for_each_entry_safe(mb, next, &pool->list.buff_list, entry) {
		list_del(&mb->entry);
		skb_free_frag(mb->data);
		kmem_cache_free(pool->bm_ctrl->list_cache_pool, mb);
	}
}

static void *mtk_bm_page_buff_alloc(struct mtk_bm_pool *pool)
{
	struct mtk_buff *mb;
	void *data;

	spin_lock_bh(&pool->lock);
	mb = list_first_entry_or_null(&pool->list.buff_list, struct mtk_buff, entry);
	if (!mb) {
		spin_unlock_bh(&pool->lock);
		data = netdev_alloc_frag(pool->buff_size);
	} else {
		list_del(&mb->entry);
		pool->curr_cnt--;
		spin_unlock_bh(&pool->lock);
		data = mb->data;
		kmem_cache_free(pool->bm_ctrl->list_cache_pool, mb);
	}

	if (pool->curr_cnt < pool->threshold)
		queue_work(pool->reload_workqueue, &pool->reload_work);

	return data;
}

static void mtk_bm_page_buff_free(struct mtk_bm_pool *pool, void *data)
{
	struct mtk_buff *mb;

	if (pool->curr_cnt >= pool->buff_cnt) {
		skb_free_frag(data);
		return;
	}

	mb = kmem_cache_alloc(pool->bm_ctrl->list_cache_pool, GFP_KERNEL);
	if (mb) {
		mb->data = data;
		spin_lock_bh(&pool->lock);
		list_add_tail(&mb->entry, &pool->list.buff_list);
		pool->curr_cnt++;
		spin_unlock_bh(&pool->lock);
	} else {
		skb_free_frag(data);
	}
}

static void mtk_bm_page_pool_reload(struct work_struct *work)
{
	struct mtk_bm_pool *pool = container_of(work, struct mtk_bm_pool, reload_work);
	struct mtk_buff *mb;

	while (pool->curr_cnt < pool->buff_cnt && !atomic_read(&pool->work_stop)) {
		mb = kmem_cache_alloc(pool->bm_ctrl->list_cache_pool, GFP_KERNEL);
		if (!mb)
			break;

		mb->data = netdev_alloc_frag(pool->buff_size);
		if (!mb->data) {
			kmem_cache_free(pool->bm_ctrl->list_cache_pool, mb);
			break;
		}

		spin_lock_bh(&pool->lock);
		list_add_tail(&mb->entry, &pool->list.buff_list);
		pool->curr_cnt++;
		spin_unlock_bh(&pool->lock);
	}
}

static struct mtk_buff_ops page_buf_ops = {
	.pool_create = mtk_bm_page_pool_create,
	.pool_destroy = mtk_bm_page_pool_destroy,
	.buff_alloc = mtk_bm_page_buff_alloc,
	.buff_free = mtk_bm_page_buff_free,
	.pool_reload = mtk_bm_page_pool_reload,
};

static int mtk_bm_skb_pool_create(struct mtk_bm_pool *pool)
{
	skb_queue_head_init(&pool->list.skb_list);

	return 0;
}

static void mtk_bm_skb_pool_destroy(struct mtk_bm_pool *pool)
{
	skb_queue_purge(&pool->list.skb_list);
}

static void *mtk_bm_skb_buff_alloc(struct mtk_bm_pool *pool)
{
	gfp_t gfp = GFP_KERNEL;
	struct sk_buff *skb;

	spin_lock_bh(&pool->lock);
	skb = __skb_dequeue(&pool->list.skb_list);
	spin_unlock_bh(&pool->lock);
	if (!skb) {
		if (in_irq() || in_softirq())
			gfp = GFP_ATOMIC;
		skb = __dev_alloc_skb(pool->buff_size, gfp);
	}

	if (pool->list.skb_list.qlen < pool->threshold)
		queue_work(pool->reload_workqueue, &pool->reload_work);

	return skb;
}

static void mtk_bm_skb_buff_free(struct mtk_bm_pool *pool, void *data)
{
	struct sk_buff *skb = data;

	if (pool->list.skb_list.qlen < pool->buff_cnt) {
		/* reset sk_buff (take __alloc_skb as ref.) */
		skb->data = skb->head;
		skb->len = 0;
		skb_reset_tail_pointer(skb);
		/* reserve memory as netdev_alloc_skb */
		skb_reserve(skb, NET_SKB_PAD);

		spin_lock_bh(&pool->lock);
		__skb_queue_tail(&pool->list.skb_list, skb);
		spin_unlock_bh(&pool->lock);
	} else {
		dev_kfree_skb_any(skb);
	}
}

static void mtk_bm_skb_pool_reload(struct work_struct *work)
{
	struct mtk_bm_pool *pool = container_of(work, struct mtk_bm_pool, reload_work);
	struct sk_buff *skb;

	while (pool->list.skb_list.qlen < pool->buff_cnt && !atomic_read(&pool->work_stop)) {
		skb = __dev_alloc_skb(pool->buff_size, GFP_KERNEL);
		if (!skb)
			break;

		spin_lock_bh(&pool->lock);
		__skb_queue_tail(&pool->list.skb_list, skb);
		spin_unlock_bh(&pool->lock);
	}
}

static struct mtk_buff_ops skb_buf_ops = {
	.pool_create = mtk_bm_skb_pool_create,
	.pool_destroy = mtk_bm_skb_pool_destroy,
	.buff_alloc = mtk_bm_skb_buff_alloc,
	.buff_free = mtk_bm_skb_buff_free,
	.pool_reload = mtk_bm_skb_pool_reload,
};

#ifdef CONFIG_DEBUG_FS

static ssize_t mtk_bm_dump_read(void *data, char *buf, ssize_t max_cnt)
{
	struct mtk_bm_pool *pool = data;
	int len;

	if (!pool) {
		len = scnprintf(buf, max_cnt, "bm buffcnt simple read\n");
	} else {
		if (pool->type == MTK_BUFF_SKB) {
			spin_lock_bh(&pool->lock);
			pool->curr_cnt = pool->list.skb_list.qlen;
			spin_unlock_bh(&pool->lock);
		}
		len = scnprintf(buf, max_cnt,
				"pool_name: %s\n"
				"pool_id: %d\n"
				"type: %d\n"
				"threshold: %d\n"
				"buff_size: %d\n"
				"buff_cnt: %d\n"
				"curr_cnt: %d\n"
				"prio: %d\n",
				pool->pool_name,
				pool->pool_id,
				pool->type,
				pool->threshold,
				pool->buff_size,
				pool->buff_cnt,
				pool->curr_cnt,
				pool->prio);
	}

	return len;
}

MTK_DBGFS(dump, mtk_bm_dump_read, NULL);

static void mtk_bm_adjust_threshold(struct mtk_bm_pool *pool, unsigned int threshold)
{
	unsigned int dflt_threshold;

	if (!pool)
		return;

	dflt_threshold = pool->buff_cnt - pool->buff_cnt / MTK_RELOAD_TH;
	if (threshold > dflt_threshold)
		pool->threshold = dflt_threshold;
	else
		pool->threshold = threshold;
}

static ssize_t mtk_bm_threshold_read(void *data, char *buf, ssize_t max_cnt)
{
	struct mtk_bm_pool *pool = data;

	if (!pool)
		return scnprintf(buf, max_cnt, "bm threshold simple read\n");
	else
		return scnprintf(buf, max_cnt, "threshold = %d\n", pool->threshold);
}

static ssize_t mtk_bm_threshold_write(void *data, const char *buf, ssize_t cnt)
{
	struct mtk_bm_pool *pool = data;
	unsigned long val;

	if (!pool) {
		pr_err("Failed to write debugfs bm buffcnt\n");
	} else {
		if (kstrtoul(buf, 10, &val)) {
			pr_err("Failed to read debugfs file\n");
			return -1;
		}
		mtk_bm_adjust_threshold(pool, val);
	}

	return cnt;
}

MTK_DBGFS(threshold, mtk_bm_threshold_read, mtk_bm_threshold_write);

static struct dentry *mtk_bm_debugfs_init(struct mtk_md_dev *mdev)
{
	struct dentry *subdir;

	subdir = mtk_dbgfs_create_dir(mtk_get_dev_dentry(mdev), "mtk_bm");
	if (IS_ERR_OR_NULL(subdir)) {
		MTK_WARN(mdev, "Failed to create bm debugfs dir\n");
		return NULL;
	}

	return subdir;
}

static void mtk_bm_pool_debugfs_init(struct dentry *parent, struct mtk_bm_pool *pool,
				     const char *name)
{
	pool->pool_dentry = mtk_dbgfs_create_dir(parent, name);
	if (IS_ERR_OR_NULL(pool->pool_dentry)) {
		pr_warn("Failed to create bm debugfs dir\n");
	} else {
		mtk_dbgfs_create_file(pool->pool_dentry, &mtk_dbgfs_dump, pool);
		mtk_dbgfs_create_file(pool->pool_dentry, &mtk_dbgfs_threshold, pool);
	}
}

static void mtk_bm_debugfs_remove(struct dentry *m_dentry)
{
	if (IS_ERR_OR_NULL(m_dentry))
		return;
	mtk_dbgfs_remove(m_dentry);
}

#else

static struct dentry *mtk_bm_debugfs_init(struct mtk_md_dev *mdev)
{
	return NULL;
}

static void mtk_bm_pool_debugfs_init(struct dentry *parent, struct mtk_bm_pool *pool,
				     const char *name)
{
}

static void mtk_bm_debugfs_remove(struct dentry *m_dentry)
{
}

#endif

/**
 * mtk_bm_init() - Init bm control block
 * @mdev: pointer to mtk_md_dev
 *
 * Return: return value is 0 on success, a negative error code on failure.
 */
int mtk_bm_init(struct mtk_md_dev *mdev)
{
	char wq_name[MTK_WQ_NAME_LEN];
	struct mtk_bm_ctrl *bm;

	bm = devm_kzalloc(mdev->dev, sizeof(*bm), GFP_KERNEL);
	if (!bm)
		return -ENOMEM;

	bm->list_cache_pool = kmem_cache_create(mdev->dev_str, sizeof(struct mtk_buff), 0, 0, NULL);
	if (unlikely(!bm->list_cache_pool))
		goto err_free_buf;

	snprintf(wq_name, sizeof(wq_name), "mtk_pool_reload_work_h_%s", mdev->dev_str);
	bm->pool_reload_workqueue_h = alloc_workqueue(wq_name,
						      WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!bm->pool_reload_workqueue_h)
		goto err_destroy_cache_pool;

	snprintf(wq_name, sizeof(wq_name), "mtk_pool_reload_work_l_%s", mdev->dev_str);
	bm->pool_reload_workqueue_l = alloc_workqueue(wq_name,
						      WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
	if (!bm->pool_reload_workqueue_l)
		goto err_destroy_wq;

	mutex_init(&bm->pool_list_mtx);
	INIT_LIST_HEAD(&bm->pool_list);

	bm->m_ops[MTK_BUFF_SKB] = &skb_buf_ops;
	bm->m_ops[MTK_BUFF_PAGE] = &page_buf_ops;

	bm->dentry = mtk_bm_debugfs_init(mdev);
	mdev->bm_ctrl = bm;

	return 0;

err_destroy_wq:
	flush_workqueue(bm->pool_reload_workqueue_h);
	destroy_workqueue(bm->pool_reload_workqueue_h);
err_destroy_cache_pool:
	kmem_cache_destroy(bm->list_cache_pool);
err_free_buf:
	devm_kfree(mdev->dev, bm);
	return -ENOMEM;
}

/**
 * mtk_bm_pool_create() - Create a buffer pool
 * @mdev: pointer to mtk_md_dev
 * @type: pool type
 * @buff_size: the buffer size
 * @buff_cnt: the buffer count
 * @prio: the priority of reload work
 *
 * Return: return value is a buffer pool on success, a NULL pointer on failure.
 */
struct mtk_bm_pool *mtk_bm_pool_create(struct mtk_md_dev *mdev,
				       enum mtk_buff_type type, unsigned int buff_size,
				       unsigned int buff_cnt, unsigned int prio,
				       const char *fmt, ...)
{
	struct mtk_bm_ctrl *bm = mdev->bm_ctrl;
	char name[MAX_POOL_NAME_SIZE];
	struct mtk_bm_pool *pool;
	va_list args;

	pool = devm_kzalloc(mdev->dev, sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	va_start(args, fmt);
	vsnprintf(name, sizeof(name), fmt, args);
	va_end(args);
	memcpy(pool->pool_name, name, sizeof(name));
	pool->type = type;
	pool->buff_size = buff_size;
	pool->buff_cnt = buff_cnt;
	pool->pool_id = bm->pool_seq++;
	pool->threshold = pool->buff_cnt - pool->buff_cnt / MTK_RELOAD_TH;
	pool->dev = mdev->dev;

	if (prio == MTK_BM_HIGH_PRIO)
		pool->reload_workqueue = bm->pool_reload_workqueue_h;
	else
		pool->reload_workqueue = bm->pool_reload_workqueue_l;
	pool->prio = prio;
	pool->bm_ctrl = bm;

	spin_lock_init(&pool->lock);
	pool->ops = bm->m_ops[pool->type];
	if (!pool->ops)
		goto out;
	INIT_WORK(&pool->reload_work, pool->ops->pool_reload);
	if (pool->ops->pool_create(pool))
		goto out;
	atomic_set(&pool->work_stop, 0);
	queue_work(pool->reload_workqueue, &pool->reload_work);

	if (bm->dentry)
		mtk_bm_pool_debugfs_init(bm->dentry, pool, name);

	mutex_lock(&bm->pool_list_mtx);
	list_add_tail(&pool->entry, &bm->pool_list);
	bm->pool_cnt++;
	mutex_unlock(&bm->pool_list_mtx);

	return pool;

out:
	MTK_ERR(mdev, "Failed to create bm pool\n");
	devm_kfree(mdev->dev, pool);
	return NULL;
}

/**
 * mtk_bm_alloc() - Alloc a block of buffer from bm pool
 * @pool: the buffer pool
 *
 * Return: return value is a block of buffer from bm pool on success, a NULL
 *         pointer on failure.
 */
void *mtk_bm_alloc(struct mtk_bm_pool *pool)
{
	return pool->ops->buff_alloc(pool);
}

/**
 * mtk_bm_free() - Free a block of buffer to bm pool
 * @pool: the buffer pool
 * @data: the buffer need to free to pool
 *
 * Return: return value is 0 on success, a negative error code on failure.
 */
int mtk_bm_free(struct mtk_bm_pool *pool, void *data)
{
	if (!data)
		return -EINVAL;

	pool->ops->buff_free(pool, data);

	return 0;
}

/**
 * mtk_bm_pool_destroy() - Destroy a buffer pool
 * @mdev: pointer to mtk_md_dev
 * @pool: the buffer pool need to destroy
 *
 * Context: we must stop calling alloc/free before this function is called.
 * Return: return value is 0 on success, a negative error code on failure.
 */
int mtk_bm_pool_destroy(struct mtk_md_dev *mdev, struct mtk_bm_pool *pool)
{
	struct mtk_bm_ctrl *bm = mdev->bm_ctrl;

	atomic_set(&pool->work_stop, 1);
	cancel_work_sync(&pool->reload_work);
	spin_lock_bh(&pool->lock);
	pool->curr_cnt = 0;
	spin_unlock_bh(&pool->lock);

	mtk_bm_debugfs_remove(pool->pool_dentry);

	mutex_lock(&bm->pool_list_mtx);
	list_del(&pool->entry);
	bm->pool_cnt--;
	mutex_unlock(&bm->pool_list_mtx);

	pool->ops->pool_destroy(pool);

	devm_kfree(mdev->dev, pool);
	return 0;
}

/**
 * mtk_bm_exit() - Deinit bm control block
 * @mdev: pointer to mtk_md_dev
 *
 * Return: return value is 0 on success, a negative error code on failure.
 */
int mtk_bm_exit(struct mtk_md_dev *mdev)
{
	struct mtk_bm_ctrl *bm = mdev->bm_ctrl;

	flush_workqueue(bm->pool_reload_workqueue_h);
	destroy_workqueue(bm->pool_reload_workqueue_h);
	flush_workqueue(bm->pool_reload_workqueue_l);
	destroy_workqueue(bm->pool_reload_workqueue_l);

	if (unlikely(!list_empty(&bm->pool_list)))
		MTK_BUG(mdev);

	mtk_bm_debugfs_remove(bm->dentry);

	kmem_cache_destroy(bm->list_cache_pool);
	devm_kfree(mdev->dev, bm);

	return 0;
}
