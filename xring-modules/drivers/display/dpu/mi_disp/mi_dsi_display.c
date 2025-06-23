// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 XiaoMi, Inc. All rights reserved.
 */
#define pr_fmt(fmt)	"mi-dsi-display:[%s] " fmt, __func__

#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_host.h"
#include "dsi_connector.h"
#include <display/xring_dpu_drm.h>
#include "mi_disp_print.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"
#include "mi_disp_feature.h"
#include "mi_panel_id.h"
#include "mi_disp_flatmode.h"
#include "drm/drm_mipi_dsi.h"

static char oled_wp_info_str[32] = {0};
static char sec_oled_wp_info_str[32] = {0};
static char cell_id_info_str[32] = {0};

#define MAX_CMDLINE_PARAM_LEN	 512
#define MAX_DEBUG_POLICY_CMDLINE_LEN 64
static char display_debug_policy[MAX_DEBUG_POLICY_CMDLINE_LEN] = {0};

static struct dsi_read_info g_dsi_read_info;

int mi_get_disp_id(const char *display_type)
{
	if (!strncmp(display_type, "primary", 7))
		return MI_DISP_PRIMARY;
	else
		return MI_DISP_SECONDARY;
}

struct dsi_display *mi_get_primary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct dsi_display *dsi_display = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_PRIMARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi_display = (struct dsi_display *)dd_ptr->display;
			return dsi_display;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

struct dsi_display *mi_get_secondary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct dsi_display *dsi_display = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_SECONDARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi_display = (struct dsi_display *)dd_ptr->display;
			return dsi_display;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	char trace_buf[64];
	int ret = 0;

	if (!dsi_display || !ctl) {
		DISP_ERROR("Invalid display or ctl ptr\n");
		return -EINVAL;
	}

	dsi_display_lock(display);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	snprintf(trace_buf, sizeof(trace_buf), "set_disp_param:%s",
			get_disp_feature_id_name(ctl->feature_id));
	ret = mi_dsi_panel_set_disp_param(dsi_display->panel, ctl);
	mi_dsi_release_wakelock(dsi_display->panel);
	dsi_display_unlock(display);

	return ret;
}

int mi_dsi_display_get_disp_param(void *display,
			struct disp_feature_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_disp_param(dsi_display->panel, ctl);
}

ssize_t mi_dsi_display_show_disp_param(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_show_disp_param(dsi_display->panel, buf, size);
}

void mi_dsi_display_set_dsi_phy_rw(struct disp_display *dd_ptr, const char *opt)
{
}

void mi_dsi_display_set_dsi_porch_rw(struct disp_display *dd_ptr, const char *opt)
{
}

ssize_t mi_dsi_display_show_dsi_phy_rw(void *display,
			char *buf, size_t size)
{
	int ret = 0;

	return ret;
}

ssize_t mi_dsi_display_show_dsi_porch_rw(void *display,
			char *buf, size_t size)
{
	ssize_t count = 0;

	return count;
}

ssize_t mi_dsi_display_show_pps_rw(void *display,
			char *buf, size_t size)
{
	ssize_t count = 0;

	return count;
}

int mi_dsi_display_write_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display_lock(display);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_dsi_cmd(dsi_display->panel, ctl);
	mi_dsi_release_wakelock(dsi_display->panel);
	dsi_display_unlock(display);

	return ret;
}

