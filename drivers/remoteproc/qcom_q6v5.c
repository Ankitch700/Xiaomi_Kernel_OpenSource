// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm Peripheral Image Loader for Q6V5
 *
 * Copyright (C) 2016-2018 Linaro Ltd.
 * Copyright (C) 2014 Sony Mobile Communications AB
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interconnect.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/soc/qcom/qcom_aoss.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/remoteproc.h>
#include <linux/delay.h>
#include <asm/timex.h>

#include "qcom_common.h"
/* XIAOMI-CHANGE-CRASH (3025407) start: crash history */
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */
#include "qcom_q6v5.h"
#include <trace/events/rproc_qcom.h>

#define Q6V5_LOAD_STATE_MSG_LEN	64
#define Q6V5_PANIC_DELAY_MS	200
/* XIAOMI-CHANGE-CRASH (3025407) start: crash history */
#define MAX_SSR_REASON_LEN	256U
#define MAX_CRASH_REASON    256

static int crash_num = 0;
static char last_modem_sfr_reason[MAX_SSR_REASON_LEN] = "none";
static struct proc_dir_entry *last_modem_sfr_entry = NULL;
static char modem_crash_reason[MAX_CRASH_REASON][MAX_SSR_REASON_LEN]={"0"};
/* modem crash history entry */
static int last_modem_sfr_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", last_modem_sfr_reason);
	return 0;
}

static int last_modem_sfr_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, last_modem_sfr_proc_show, NULL);
}

static ssize_t last_modem_sfr_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    char *data = kmalloc(count + 1, GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }

    if (copy_from_user(data, buffer, count)) {
        kfree(data);
        return -EFAULT;
    }

    data[count] = '\0';

    pr_err("subsystem failure reason: %s\n", data);

    kfree(data);
    return count;
}

static const struct proc_ops last_modem_sfr_file_ops = {
	//.owner   = THIS_MODULE,
	.proc_open    = last_modem_sfr_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
	.proc_write   = last_modem_sfr_proc_write,
};


