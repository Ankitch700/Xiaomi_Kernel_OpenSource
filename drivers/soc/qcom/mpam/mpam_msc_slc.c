// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "qcom_mpam_slc: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/scmi_protocol.h>
#include <soc/qcom/mpam_msc.h>
#include <soc/qcom/mpam_slc.h>

#define QCOM_SLC_MPAM_SCMI_STR	0x534c434d50414d //SLCMPAM

enum mpam_slc_set_param_ids {
	PARAM_SET_CACHE_PARTITION_MSC = 1,
	PARAM_RESET_CACHE_PARTITION_MSC = 2,
};

enum mpam_slc_get_param_ids {
	PARAM_GET_CLIENT_INFO_MSC = 1,
	PARAM_GET_CACHE_CAPABILITY_MSC = 2,
	PARAM_GET_CACHE_PARTITION_MSC = 3,
};

static uint16_t slc_client_id[] = {
	APPS,
	GPU,
	NSP,
	SLC_CLIENT_MAX,
};

static int mpam_msc_slc_set_params(struct device *dev, void *param, int param_len, uint32_t param_id)
{
	int ret = -EPERM;
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	/* Interface mecahnism for HLOS to control software,
	* SCMI incase of sun.
	*/
	if ((qcom_msc->qcom_msc_id.qcom_msc_type == SLC) && (qcom_msc->scmi_ops))
		ret = qcom_msc->scmi_ops->set_param(qcom_msc->ph, param, QCOM_SLC_MPAM_SCMI_STR,
				param_id, param_len);

	return ret;
}

static int mpam_msc_slc_get_params(struct device *dev, void *param_in, int param_in_len, void *param_out,
				int param_out_len, uint32_t param_id)
{
	int ret = -EPERM;
	uint8_t buf[32];
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	/* Interface mecahnism for HLOS to control software,
	* SCMI incase of sun.
	*/
	if ((qcom_msc->qcom_msc_id.qcom_msc_type == SLC) && (qcom_msc->scmi_ops)) {
		if (param_in_len && param_in_len <= sizeof(buf)) {
			memcpy(buf, param_in, param_in_len);
			ret = qcom_msc->scmi_ops->get_param(qcom_msc->ph, buf, QCOM_SLC_MPAM_SCMI_STR, param_id,
					param_in_len, param_out_len);
		}

		if (!ret)
			memcpy(param_out, buf, param_out_len);
	}

	return ret;
}

static struct qcom_mpam_msc* slc_capability_check(struct device *dev, struct msc_query *query)
{
	struct qcom_mpam_msc *qcom_msc;
	struct qcom_slc_capability *slc_capability;
	struct slc_client_capability *slc_client_cap;
	int client_idx, partid_idx;

	if (query->qcom_msc_id.qcom_msc_type != SLC) {
		dev_err(dev, "Invalid Client type, expected %d, query was for %d\n", SLC,
				query->qcom_msc_id.qcom_msc_type );
		return NULL;
	}

	client_idx = query->client_id;
	partid_idx = query->part_id;

	if (client_idx >= SLC_CLIENT_MAX) {
		dev_err(dev, "Invalid Client ID %d\n", client_idx);
		return NULL;
	}

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return NULL;

	slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &(slc_capability->slc_client_cap[client_idx]);
#if 0
	/* On query, it has to return from here */
	if (slc_client_cap->client_info.num_part_id == 0)
		return qcom_msc;
#endif

	if (slc_client_cap->enabled == false) {
		dev_err(dev, "Client not enabled for configuration %d\n", client_idx);
		return NULL;
	}

	if (partid_idx >= slc_client_cap->client_info.num_part_id) {
		dev_err(dev, "Invalid PART id %d\n", partid_idx);
		return NULL;
	}

	return qcom_msc;
}