int mi_dsi_display_read_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_cmd_desc cmd_desc = {0};
	int i = 0, ret = 0;
	u32 transfer_flag;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	cmd_desc.msg.channel = 0;
	cmd_desc.msg.flags   |= MIPI_DSI_MSG_UNICAST_COMMAND;
	cmd_desc.msg.rx_len  = ctl->rx_len;
	cmd_desc.msg.rx_buf  = ctl->rx_ptr;
	cmd_desc.port_index  = ctl->is_master_port ? 0 : 1;

	dsi_display_lock(dsi_display);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = dsi_panel_create_cmd_packets(ctl->tx_ptr, ctl->tx_len, 1, &cmd_desc);
	if (ret) {
		DISP_ERROR("dsi_panel_create_cmd_packets failed\n");
		mi_dsi_release_wakelock(dsi_display->panel);
		dsi_display_unlock(dsi_display);
		return -EINVAL;
	}

	transfer_flag = dsi_basic_cmd_flag_get(&cmd_desc, USE_CPU, 0, 1);
	ret = dsi_display_cmd_transfer_locked(dsi_display, &cmd_desc, transfer_flag);
	mi_dsi_release_wakelock(dsi_display->panel);
	dsi_display_unlock(dsi_display);

	for (i = 0; i < cmd_desc.msg.rx_len; i++)
		DISP_INFO("read reg 0x%x: 0x%x\n",
			((u8 *)cmd_desc.msg.tx_buf)[0], ((u8 *)cmd_desc.msg.rx_buf)[i]);

	return ret;
}

int mi_dsi_display_set_mipi_rw(void *display, char *buf)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_cmd_rw_ctl ctl;
	int ret = 0;
	char *token = NULL, *input_copy = NULL, *input_dup = NULL;
	const char *delim = " ";
	bool is_read = false;
	bool is_master_port = true;
	char *buffer = NULL;
	char *alloc_buffer = NULL;
	u32 buf_size = 0;
	u32 tmp_data = 0;

	memset(&ctl, 0, sizeof(struct dsi_cmd_rw_ctl));
	memset(&g_dsi_read_info, 0, sizeof(struct dsi_read_info));

	DISP_TIME_INFO("input buffer:{%s}\n", buf);

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		ret = -ENOMEM;
		goto exit;
	}

	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);

	/* Split a string into token */
	token = strsep(&input_copy, delim);
	ret = kstrtoint(token, 10, &tmp_data);
	if (ret) {
		DISP_ERROR("input buffer conversion failed\n");
		goto exit_free0;
	}
	is_read = !!(tmp_data & 0x1);
	is_master_port = !(tmp_data & 0x2);

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	token = strsep(&input_copy, delim);
	if (token) {
		ret = kstrtoint(token, 10, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		if (tmp_data > sizeof(g_dsi_read_info.rx_buf)) {
			DISP_ERROR("read size exceeding the limit %lu\n",
					sizeof(g_dsi_read_info.rx_buf));
			goto exit_free0;
		}
		ctl.rx_len = tmp_data;
		ctl.rx_ptr = g_dsi_read_info.rx_buf;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	if (strlen(input_copy) > 0 && strlen(input_copy) <  128*1024) {
		alloc_buffer = kzalloc(strlen(input_copy)+1, GFP_KERNEL);
		if (!alloc_buffer) {
			ret = -ENOMEM;
			goto exit_free0;
		}
		buffer = alloc_buffer;
	} else {
		DISP_ERROR("read/write size exceeding the limit\n");
		ret = -ENOMEM;
		goto exit_free0;
	}
	token = strsep(&input_copy, delim);
	while (token) {
		ret = kstrtoint(token, 16, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free1;
		}
		DISP_INFO("buffer[%d] = 0x%02x\n", buf_size, tmp_data);
		buffer[buf_size++] = (tmp_data & 0xff);
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
			token = strsep(&input_copy, delim);
		} else {
			token = NULL;
		}
	}

	/* check buffer's fist num to find whether set LP/HS flag to send command: (00: LP)/(01:HS) */
	if (buffer[0] < MI_DSI_CMD_MAX_STATE) {
		if (buffer[0] == 0x00)
			ctl.tx_state = MI_DSI_CMD_LP_STATE;
		else if (buffer[0] == 0x01)
			ctl.tx_state = MI_DSI_CMD_HS_STATE;
		++buffer;
		--buf_size;
	} else {
		ctl.tx_state = MI_DSI_CMD_LP_STATE;
		DISP_INFO("transfer speed not set, default use LPM. buffer[0] = 0x%02x\n", buffer[0]);
	}
	DISP_INFO("ctl.tx_state= 0x%02x\n", ctl.tx_state);

	ctl.tx_len = buf_size;
	ctl.tx_ptr = buffer;
	ctl.is_master_port = is_master_port;
	if (is_read) {
		ret = mi_dsi_display_read_dsi_cmd(dsi_display, &ctl);
		if (ret) {
			DISP_ERROR("read dsi cmd transfer failed rc = %d\n", ret);
			ret = -EAGAIN;
		} else {
			g_dsi_read_info.is_read_sucess = true;
			g_dsi_read_info.rx_len = ctl.rx_len;
			ret = 0;
		}
	} else {
		ret = mi_dsi_display_write_dsi_cmd(dsi_display, &ctl);
	}

exit_free1:
	kfree(alloc_buffer);
exit_free0:
	kfree(input_dup);
exit:

	return ret;
}