/*  modem power feature start ********************/
struct sleep_stats {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};
static struct proc_dir_entry *modem_sleep_stats_sfr_entry = NULL;
static int modem_sleep_stats_proc_show(struct seq_file *m, void *v)
{
	struct sleep_stats *stat;
	u64 accumulated = 0;
	stat = qcom_smem_get(1, 605, NULL); //refer to static struct subsystem_data subsystems[] 
	if (IS_ERR(stat))
		return PTR_ERR(stat);

	accumulated = stat->accumulated;
	/*
	 * If a subsystem is in sleep when reading the sleep stats adjust
	 * the accumulated sleep duration to show actual sleep time.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated += arch_timer_read_counter()
			       - stat->last_entered_at;

	seq_printf(m, "Count = %u\n", stat->count);
	seq_printf(m, "Last Entered At = %llu\n", stat->last_entered_at);
	seq_printf(m, "Last Exited At = %llu\n", stat->last_exited_at);
	seq_printf(m, "Accumulated Duration = %llu\n", accumulated);
	return 0;
}

static int modem_sleep_stats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, modem_sleep_stats_proc_show, NULL);
}

static const struct proc_ops modem_sleep_stats_file_ops = {
	//.owner   = THIS_MODULE,
	.proc_open    = modem_sleep_stats_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/*  modem power feature end ********************/

/*  apss power feature start ********************/
static struct proc_dir_entry *apss_sleep_stats_sfr_entry = NULL;
static int apss_sleep_stats_proc_show(struct seq_file *m, void *v)
{
	struct sleep_stats *stat;
	u64 accumulated = 0;
	stat = qcom_smem_get(-1, 631, NULL); //refer to static struct subsystem_data subsystems[] 
	if (IS_ERR(stat))
		return PTR_ERR(stat);

	accumulated = stat->accumulated;
	/*
	 * If a subsystem is in sleep when reading the sleep stats adjust
	 * the accumulated sleep duration to show actual sleep time.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated += arch_timer_read_counter()
			       - stat->last_entered_at;

	seq_printf(m, "Count = %u\n", stat->count);
	seq_printf(m, "Last Entered At = %llu\n", stat->last_entered_at);
	seq_printf(m, "Last Exited At = %llu\n", stat->last_exited_at);
	seq_printf(m, "Accumulated Duration = %llu\n", accumulated);
	return 0;
}

static int apss_sleep_stats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, apss_sleep_stats_proc_show, NULL);
}

static const struct proc_ops apss_sleep_stats_file_ops = {
	//.owner   = THIS_MODULE,
	.proc_open    = apss_sleep_stats_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
/*  apss power feature end ********************/
/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */

static int q6v5_load_state_toggle(struct qcom_q6v5 *q6v5, bool enable)
{
	int ret;

	if (!q6v5->qmp)
		return 0;

	ret = qmp_send(q6v5->qmp, "{class: image, res: load_state, name: %s, val: %s}",
		       q6v5->load_state, enable ? "on" : "off");
	if (ret)
		dev_err(q6v5->dev, "failed to toggle load state\n");

	return ret;
}

/**
 * qcom_q6v5_prepare() - reinitialize the qcom_q6v5 context before start
 * @q6v5:	reference to qcom_q6v5 context to be reinitialized
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_prepare(struct qcom_q6v5 *q6v5)
{
	int ret;

	ret = icc_set_bw(q6v5->path, UINT_MAX, UINT_MAX);
	if (ret < 0) {
		dev_err(q6v5->dev, "failed to set bandwidth request\n");
		return ret;
	}

	ret = q6v5_load_state_toggle(q6v5, true);
	if (ret) {
		icc_set_bw(q6v5->path, 0, 0);
		return ret;
	}
	/* XIAOMI-CHANGE-CRASH (3025407) start: crash history */
	if (last_modem_sfr_entry == NULL) {
		last_modem_sfr_entry = proc_create("last_mcrash", 0777, NULL, &last_modem_sfr_file_ops);
	}
	if (!last_modem_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry last_mcrash\n");
	}

	reinit_completion(&q6v5->start_done);
	if (modem_sleep_stats_sfr_entry == NULL) {
		modem_sleep_stats_sfr_entry = proc_create("modem_sleep_stats", S_IFREG | S_IRUGO, NULL, &modem_sleep_stats_file_ops);
	}
	if (!modem_sleep_stats_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry modem_sleep_stats\n");
	}
	/*  modem power feature end ********************/
        /*  apss power feature start ********************/
	if (apss_sleep_stats_sfr_entry == NULL) {
		apss_sleep_stats_sfr_entry = proc_create("apss_sleep_stats", S_IFREG | S_IRUGO, NULL, &apss_sleep_stats_file_ops);
	}
	if (!apss_sleep_stats_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry apss_sleep_stats\n");
	}
	/*  apss power feature end ********************/
	/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */

	reinit_completion(&q6v5->stop_done);

	q6v5->running = true;
	q6v5->handover_issued = false;

	enable_irq(q6v5->handover_irq);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_prepare);

/**
 * qcom_q6v5_unprepare() - unprepare the qcom_q6v5 context after stop
 * @q6v5:	reference to qcom_q6v5 context to be unprepared
 *
 * Return: 0 on success, 1 if handover hasn't yet been called
 */
int qcom_q6v5_unprepare(struct qcom_q6v5 *q6v5)
{
	disable_irq(q6v5->handover_irq);
	q6v5_load_state_toggle(q6v5, false);

	/* Disable interconnect vote, in case handover never happened */
	icc_set_bw(q6v5->path, 0, 0);

	return !q6v5->handover_issued;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_unprepare);

void qcom_q6v5_register_ssr_subdev(struct qcom_q6v5 *q6v5, struct rproc_subdev *ssr_subdev)
{
	q6v5->ssr_subdev = ssr_subdev;
}
EXPORT_SYMBOL(qcom_q6v5_register_ssr_subdev);

static void qcom_q6v5_crash_handler_work(struct work_struct *work)
{
	struct qcom_q6v5 *q6v5 = container_of(work, struct qcom_q6v5, crash_handler);
	struct rproc *rproc = q6v5->rproc;
	struct rproc_subdev *subdev;
	int votes;

	mutex_lock(&rproc->lock);
	votes = atomic_read(&rproc->power);
	if (votes == 0 || q6v5->crash_seq != q6v5->seq) {
		mutex_unlock(&rproc->lock);
		return;
	}

	rproc->state = RPROC_CRASHED;
	list_for_each_entry_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->stop)
			subdev->stop(subdev, true);
	}

	msleep(100);
	/*
	 * Temporary workaround until ramdump userspace application calls
	 * sync() and fclose() on attempting the dump.
	 */
	panic("Panicking, remoteproc %s crashed\n", q6v5->rproc->name);
	mutex_unlock(&rproc->lock);
}