static int slc_set_cache_partition(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct slc_parid_config slc_part_config;
	struct qcom_slc_capability *slc_capability;
	struct slc_client_capability *slc_client_cap;
	struct slc_partid_capability *slc_partid_cap;
	int client_idx, partid_idx, gear_idx;

	memcpy(&slc_part_config.query, msc_partid, sizeof(struct msc_query));
	memcpy(&slc_part_config.gear_config, msc_partconfig, sizeof(struct qcom_slc_gear_val));

	qcom_msc = slc_capability_check(dev, &slc_part_config.query);
	if (qcom_msc == NULL)
		return -EINVAL;

	client_idx = slc_part_config.query.client_id;
	partid_idx = slc_part_config.query.part_id;
	slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &(slc_capability->slc_client_cap[client_idx]);
	slc_partid_cap = &(slc_client_cap->slc_partid_cap[partid_idx]);

	for (gear_idx = 0; gear_idx < slc_partid_cap->num_gears; gear_idx++) {
		if (slc_partid_cap->part_id_gears[gear_idx] == slc_part_config.gear_config.gear_val)
			break;
	}

	if (gear_idx == slc_partid_cap->num_gears) {
		dev_err(dev, "GEAR config not valid!\n");
		return -EINVAL;
	}

	return mpam_msc_slc_set_params(dev, &slc_part_config, sizeof(struct slc_parid_config),
			PARAM_SET_CACHE_PARTITION_MSC);
}

static int slc_reset_cache_partition(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct slc_parid_config slc_part_config;

	memcpy(&slc_part_config.query, msc_partid, sizeof(struct msc_query));
	memcpy(&slc_part_config.gear_config, msc_partconfig, sizeof(struct qcom_slc_gear_val));

	qcom_msc = slc_capability_check(dev, &slc_part_config.query);
	if (qcom_msc == NULL)
		return -EINVAL;

	return mpam_msc_slc_set_params(dev, &slc_part_config, sizeof(struct slc_parid_config),
			PARAM_RESET_CACHE_PARTITION_MSC);
}

static int slc_client_query(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct slc_client_info *client_info;

	query = (struct msc_query *)msc_partid;
	client_info = (struct slc_client_info *) msc_partconfig;

#if 0
	qcom_msc = slc_capability_check(dev, query);
#else
	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
#endif
	if (qcom_msc == NULL)
		return -EINVAL;

	return mpam_msc_slc_get_params(dev, query, sizeof(struct msc_query), client_info,
			sizeof(struct slc_client_info), PARAM_GET_CLIENT_INFO_MSC);
}

static int slc_get_cache_partition(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct qcom_slc_capability *qcom_slc_capability;
	struct slc_client_capability *slc_client_cap;
	struct qcom_slc_gear_val *gear_config;

	query = (struct msc_query *) msc_partid;
	qcom_msc = slc_capability_check(dev, query);
	if (qcom_msc == NULL)
		return -EINVAL;

	gear_config = (struct qcom_slc_gear_val *)msc_partconfig;
	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &qcom_slc_capability->slc_client_cap[query->client_id];
	if (slc_client_cap->enabled == false)
		return -EINVAL;

	return mpam_msc_slc_get_params(dev, query, sizeof(struct msc_query), gear_config,
			sizeof(struct qcom_slc_gear_val), PARAM_GET_CACHE_PARTITION_MSC);
}

static int slc_get_cache_partition_capability(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct qcom_slc_capability *qcom_slc_capability;
	struct slc_partid_capability *slc_partid_capability;
	struct slc_client_capability *slc_client_cap;

	query = (struct msc_query *) msc_partid;
	qcom_msc = slc_capability_check(dev, query);
	if (qcom_msc == NULL)
		return -EINVAL;

	slc_partid_capability = (struct slc_partid_capability *) msc_partconfig;
	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &qcom_slc_capability->slc_client_cap[query->client_id];
	if (slc_client_cap->enabled == false)
		return -EINVAL;

	return mpam_msc_slc_get_params(dev, query, sizeof(struct msc_query), slc_partid_capability,
			sizeof(struct slc_partid_capability), PARAM_GET_CACHE_CAPABILITY_MSC);
}