ssize_t mi_dsi_display_show_mipi_rw(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	ssize_t count = 0;
	int i = 0;
	const char help_message[] = {
		"\tFirst byte means write/read: 00/01 (Decimalism)\n"
		"\tSecend byte means read back bytes size (Decimalism)\n"
		"\tThird byte means LP/HS send: 00/01 (Hexadecimal)\n"
		"\tExample:\n"
		"\t\twrite:00 00 00 39 00 00 00 00 00 03 51 07 FF\n"
		"\t\tread: 01 02 00 06 01 00 00 00 00 01 52\n"
	};

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (g_dsi_read_info.is_read_sucess) {
		for (i = 0; i < g_dsi_read_info.rx_len; i++) {
			if (i == g_dsi_read_info.rx_len - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X\n",
					 g_dsi_read_info.rx_buf[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X,",
					 g_dsi_read_info.rx_buf[i]);
			}
		}
	} else {
		count = snprintf(buf, PAGE_SIZE, "%s\n", help_message);
	}

	return count;
}

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	char *pname = NULL;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (dsi_display->panel->panel_info.name) {
		/* find the last occurrence of a character in a string */
		pname = strrchr(dsi_display->panel->panel_info.name, ',');
		if (pname && *pname)
			ret = snprintf(buf, size, "panel_name=%s\n", ++pname);
		else
			ret = snprintf(buf, size, "panel_name=%s\n", dsi_display->panel->panel_info.name);
	} else {
		ret = snprintf(buf, size, "panel_name=%s\n", "null");
	}

	return ret;
}

int mi_dsi_display_read_panel_infos(struct dsi_display *display)
{
	char buf[4095] = {0};

	if (!display || !display->panel) {
		DSI_ERROR("Invalid display or panel pointer\n");
		return -EINVAL;
	}

	if (display->panel->mi_panel.mi_cfg.panel_build_id_read_needed) {
		if (mi_dsi_display_read_panel_build_id(display)) {
			DISP_INFO("[%s] DSI display read panel build id failed\n",
					display->panel->panel_info.name);
		}
	}
	mi_dsi_display_read_wp_info((void *)display, buf, sizeof(buf));
	mi_dsi_display_read_cell_id((void *)display, buf, sizeof(buf));

	return 0;
}

int mi_dsi_display_read_panel_build_id(struct dsi_display *display)
{
	int rc = 0;
	struct drm_panel_build_id_config *config;
	struct dsi_cmd_desc cmd;
	struct dsi_panel *panel;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!display || !display->panel)
		return -EINVAL;

	panel = display->panel;
	mi_cfg = &panel->mi_panel.mi_cfg;
	if (!mi_cfg || !mi_cfg->panel_build_id_read_needed) {
		DISP_INFO("panel build id do not have to read\n");
		return 0;
	}

	config = &(panel->mi_panel.id_config);

	cmd = config->id_cmd.cmds[0];

	rc = mi_dsi_display_cmd_read(display, cmd, &config->build_id, config->id_cmds_rlen);

	if (rc)
		DISP_ERROR("[DSI] Display command receive failed, rc=%d\n", rc);
	else
		mi_cfg->panel_build_id_read_needed = false;

	return rc;
}

