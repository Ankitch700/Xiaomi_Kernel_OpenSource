/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2017, Linaro Ltd
 */

#ifndef __QCOM_GLINK_NATIVE_H__
#define __QCOM_GLINK_NATIVE_H__

#include <linux/types.h>

#define GLINK_FEATURE_INTENT_REUSE	BIT(0)
#define GLINK_FEATURE_MIGRATION		BIT(1)
#define GLINK_FEATURE_TRACER_PKT	BIT(2)

/**
 * rpmsg rx callback return definitions
 * @RPMSG_HANDLED: rpmsg user is done processing data, framework can free the
 *                 resources related to the buffer
 * @RPMSG_DEFER:   rpmsg user is not done processing data, framework will hold
 *                 onto resources related to the buffer until rpmsg_rx_done is
 *                 called. User should check their endpoint to see if rx_done
 *                 is a supported operation.
 */
#define RPMSG_HANDLED	0
#define RPMSG_DEFER	1

struct qcom_glink_pipe {
	size_t length;

	size_t (*avail)(struct qcom_glink_pipe *glink_pipe);

	void (*peek)(struct qcom_glink_pipe *glink_pipe, void *data,
		     unsigned int offset, size_t count);
	void (*advance)(struct qcom_glink_pipe *glink_pipe, size_t count);

	void (*write)(struct qcom_glink_pipe *glink_pipe,
		      const void *hdr, size_t hlen,
		      const void *data, size_t dlen);
	void (*kick)(struct qcom_glink_pipe *glink_pipe);
};

struct device;
struct qcom_glink;
extern const struct dev_pm_ops glink_native_pm_ops;

struct qcom_glink *qcom_glink_native_probe(struct device *dev,
					   unsigned long features,
					   struct qcom_glink_pipe *rx,
					   struct qcom_glink_pipe *tx,
					   bool intentless);
int qcom_glink_native_start(struct qcom_glink *glink);
void qcom_glink_native_remove(struct qcom_glink *glink);
void qcom_glink_native_rx(struct qcom_glink *glink);

/* These operations are temporarily exposing deferred freeing interfaces */
bool qcom_glink_rx_done_supported(struct rpmsg_endpoint *ept);
int qcom_glink_rx_done(struct rpmsg_endpoint *ept, void *data);

#endif