#if 0
static int slc_mon_config(uint32_t msc_id, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct slc_miss_config slc_miss_config;
	struct qcom_slc_capability *qcom_slc_capability;
	struct slc_client_capability *slc_client_cap;
	struct qcom_slc_mon *slc_mon;
	uint32_t client_id, part_id;

	qcom_msc = qcom_msc_lookup(msc_id);
	if (qcom_msc == NULL)
		return -EINVAL;


	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	memcpy(&slc_miss_config.query, msc_partid, sizeof(struct msc_query));
	client_id = slc_miss_config.query.client_id;
	part_id = slc_miss_config.query.part_id;
	slc_client_cap = &(qcom_slc_capability->slc_client_cap[client_id]);
	slc_mon = &slc_client_cap->qcom_slc_mon[part_id];
	memcpy(&slc_miss_config.enable, msc_partconfig, sizeof(struct miss_config_enabled));
	if (slc_miss_config.enable.enabled) {
		if (miss_cfgd == miss_cfg_available - 1)
			return -EINVAL;

		if (slc_mon->enabled == true)
			return -EINVAL;

	} else {
		if (miss_cfgd == 0)
			return -EINVAL;

		if (slc_mon->enabled == false)
			return -EINVAL;

	}

	if (mpam_msc_slc_set_params(dev, &slc_miss_config, sizeof(struct  slc_miss_config),
			PARAM_SET_CONFIG_MONITOR_SLC) == 0) {
		if (slc_miss_config.enable.enabled) {
			slc_mon->enabled = true;
			miss_cfgd++;
		} else {
			slc_mon->enabled = false;
			miss_cfgd--;
		}
	}

	return 0;
};