ssize_t mi_dsi_display_read_panel_build_id_info(void *display,
		char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_panel *panel;
	int ret = 0;
	int real_build_id = 0;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;
	if (panel->mi_panel.id_config.build_id) {
		real_build_id = mi_get_panel_build_id(panel->mi_panel.id_config.build_id);
		ret = snprintf(buf, size, "P%02X\n", real_build_id);
	} else {
		ret = snprintf(buf, size, "%s\n", "Unsupported");
	}
	return ret;
}

static int mi_dsi_display_re_read_wp_info(struct dsi_display *display)
{
	int rc = 0;
	struct drm_panel_wp_config *config;
	struct dsi_cmd_desc cmd;
	struct dsi_cmd_set *cmd_sets = NULL;

	if (!display || !display->panel)
		return -EINVAL;

	config = &(display->panel->mi_panel.wp_config);
	if (config->wp_cmd.num_cmds == 0 || config->wp_cmds_rlen == 0)
		return -EINVAL;

	cmd_sets = &config->pre_tx_cmd;
	if (cmd_sets->num_cmds != 0) {
		rc = mi_dsi_display_cmd_write(display, cmd_sets);
		if (rc) {
			DISP_ERROR("send cell id pre cmd failed!");
			return rc;
		}
	}

	cmd = config->wp_cmd.cmds[0];
	if (!config->return_buf) {
		DISP_ERROR("[%s] wp_info return buffer is null, rc=%d\n",
			display->panel->panel_info.name, rc);
		return -ENOMEM;
	}

	memset(config->return_buf, 0x0, sizeof(*config->return_buf));

	rc = mi_dsi_display_cmd_read(display, cmd, config->return_buf, config->wp_cmds_rlen);
	if (rc)
		DISP_ERROR("[DSI] Display command receive failed, rc=%d\n", rc);
	return rc;
}

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_panel *panel;
	int display_id = 0;
	int ret = 0, i = 0;
	char *wp_info_str;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	display_id = dsi_display->display_type;
	panel = dsi_display->panel;

	if (display_id == MI_DISP_PRIMARY)
		wp_info_str = oled_wp_info_str;
	else if (display_id == MI_DISP_SECONDARY)
		wp_info_str = sec_oled_wp_info_str;
	else {
		ret = snprintf(buf, size, "%s\n", "Unsupported display");
		return ret;
	}

	if (!strlen(wp_info_str) || strstr(wp_info_str, "00000000") != NULL) {
		DISP_WARN("[%s-%d]read oled_wp_info_str() failed from cmdline\n",
				dsi_display->panel->panel_info.name, display_id);
		ret = mi_dsi_display_re_read_wp_info(display);
		if (ret) {
			wp_info_str = NULL;
			DISP_ERROR("[%s-%d] read wp_info failed, rc=%d\n",
					dsi_display->panel->panel_info.name, display_id, ret);
			return ret;
		}

		for (i = 0; i < panel->mi_panel.wp_config.wp_cmds_rlen-panel->mi_panel.wp_config.wp_read_info_index; i++)
			snprintf(wp_info_str + i * 2, size - (i * 2), "%02x",
					panel->mi_panel.wp_config.return_buf[i +
					panel->mi_panel.wp_config.wp_read_info_index]);
	}

	DISP_TIME_INFO("display %d wp info is %s,index is %d\n",
			dsi_display->display_type, wp_info_str,
			panel->mi_panel.wp_config.wp_read_info_index);
	ret = snprintf(buf, size, "%s\n", wp_info_str);
	return ret;
}

int mi_dsi_display_get_fps(void *display, struct disp_fps_info *fps_info)
{
	int ret = 120;

	return ret;
}