static irqreturn_t q6v5_wdog_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;
	size_t len;
	char *msg;
	int temp_num;    // XIAOMI-CHANGE-CRASH (3025407): crash history

	/* Sometimes the stop triggers a watchdog rather than a stop-ack */
	if (!q6v5->running) {
		dev_info(q6v5->dev, "received wdog irq while q6 is offline\n");
		complete(&q6v5->stop_done);
		return IRQ_HANDLED;
	}

	dev_err(q6v5->dev, "rproc crash at cycle:%llu, recovery state: %s\n",
		get_cycles(),
		q6v5->rproc->recovery_disabled ? "disabled and lead to device crash" :
		"enabled and kick recovery process");

	q6v5->crash_seq = q6v5->seq;
	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, q6v5->crash_reason, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0]) {
		dev_err(q6v5->dev, "watchdog received: %s\n", msg);
		pr_err("%s subsystem failure reason: %s. \n", dev_name(q6v5->dev), msg);
		/* XIAOMI-CHANGE-CRASH (3025407) start: crash history */
		strlcpy(last_modem_sfr_reason, msg, MAX_SSR_REASON_LEN);
		strlcpy(modem_crash_reason[crash_num++], msg, MAX_SSR_REASON_LEN);
	} else {
		dev_err(q6v5->dev, "watchdog without message\n");
		pr_err("%s subsystem failure reason: watchdog without message. \n", dev_name(q6v5->dev));
	}

	if (q6v5->crash_stack) {
		msg = qcom_smem_get(q6v5->smem_host_id, q6v5->crash_stack, &len);
		if (!IS_ERR(msg) && len > 0 && msg[0]) {
			dev_err(q6v5->dev, "%s\n", msg);
			pr_err("%s subsystem failure reason from crash_stack: %s. \n", dev_name(q6v5->dev), msg);
		}
	}

	q6v5->running = false;

	temp_num = crash_num-1;
	while((temp_num--) && crash_num && (crash_num >= 10))
	{
		if(!strcmp(modem_crash_reason[crash_num-1],modem_crash_reason[temp_num]))
		{
			crash_num = crash_num-1;
			q6v5->rproc->dump_conf = RPROC_COREDUMP_DISABLED;
			pr_err("qcom_q6v5.c :in compare, same crash reason to skip dump");
			break;
		}else if (!temp_num){
			q6v5->rproc->dump_conf = RPROC_COREDUMP_ENABLED;
			break;
		}
	}
	/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */
	dev_err(q6v5->dev, "rproc coredump state: %d\n", q6v5->rproc->dump_conf);
	dev_err(q6v5->dev, "rproc recovery state: %s\n",
		q6v5->rproc->recovery_disabled ?
		"disabled and lead to device crash" :
		"enabled and kick reovery process");

	trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_wdog", msg);
	if (q6v5->ssr_subdev)
		qcom_notify_early_ssr_clients(q6v5->ssr_subdev);

	if (q6v5->rproc->recovery_disabled)
		queue_work(system_unbound_wq, &q6v5->crash_handler);
	else
		rproc_report_crash(q6v5->rproc, RPROC_WATCHDOG);

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_fatal_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;
	size_t len;
	char *msg;
	int temp_num;    // XIAOMI-CHANGE-CRASH (3025407): crash history

	if (!q6v5->running) {
		dev_info(q6v5->dev, "received fatal irq while q6 is offline\n");
		return IRQ_HANDLED;
	}

	dev_err(q6v5->dev, "rproc crash at cycle:%llu, recovery state: %s\n",
		get_cycles(),
		q6v5->rproc->recovery_disabled ? "disabled and lead to device crash" :
		"enabled and kick recovery process");

	q6v5->crash_seq = q6v5->seq;
	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, q6v5->crash_reason, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0]) {
		dev_err(q6v5->dev, "fatal error received: %s\n", msg);
		pr_err("%s subsystem failure reason: %s. \n", dev_name(q6v5->dev), msg);
		/* XIAOMI-CHANGE-CRASH (3025407) start: crash history */
		strlcpy(last_modem_sfr_reason, msg, MAX_SSR_REASON_LEN);
		strlcpy(modem_crash_reason[crash_num++], msg, MAX_SSR_REASON_LEN);
	} else {
		dev_err(q6v5->dev, "fatal error without message\n");
		pr_err("%s subsystem failure reason: fatal error without message. \n", dev_name(q6v5->dev));
	}

	if (q6v5->crash_stack) {
		msg = qcom_smem_get(q6v5->smem_host_id, q6v5->crash_stack, &len);
		if (!IS_ERR(msg) && len > 0 && msg[0]) {
			dev_err(q6v5->dev, "%s\n", msg);
			pr_err("%s subsystem failure reason from crash_stack: %s. \n", dev_name(q6v5->dev), msg);
		}
	}

	q6v5->running = false;
	temp_num = crash_num-1;
	while((temp_num--) && crash_num && (crash_num >= 10))
	{
		if(!strcmp(modem_crash_reason[crash_num-1],modem_crash_reason[temp_num]))
		{
			crash_num = crash_num-1;
			q6v5->rproc->dump_conf = RPROC_COREDUMP_DISABLED;
			pr_err("qcom_q6v5.c :in compare, same crash reason to skip dump");
			break;
		}else if (!temp_num){
			q6v5->rproc->dump_conf = RPROC_COREDUMP_ENABLED;
			break;
		}
	}
	/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */
	dev_err(q6v5->dev, "rproc coredump state: %d\n", q6v5->rproc->dump_conf);
	dev_err(q6v5->dev, "rproc recovery state: %s\n",
		q6v5->rproc->recovery_disabled ? "disabled and lead to device crash" :
		"enabled and kick reovery process");

	trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_fatal", msg);
	if (q6v5->ssr_subdev)
		qcom_notify_early_ssr_clients(q6v5->ssr_subdev);

	if (q6v5->rproc->recovery_disabled)
		queue_work(system_unbound_wq, &q6v5->crash_handler);
	else
		rproc_report_crash(q6v5->rproc, RPROC_FATAL_ERROR);

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_ready_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	complete(&q6v5->start_done);

	return IRQ_HANDLED;
}