int slc_mon_shared_memread(struct qcom_slc_mon_mem *ptr) {
	struct qcom_slc_mon_mem *addr = (struct qcom_slc_mon_mem *) slc_mon_base;
	int retry_cnt = 0, i;
	uint64_t capture_status, timestamp;

	do {
		while (unlikely((capture_status = addr->capture_status) % 2) &&
								(retry_cnt < 10))
			retry_cnt++;
		i = 0;
		timestamp = addr->last_capture_time;

		for (i = 0; i < 5; i++) {
			ptr->data[i].part_info.client_id = addr->data[i].part_info.client_id;
			ptr->data[i].part_info.part_id = addr->data[i].part_info.part_id;
			ptr->data[i].slc_lines.num_cache_lines = addr->data[i].slc_lines.num_cache_lines;
			ptr->data[i].rd_miss_stats.rd_misses = addr->data[i].rd_miss_stats.rd_misses;
		}



	} while (capture_status != addr->capture_status);

	if (retry_cnt == 10)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_GPL(slc_mon_shared_memread);

static int slc_mon_stats_read(uint32_t msc_id, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = qcom_msc_lookup(msc_id);
	if (qcom_msc == NULL)
		return -EINVAL;

	return 0;
};
#endif

static struct mpam_msc_ops slc_msc_ops = {
	.set_cache_partition = slc_set_cache_partition,
	.get_cache_partition = slc_get_cache_partition,
	.get_cache_partition_capability = slc_get_cache_partition_capability,
	.reset_cache_partition = slc_reset_cache_partition,
	//.mon_config = slc_mon_config,
	//.mon_stats_read = slc_mon_stats_read,

};

static int slc_client_info_read(struct device *dev, struct device_node *node)
{
	int client_idx, partid_idx, ret = -EINVAL;
	struct msc_query query;
	struct qcom_mpam_msc *qcom_msc;
	struct slc_client_capability *slc_client_cap;
	struct qcom_slc_capability *qcom_slc_capability;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc->qcom_msc_id.qcom_msc_type != SLC)
		return -EINVAL;

	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	query.qcom_msc_id.qcom_msc_type = qcom_msc->qcom_msc_id.qcom_msc_type;
	query.qcom_msc_id.qcom_msc_class = qcom_msc->qcom_msc_id.qcom_msc_class;
	query.qcom_msc_id.idx = qcom_msc->qcom_msc_id.idx;
	for (client_idx = 0; client_idx < qcom_slc_capability->num_clients; client_idx++) {
		slc_client_cap = &(qcom_slc_capability->slc_client_cap[client_idx]);
		query.client_id = slc_client_id[client_idx];
		slc_client_cap->client_info.client_id = slc_client_id[client_idx];
		ret = of_property_read_string_index(node, "qcom,slc_clients", client_idx,
							&slc_client_cap->client_name);
		slc_client_cap->enabled = false;
		ret = slc_client_query(dev, &query, &(slc_client_cap->client_info));
		if (ret)
			continue;

		if ((slc_client_cap->client_info.num_part_id == 0) ||
				(slc_client_cap->client_info.num_part_id == SLC_INVALID_PARTID))
			continue;

		slc_client_cap->enabled = true;
		slc_client_cap->slc_partid_cap = devm_kcalloc(dev,
				slc_client_cap->client_info.num_part_id,
				sizeof(struct slc_partid_capability), GFP_KERNEL);
		if (slc_client_cap->slc_partid_cap == NULL) {
			ret = -ENOMEM;
			break;
		}

		for (partid_idx = 0; partid_idx < slc_client_cap->client_info.num_part_id;
				partid_idx++) {
			query.part_id = partid_idx;
			ret = msc_system_get_device_capability(qcom_msc->msc_id, &query,
					&(slc_client_cap->slc_partid_cap[partid_idx]));
			if (ret)
				continue;
		}
	}

	return ret;
}
static int mpam_msc_slc_probe(struct platform_device *pdev)
{
	int ret;
	uint32_t indx;
	struct device_node *node;
	struct qcom_mpam_msc *qcom_msc;
	struct qcom_slc_capability *qcom_slc_capability;

	qcom_msc = devm_kzalloc(&pdev->dev, sizeof(struct qcom_mpam_msc), GFP_KERNEL);
	if (qcom_msc == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	qcom_msc->sdev = get_qcom_scmi_device();
	if (IS_ERR(qcom_msc->sdev)) {
		ret = PTR_ERR(qcom_msc->sdev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Error getting scmi_dev ret=%d\n", ret);

		goto err;
	}

	qcom_msc->scmi_ops = qcom_msc->sdev->handle->devm_protocol_get(qcom_msc->sdev, QCOM_SCMI_VENDOR_PROTOCOL, &qcom_msc->ph);
	if (IS_ERR(qcom_msc->scmi_ops)) {
		ret = PTR_ERR(qcom_msc->scmi_ops);
		qcom_msc->scmi_ops = NULL;
		dev_err(&pdev->dev, "Error getting vendor protocol ops: %d\n", ret);
		goto err;
	}

	qcom_msc->qcom_msc_id.qcom_msc_type = SLC;
	qcom_msc->qcom_msc_id.qcom_msc_class = CACHE_TYPE;
	qcom_msc->ops = &slc_msc_ops;
	qcom_msc->dev = &pdev->dev;

	node = pdev->dev.of_node;
	of_property_read_u32(node, "dev-index", &indx);
	qcom_msc->qcom_msc_id.idx = indx;
	of_property_read_u32(node, "qcom,msc-id", &qcom_msc->msc_id);
	of_property_read_string(node, "qcom,msc-name", &qcom_msc->msc_name);
	ret = attach_dev(&pdev->dev, qcom_msc, SLC);
	if (ret)
		goto err;

	qcom_msc->msc_capability = devm_kzalloc(&pdev->dev, sizeof(struct qcom_slc_capability), GFP_KERNEL);
	if (qcom_msc->msc_capability == NULL) {
		ret = -ENOMEM;
		goto err_detach;
	}

	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	qcom_slc_capability->num_clients = of_property_count_strings(node, "qcom,slc_clients");
	qcom_slc_capability->slc_client_cap = devm_kcalloc(&pdev->dev,
			qcom_slc_capability->num_clients, sizeof(struct  slc_client_capability),
			GFP_KERNEL);
	if (qcom_slc_capability->slc_client_cap == NULL) {
		ret = -ENOMEM;
		goto err_detach;
	}

	platform_set_drvdata(pdev, qcom_msc);
	slc_client_info_read(&pdev->dev, node);

	return 0;

err_detach:
	detach_dev(&pdev->dev, qcom_msc, SLC);
err:
	pr_err("MPAM SLC driver probe failed!\n");
	return ret;
}

int mpam_msc_slc_remove(struct platform_device *pdev)
{
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = (struct qcom_mpam_msc *)platform_get_drvdata(pdev);
	detach_dev(&pdev->dev, qcom_msc, SLC);
	platform_set_drvdata(pdev, NULL);
	return 0;
}
static const struct of_device_id mpam_msc_slc_table[] = {
	{ .compatible = "qcom,slc-mpam" },
	{}
};
MODULE_DEVICE_TABLE(of, mpam_msc_slc_table);

static struct platform_driver mpam_msc_slc_driver = {
	.driver = {
		.name = "mpam-msc-slc",
		.of_match_table = mpam_msc_slc_table,
	},
	.probe = mpam_msc_slc_probe,
	.remove = mpam_msc_slc_remove,
};

module_platform_driver(mpam_msc_slc_driver);

MODULE_SOFTDEP("pre: llcc_qcom");
MODULE_SOFTDEP("pre: mpam");
MODULE_DESCRIPTION("QCOM MPAM MSC SLC driver");
MODULE_LICENSE("GPL");