int mi_dsi_display_set_doze_brightness(void *display,
			u32 doze_brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int disp_id = MI_DISP_PRIMARY;
	int ret = 0;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid display/panel\n");
		return -EINVAL;
	}

	dsi_display_lock(dsi_display);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	mutex_lock(&dsi_display->panel->mi_panel.mi_cfg.doze_lock);
	ret = mi_dsi_panel_set_doze_brightness(dsi_display->panel,
				doze_brightness, USE_CPU);
	mutex_unlock(&dsi_display->panel->mi_panel.mi_cfg.doze_lock);
	mi_dsi_release_wakelock(dsi_display->panel);
	dsi_display_unlock(dsi_display);

	disp_id = mi_get_disp_id(dsi_display->panel->mi_panel.type);
	mi_disp_feature_event_notify_by_type(disp_id, MI_DISP_EVENT_DOZE,
			sizeof(doze_brightness), doze_brightness);

	return ret;

}

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *doze_brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_doze_brightness(dsi_display->panel,
				doze_brightness);
}

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness(dsi_display->panel,
				brightness);
}

int mi_dsi_display_set_fp_unlock_state(void *display,
			u32 fp_unlock_value)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display_lock(dsi_display);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_set_fp_unlock_state(dsi_display->panel, fp_unlock_value);
	mi_dsi_release_wakelock(dsi_display->panel);
	dsi_display_unlock(dsi_display);

	return ret;
}

int mi_dsi_display_write_dsi_cmd_set(void *display,
			int type)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_dsi_cmd_set(dsi_display->panel, type);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

ssize_t mi_dsi_display_show_dsi_cmd_set_type(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_show_dsi_cmd_set_type(dsi_display->panel, buf, size);
}

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	ret = mi_dsi_panel_set_brightness_clone(dsi_display->panel,
				brightness_clone);

	return ret;
}

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness_clone(dsi_display->panel,
				brightness_clone);
}

int mi_dsi_display_get_max_brightness_clone(void *display,
			u32 *max_brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_max_brightness_clone(dsi_display->panel,
				max_brightness_clone);
}

ssize_t mi_dsi_display_get_hw_vsync_info(void *display,
			char *buf, size_t size)
{
	return 0;
}

static ssize_t compose_ddic_cell_id(char *outbuf, u32 outbuf_len,
		const char *inbuf, u32 inbuf_len)
{
	int i = 0;
	int idx = 0;
	ssize_t count = 0;
	const char ddic_cell_id_dictionary[36] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
		'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	};

	if (!outbuf || !inbuf) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < inbuf_len; i++) {
		idx = inbuf[i] > 35 ? 35 : inbuf[i];
			count += snprintf(outbuf + count, outbuf_len - count, "%c",
				ddic_cell_id_dictionary[idx]);
		DISP_DEBUG("cell_id[%d] = 0x%02X, ch=%c\n", i, idx,
				ddic_cell_id_dictionary[idx]);
	}

	return count;
}

static ssize_t mi_dsi_display_read_ddic_cell_id(struct dsi_display *display,
		char *buf, size_t size)
{
	int rc = 0;
	struct drm_panel_cell_id_config *config;
	struct dsi_cmd_desc cmd;
	struct dsi_cmd_set *cmd_sets = NULL;

	if (!display || !display->panel)
		return -EINVAL;

	config = &(display->panel->mi_panel.cell_id_config);
	if (config->cell_id_cmd.num_cmds == 0 || config->cell_id_cmds_rlen == 0)
		return -EINVAL;

	cmd_sets = &config->pre_tx_cmd;
	if (cmd_sets->num_cmds != 0) {
		rc = mi_dsi_display_cmd_write(display, cmd_sets);
		if (rc) {
			DISP_ERROR("send cell id pre cmd failed!");
			return rc;
		}
	}

	cmd = config->cell_id_cmd.cmds[0];
	if (!config->return_buf) {
		DISP_ERROR("[%s] cell_id_info return buffer is null, rc=%d\n",
			display->panel->panel_info.name, rc);
		return -ENOMEM;
	}

	memset(config->return_buf, 0x0, sizeof(*config->return_buf));
	rc = mi_dsi_display_cmd_read(display, cmd, config->return_buf, config->cell_id_cmds_rlen);
	if (!rc) {
		rc = compose_ddic_cell_id(buf, size, config->return_buf, config->cell_id_cmds_rlen);
		DISP_INFO("cell_id = %s\n", buf);
	} else {
		DISP_ERROR("failed to read panel cell id, rc = %d\n", rc);
		return -EINVAL;
	}

	cmd_sets = &config->after_tx_cmd;
	if (cmd_sets->num_cmds != 0) {
		rc = mi_dsi_display_cmd_write(display, cmd_sets);
		if (rc) {
			DISP_ERROR("send cell id post cmd failed!");
			return rc;
		}
	}

	return rc;
}