/**
 * qcom_q6v5_wait_for_start() - wait for remote processor start signal
 * @q6v5:	reference to qcom_q6v5 context
 * @timeout:	timeout to wait for the event, in jiffies
 *
 * qcom_q6v5_unprepare() should not be called when this function fails.
 *
 * Return: 0 on success, -ETIMEDOUT on timeout
 */
int qcom_q6v5_wait_for_start(struct qcom_q6v5 *q6v5, int timeout)
{
	int ret;

	ret = wait_for_completion_timeout(&q6v5->start_done, timeout);
	if (!ret)
		disable_irq(q6v5->handover_irq);

	return !ret ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_wait_for_start);

static irqreturn_t q6v5_handover_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	if (q6v5->handover)
		q6v5->handover(q6v5);

	icc_set_bw(q6v5->path, 0, 0);

	q6v5->handover_issued = true;

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_stop_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	complete(&q6v5->stop_done);

	return IRQ_HANDLED;
}

/**
 * qcom_q6v5_request_stop() - request the remote processor to stop
 * @q6v5:	reference to qcom_q6v5 context
 * @sysmon:	reference to the remote's sysmon instance, or NULL
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_request_stop(struct qcom_q6v5 *q6v5, struct qcom_sysmon *sysmon)
{
	int ret;

	/*  modem power feature start ********************/
	if (modem_sleep_stats_sfr_entry) {
		remove_proc_entry("modem_sleep_stats", NULL);
		modem_sleep_stats_sfr_entry = NULL;
	}
	/*  modem power feature end ********************/

	/*  apss power feature start ********************/
	if (apss_sleep_stats_sfr_entry) {
		remove_proc_entry("apss_sleep_stats", NULL);
		apss_sleep_stats_sfr_entry = NULL;
	}
	/*  apss power feature end ********************/
	/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */
	q6v5->running = false;

	/* Don't perform SMP2P dance if remote isn't running */
	if (qcom_sysmon_shutdown_acked(sysmon) || (q6v5->rproc->state != RPROC_RUNNING))
		return 0;

	qcom_smem_state_update_bits(q6v5->state,
				    BIT(q6v5->stop_bit), BIT(q6v5->stop_bit));

	ret = wait_for_completion_timeout(&q6v5->stop_done, 5 * HZ);

	qcom_smem_state_update_bits(q6v5->state, BIT(q6v5->stop_bit), 0);

	return ret == 0 ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_request_stop);