ssize_t mi_dsi_display_read_cell_id(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	static char cell_id_info_buffer[32] = {0};
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (!strlen(cell_id_info_str) &&
		(!strlen(cell_id_info_buffer) || strstr(cell_id_info_buffer, "00000000") != NULL)) {
		mi_dsi_acquire_wakelock(dsi_display->panel);
		ret = mi_dsi_display_read_ddic_cell_id(dsi_display, cell_id_info_buffer, size);
		mi_dsi_release_wakelock(dsi_display->panel);
	} else {
		if (strlen(cell_id_info_str)) {
			ret = snprintf(buf, size, "%s\n", cell_id_info_str);
			DISP_INFO("cell_id: %s\n", cell_id_info_str);
		} else {
			ret = snprintf(buf, size, "%s\n", cell_id_info_buffer);
			DISP_INFO("cell_id: %s\n", cell_id_info_buffer);
		}
	}

	return ret;
}

int mi_dsi_display_set_disp_count(void *display, char *buf, size_t count)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return 0;
}

int mi_dsi_display_get_disp_count(void *display, char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return 0;
}

int mi_dsi_display_esd_irq_ctrl(struct dsi_display *display,
			bool enable)
{
	int ret = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display_lock(display);

	ret = mi_dsi_panel_esd_irq_ctrl(display->panel, enable);
	if (ret)
		DISP_ERROR("[%s] failed to set esd irq, rc=%d\n",
				display->panel->panel_info.name, ret);

	dsi_display_unlock(display);

	return ret;
}

void mi_dsi_display_wakeup_pending_doze_work(struct dsi_display *display)
{
	int disp_id = 0;
	struct disp_display *dd_ptr;
	struct disp_feature *df = mi_get_disp_feature();

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return;
	}

	disp_id = display->display_type;
	dd_ptr = &df->d_display[disp_id];
	DISP_DEBUG("display %d pending_doze_cnt = %d\n",
			display->display_type, atomic_read(&dd_ptr->pending_doze_cnt));
	if (atomic_read(&dd_ptr->pending_doze_cnt)) {
		DISP_INFO("display %d wake up pending doze work, pending_doze_cnt = %d\n",
			display->display_type, atomic_read(&dd_ptr->pending_doze_cnt));
		wake_up_interruptible_all(&dd_ptr->pending_wq);
	}
}

int mi_dsi_display_cmd_read_locked(struct dsi_display *display,
			      struct dsi_cmd_desc cmd_desc, u8 *rx_buf, u32 rx_len)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int i = 0, ret = 0;
	u32 transfer_flag;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	//cmd_desc.msg.channel = 0;
	cmd_desc.msg.flags   |= MIPI_DSI_MSG_UNICAST_COMMAND;
	cmd_desc.msg.rx_len  = rx_len;
	cmd_desc.msg.rx_buf  = rx_buf;
	//cmd_desc.msg.type = 0x06;
	transfer_flag = dsi_basic_cmd_flag_get(&cmd_desc, USE_CPU, 0, 1);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = dsi_display_cmd_transfer_locked(dsi_display, &cmd_desc, transfer_flag);
	mi_dsi_release_wakelock(dsi_display->panel);
	for (i = 0; i < cmd_desc.msg.rx_len; i++)
		DISP_DEBUG("read reg 0x%x: 0x%x\n",
			((u8 *)cmd_desc.msg.tx_buf)[0], ((u8 *)cmd_desc.msg.rx_buf)[i]);
	return ret;
}