/**
 * qcom_q6v5_panic() - panic handler to invoke a stop on the remote
 * @q6v5:	reference to qcom_q6v5 context
 *
 * Set the stop bit and sleep in order to allow the remote processor to flush
 * its caches etc for post mortem debugging.
 *
 * Return: 200ms
 */
unsigned long qcom_q6v5_panic(struct qcom_q6v5 *q6v5)
{
	qcom_smem_state_update_bits(q6v5->state,
				    BIT(q6v5->stop_bit), BIT(q6v5->stop_bit));

	return Q6V5_PANIC_DELAY_MS;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_panic);

/**
 * qcom_q6v5_init() - initializer of the q6v5 common struct
 * @q6v5:	handle to be initialized
 * @pdev:	platform_device reference for acquiring resources
 * @rproc:	associated remoteproc instance
 * @crash_reason: SMEM id for crash reason string, or 0 if none
 * @load_state: load state resource string
 * @handover:	function to be called when proxy resources should be released
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_init(struct qcom_q6v5 *q6v5, struct platform_device *pdev,
		   struct rproc *rproc,  int crash_reason, int crash_stack,
		   unsigned int smem_host_id, const char *load_state,
		   void (*handover)(struct qcom_q6v5 *q6v5))
{
	int ret;

	/* XIAOMI-CHANGE-CRASH (3025407) start: crash history */
	if (last_modem_sfr_entry == NULL) {
		last_modem_sfr_entry = proc_create("last_mcrash", 0777, NULL, &last_modem_sfr_file_ops);
	}
	if (!last_modem_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry last_mcrash\n");
	}

	/*  modem power feature start ********************/
	if (modem_sleep_stats_sfr_entry == NULL) {
		modem_sleep_stats_sfr_entry = proc_create("modem_sleep_stats", S_IFREG | S_IRUGO, NULL, &modem_sleep_stats_file_ops);
	}
	if (!modem_sleep_stats_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry modem_sleep_stats\n");
	}
	/*  modem power feature end ********************/
	/*  apss power feature start ********************/
	if (apss_sleep_stats_sfr_entry == NULL) {
		apss_sleep_stats_sfr_entry = proc_create("apss_sleep_stats", S_IFREG | S_IRUGO, NULL, &apss_sleep_stats_file_ops);
	}
	if (!apss_sleep_stats_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry apss_sleep_stats\n");
	}
	/*  apss power feature end ********************/
	/* XIAOMI-CHANGE-CRASH (3025407) end: crash history */
	q6v5->rproc = rproc;
	q6v5->dev = &pdev->dev;
	q6v5->crash_reason = crash_reason;
	q6v5->crash_stack = crash_stack;
	q6v5->smem_host_id = smem_host_id;
	q6v5->handover = handover;
	q6v5->ssr_subdev = NULL;

	init_completion(&q6v5->start_done);
	init_completion(&q6v5->stop_done);

	q6v5->wdog_irq = platform_get_irq_byname(pdev, "wdog");
	if (q6v5->wdog_irq < 0)
		return q6v5->wdog_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->wdog_irq,
					NULL, q6v5_wdog_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 wdog", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire wdog IRQ\n");
		return ret;
	}

	q6v5->fatal_irq = platform_get_irq_byname(pdev, "fatal");
	if (q6v5->fatal_irq < 0)
		return q6v5->fatal_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->fatal_irq,
					NULL, q6v5_fatal_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 fatal", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire fatal IRQ\n");
		return ret;
	}

	q6v5->ready_irq = platform_get_irq_byname(pdev, "ready");
	if (q6v5->ready_irq < 0)
		return q6v5->ready_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->ready_irq,
					NULL, q6v5_ready_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 ready", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire ready IRQ\n");
		return ret;
	}

	q6v5->handover_irq = platform_get_irq_byname(pdev, "handover");
	if (q6v5->handover_irq < 0)
		return q6v5->handover_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->handover_irq,
					NULL, q6v5_handover_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 handover", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire handover IRQ\n");
		return ret;
	}
	disable_irq(q6v5->handover_irq);

	q6v5->stop_irq = platform_get_irq_byname(pdev, "stop-ack");
	if (q6v5->stop_irq < 0)
		return q6v5->stop_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->stop_irq,
					NULL, q6v5_stop_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 stop", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire stop-ack IRQ\n");
		return ret;
	}

	q6v5->state = devm_qcom_smem_state_get(&pdev->dev, "stop", &q6v5->stop_bit);
	if (IS_ERR(q6v5->state)) {
		dev_err(&pdev->dev, "failed to acquire stop state\n");
		return PTR_ERR(q6v5->state);
	}

	q6v5->load_state = devm_kstrdup_const(&pdev->dev, load_state, GFP_KERNEL);
	q6v5->qmp = qmp_get(&pdev->dev);
	if (IS_ERR(q6v5->qmp)) {
		if (PTR_ERR(q6v5->qmp) != -ENODEV)
			return dev_err_probe(&pdev->dev, PTR_ERR(q6v5->qmp),
					     "failed to acquire load state\n");
		q6v5->qmp = NULL;
	} else if (!q6v5->load_state) {
		if (!load_state)
			dev_err(&pdev->dev, "load state resource string empty\n");

		qmp_put(q6v5->qmp);
		return load_state ? -ENOMEM : -EINVAL;
	}

	q6v5->path = devm_of_icc_get(&pdev->dev, NULL);
	if (IS_ERR(q6v5->path))
		return dev_err_probe(&pdev->dev, PTR_ERR(q6v5->path),
				     "failed to acquire interconnect path\n");

	INIT_WORK(&q6v5->crash_handler, qcom_q6v5_crash_handler_work);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_init);

/**
 * qcom_q6v5_deinit() - deinitialize the q6v5 common struct
 * @q6v5:	reference to qcom_q6v5 context to be deinitialized
 */
void qcom_q6v5_deinit(struct qcom_q6v5 *q6v5)
{
	qmp_put(q6v5->qmp);
}
EXPORT_SYMBOL_GPL(qcom_q6v5_deinit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Peripheral Image Loader for Q6V5");