int mi_dsi_display_cmd_read(struct dsi_display *display,
			      struct dsi_cmd_desc cmd, u8 *rx_buf, u32 rx_len)
{
	int rc = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display_lock(display);
	rc = mi_dsi_display_cmd_read_locked(display, cmd, rx_buf, rx_len);
	dsi_display_unlock(display);
	return rc;
}

int mi_dsi_display_cmd_write_locked(struct dsi_display *display, struct dsi_cmd_set *cmd_sets)
{
	int rc = 0;

	dsi_panel_lock(display->panel);
	rc = mi_dsi_panel_write_cmd_set(display->panel, cmd_sets);
	if (rc)
		DISP_ERROR("[%s] failed to send cmds, rc=%d\n", display->panel->mi_panel.type, rc);

	dsi_panel_unlock(display->panel);

	return rc;
}

int mi_dsi_display_cmd_write(struct dsi_display *display, struct dsi_cmd_set *cmd_sets)
{
	int rc = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display_lock(display);
	rc = mi_dsi_display_cmd_write_locked(display, cmd_sets);
	dsi_display_unlock(display);

	return rc;
}

int mi_dsi_display_check_flatmode_status(void *display, bool *status)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int rc = 0;

	if (!display || !status) {
		DISP_ERROR("Invalid display/status ptr\n");
		return -EINVAL;
	}

	*status = false;

	mi_dsi_acquire_wakelock(dsi_display->panel);
	rc = mi_dsi_panel_flatmode_validate_status(dsi_display, status);
	mi_dsi_release_wakelock(dsi_display->panel);

	return rc;
}

bool mi_dsi_display_ramdump_support(void)
{
	/* when debug policy is 0x0 or 0x20, full dump not supported */
	if (strcmp(display_debug_policy, "0x0") != 0 && strcmp(display_debug_policy, "0x20") != 0)
		return true;
	return false;
}

static void mi_display_pm_suspend_delayed_work_handler(struct kthread_work *k_work)
{
	struct kthread_delayed_work *kdelayed_work =
			container_of(k_work, struct kthread_delayed_work, work);
	struct disp_delayed_work *delayed_work =
			container_of(kdelayed_work, struct disp_delayed_work, delayed_work);
	struct dsi_panel *dsi_panel = (struct dsi_panel *)(delayed_work->data);
	struct mi_dsi_panel_cfg *mi_cfg = &dsi_panel->mi_panel.mi_cfg;
	unsigned long mode_flags_backup = 0;
	int pmic_pwrkey_status = mi_cfg->pmic_pwrkey_status;
	int rc = 0;

	mi_dsi_acquire_wakelock(dsi_panel);

	dsi_panel_lock(dsi_panel);
	if (dsi_panel->initialized && pmic_pwrkey_status == PMIC_PWRKEY_BARK_TRIGGER) {
		/* First disable esd irq, then send panel off cmd */
		if (mi_cfg->esd_err_enabled)
			mi_dsi_panel_esd_irq_ctrl_locked(dsi_panel, false);

		mode_flags_backup = dsi_panel->mipi_device.mode_flags;
		dsi_panel->mipi_device.mode_flags |= MIPI_DSI_MODE_LPM;
		rc = mipi_dsi_dcs_set_display_off(&dsi_panel->mipi_device);
		dsi_panel->mipi_device.mode_flags = mode_flags_backup;
		if (rc < 0)
			DISP_ERROR("failed to send MIPI_DCS_SET_DISPLAY_OFF\n");
		else
			DISP_INFO("panel send MIPI_DCS_SET_DISPLAY_OFF\n");
	} else {
		DISP_INFO("panel does not need to be off in advance\n");
	}
	dsi_panel_unlock(dsi_panel);

	mi_dsi_release_wakelock(dsi_panel);

	kfree(delayed_work);
}

int mi_display_pm_suspend_delayed_work(struct dsi_display *display)
{
	int disp_id = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct dsi_panel *panel = display->panel;
	struct disp_display *dd_ptr;
	struct disp_delayed_work *suspend_delayed_work;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	suspend_delayed_work = kzalloc(sizeof(*suspend_delayed_work), GFP_KERNEL);
	if (!suspend_delayed_work) {
		DISP_ERROR("failed to allocate delayed_work buffer\n");
		return -ENOMEM;
	}

	disp_id = mi_get_disp_id(panel->mi_panel.type);
	dd_ptr = &df->d_display[disp_id];

	kthread_init_delayed_work(&suspend_delayed_work->delayed_work,
			mi_display_pm_suspend_delayed_work_handler);
	suspend_delayed_work->dd_ptr = dd_ptr;
	suspend_delayed_work->wq = &dd_ptr->pending_wq;
	suspend_delayed_work->data = panel;
	return kthread_queue_delayed_work(dd_ptr->worker, &suspend_delayed_work->delayed_work,
				msecs_to_jiffies(DISPLAY_DELAY_SHUTDOWN_TIME_MS));
}

int mi_display_powerkey_callback(int status)
{
	struct dsi_display *dsi_display = mi_get_primary_dsi_display();
	struct dsi_panel *panel;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid dsi_display or dsi_panel ptr\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;
	mi_cfg = &panel->mi_panel.mi_cfg;
	mi_cfg->pmic_pwrkey_status = status;

	if (status == PMIC_PWRKEY_BARK_TRIGGER)
		return mi_display_pm_suspend_delayed_work(dsi_display);

	return 0;
}

int mi_dsi_display_set_count_info(void *display,
			struct disp_count_info *count_info)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	ret = mi_dsi_panel_set_count_info(dsi_display->panel, count_info);

	return ret;
}

u32 mi_get_display_power_mode(struct dsi_display *display)
{
	struct dsi_connector *dsi_connector;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}
	dsi_connector = to_dsi_connector(display->connector);
	if (!dsi_connector) {
		DISP_ERROR("invalid parameters, %pK\n", dsi_connector);
		return -EINVAL;
	}
	return dsi_connector->current_power_mode;
}

int mi_dsi_display_backlight_set_locked(struct dsi_display *display, u32 backlight_level)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_panel *panel;
	struct mi_dsi_panel_cfg *mi_cfg;
	int rc = 0;

	if (dsi_display == NULL || dsi_display->panel == NULL)
		return -EINVAL;

	panel = dsi_display->panel;
	mi_cfg = &panel->mi_panel.mi_cfg;

	if (mi_cfg->hwc_set_doze_brightness) {
		if (backlight_level == 0) {
			DISP_WARN("not allow set backlight zero in DOZE");
			goto exit;
		}
		if (mi_cfg->fp_unlock_value != FINGERPRINT_UNLOCK_SUCCESS)
			mi_dsi_panel_update_last_bl_level(panel, backlight_level);
		rc = mi_dsi_panel_set_doze_brightness_lock(panel,
					backlight_level > 150 ? DOZE_BRIGHTNESS_HBM : DOZE_BRIGHTNESS_LBM,
					USE_CPU);
		if (rc)
			DISP_ERROR("failed to set doze brightness\n");
		goto exit;
	}
exit:
	return rc;
}

int mi_dsi_display_cmd_set_send_locked(struct dsi_display *display,
		enum dsi_cmd_set_type type, struct dsi_cmd_set *cmd_set,
		u8 transfer_type, bool is_possible_in_suspend)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid dsi_display or dsi_panel ptr\n");
		return -EINVAL;
	}

	if (is_possible_in_suspend)
		ret = dsi_display_cmd_set_send_locked(dsi_display, type, cmd_set, transfer_type);
	else
		ret = dsi_panel_cmd_set_send(dsi_display->panel, type, cmd_set, transfer_type);

	return ret;
}
