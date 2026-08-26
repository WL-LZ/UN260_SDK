// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for xm91080s DSI panel.
 *
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <video.h>
#include <panel.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_xm91080"

struct xm91080 {
	struct gpio_desc reset;
};

static inline struct xm91080 *panel_to_xm91080(struct aic_panel *panel)
{
	return (struct xm91080 *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct xm91080 *xm91080 = panel_to_xm91080(panel);
	int ret;

	panel_di_enable(panel, 0);

	ret = gpio_request_by_name(panel->dev, "reset-gpios", 0,
					&xm91080->reset, GPIOD_IS_OUT);
	if (ret) {
		dev_err(panel->dev, "Failed to get reset gpio\n");
		return ret;
	}
	dm_gpio_set_value(&xm91080->reset, 0);

	aic_delay_ms(10);
	dm_gpio_set_value(&xm91080->reset, 1);
	aic_delay_ms(1);
	dm_gpio_set_value(&xm91080->reset, 0);
	aic_delay_ms(10);
	dm_gpio_set_value(&xm91080->reset, 1);
	aic_delay_ms(120);

	panel_dsi_send_perpare(panel);
	aic_delay_ms(150);

	panel_dsi_dcs_send_seq(panel, 0xB0,0x5A);
	panel_dsi_dcs_send_seq(panel, 0xB1,0x00);
	panel_dsi_dcs_send_seq(panel, 0x89,0x01);
	panel_dsi_dcs_send_seq(panel, 0x91,0x07);
	panel_dsi_dcs_send_seq(panel, 0x92,0xF9);
	panel_dsi_dcs_send_seq(panel, 0xB1,0x03);
	panel_dsi_dcs_send_seq(panel, 0x2C,0x28);
	panel_dsi_dcs_send_seq(panel, 0x00,0xB7);
	panel_dsi_dcs_send_seq(panel, 0x01,0x1B);
	panel_dsi_dcs_send_seq(panel, 0x02,0x00);
	panel_dsi_dcs_send_seq(panel, 0x03,0x00);
	panel_dsi_dcs_send_seq(panel, 0x04,0x00);
	panel_dsi_dcs_send_seq(panel, 0x05,0x00);
	panel_dsi_dcs_send_seq(panel, 0x06,0x00);
	panel_dsi_dcs_send_seq(panel, 0x07,0x00);
	panel_dsi_dcs_send_seq(panel, 0x08,0x00);
	panel_dsi_dcs_send_seq(panel, 0x09,0x00);
	panel_dsi_dcs_send_seq(panel, 0x0A,0x01);
	panel_dsi_dcs_send_seq(panel, 0x0B,0x01);
	panel_dsi_dcs_send_seq(panel, 0x0C,0x20);
	panel_dsi_dcs_send_seq(panel, 0x0D,0x00);
	panel_dsi_dcs_send_seq(panel, 0x0E,0x24);
	panel_dsi_dcs_send_seq(panel, 0x0F,0x1C);
	panel_dsi_dcs_send_seq(panel, 0x10,0xC9);
	panel_dsi_dcs_send_seq(panel, 0x11,0x60);
	panel_dsi_dcs_send_seq(panel, 0x12,0x70);
	panel_dsi_dcs_send_seq(panel, 0x13,0x01);
	panel_dsi_dcs_send_seq(panel, 0x14,0xE7);
	panel_dsi_dcs_send_seq(panel, 0x15,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x16,0x3D);
	panel_dsi_dcs_send_seq(panel, 0x17,0x0E);
	panel_dsi_dcs_send_seq(panel, 0x18,0x01);
	panel_dsi_dcs_send_seq(panel, 0x19,0x00);
	panel_dsi_dcs_send_seq(panel, 0x1A,0x00);
	panel_dsi_dcs_send_seq(panel, 0x1B,0xFC);
	panel_dsi_dcs_send_seq(panel, 0x1C,0x0B);
	panel_dsi_dcs_send_seq(panel, 0x1D,0xA0);
	panel_dsi_dcs_send_seq(panel, 0x1E,0x03);
	panel_dsi_dcs_send_seq(panel, 0x1F,0x04);
	panel_dsi_dcs_send_seq(panel, 0x20,0x0C);
	panel_dsi_dcs_send_seq(panel, 0x21,0x00);
	panel_dsi_dcs_send_seq(panel, 0x22,0x04);
	panel_dsi_dcs_send_seq(panel, 0x23,0x81);
	panel_dsi_dcs_send_seq(panel, 0x24,0x1F);
	panel_dsi_dcs_send_seq(panel, 0x25,0x10);
	panel_dsi_dcs_send_seq(panel, 0x26,0x9B);
	panel_dsi_dcs_send_seq(panel, 0x2D,0x01);
	panel_dsi_dcs_send_seq(panel, 0x2E,0x84);
	panel_dsi_dcs_send_seq(panel, 0x2F,0x00);
	panel_dsi_dcs_send_seq(panel, 0x30,0x02);
	panel_dsi_dcs_send_seq(panel, 0x31,0x08);
	panel_dsi_dcs_send_seq(panel, 0x32,0x01);
	panel_dsi_dcs_send_seq(panel, 0x33,0x1C);
	panel_dsi_dcs_send_seq(panel, 0x34,0x40);
	panel_dsi_dcs_send_seq(panel, 0x35,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x36,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x37,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x38,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x39,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x3A,0x05);
	panel_dsi_dcs_send_seq(panel, 0x3B,0x00);
	panel_dsi_dcs_send_seq(panel, 0x3C,0x00);
	panel_dsi_dcs_send_seq(panel, 0x3D,0x00);
	panel_dsi_dcs_send_seq(panel, 0x3E,0xCF);
	panel_dsi_dcs_send_seq(panel, 0x3F,0x84);
	panel_dsi_dcs_send_seq(panel, 0x40,0x28);
	panel_dsi_dcs_send_seq(panel, 0x41,0xFC);
	panel_dsi_dcs_send_seq(panel, 0x42,0x01);
	panel_dsi_dcs_send_seq(panel, 0x43,0x40);
	panel_dsi_dcs_send_seq(panel, 0x44,0x05);
	panel_dsi_dcs_send_seq(panel, 0x45,0xE8);
	panel_dsi_dcs_send_seq(panel, 0x46,0x16);
	panel_dsi_dcs_send_seq(panel, 0x47,0x00);
	panel_dsi_dcs_send_seq(panel, 0x48,0x00);
	panel_dsi_dcs_send_seq(panel, 0x49,0x88);
	panel_dsi_dcs_send_seq(panel, 0x4A,0x08);
	panel_dsi_dcs_send_seq(panel, 0x4B,0x05);
	panel_dsi_dcs_send_seq(panel, 0x4C,0x03);
	panel_dsi_dcs_send_seq(panel, 0x4D,0xD0);
	panel_dsi_dcs_send_seq(panel, 0x4E,0x13);
	panel_dsi_dcs_send_seq(panel, 0x4F,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x50,0x0A);
	panel_dsi_dcs_send_seq(panel, 0x51,0x53);
	panel_dsi_dcs_send_seq(panel, 0x52,0x26);
	panel_dsi_dcs_send_seq(panel, 0x53,0x22);
	panel_dsi_dcs_send_seq(panel, 0x54,0x09);
	panel_dsi_dcs_send_seq(panel, 0x55,0x22);
	panel_dsi_dcs_send_seq(panel, 0x56,0x00);
	panel_dsi_dcs_send_seq(panel, 0x57,0x1C);
	panel_dsi_dcs_send_seq(panel, 0x58,0x03);
	panel_dsi_dcs_send_seq(panel, 0x59,0x3F);
	panel_dsi_dcs_send_seq(panel, 0x5A,0x28);
	panel_dsi_dcs_send_seq(panel, 0x5B,0x01);
	panel_dsi_dcs_send_seq(panel, 0x5C,0xCC);
	panel_dsi_dcs_send_seq(panel, 0x5D,0x21);
	panel_dsi_dcs_send_seq(panel, 0x5E,0x84);
	panel_dsi_dcs_send_seq(panel, 0x5F,0x10);
	panel_dsi_dcs_send_seq(panel, 0x60,0x42);
	panel_dsi_dcs_send_seq(panel, 0x61,0x40);
	panel_dsi_dcs_send_seq(panel, 0x62,0x06);
	panel_dsi_dcs_send_seq(panel, 0x63,0x3A);
	panel_dsi_dcs_send_seq(panel, 0x64,0xA6);
	panel_dsi_dcs_send_seq(panel, 0x65,0x04);
	panel_dsi_dcs_send_seq(panel, 0x66,0x09);
	panel_dsi_dcs_send_seq(panel, 0x67,0x21);
	panel_dsi_dcs_send_seq(panel, 0x68,0x84);
	panel_dsi_dcs_send_seq(panel, 0x69,0x10);
	panel_dsi_dcs_send_seq(panel, 0x6A,0x42);
	panel_dsi_dcs_send_seq(panel, 0x6B,0x08);
	panel_dsi_dcs_send_seq(panel, 0x6C,0x21);
	panel_dsi_dcs_send_seq(panel, 0x6D,0x84);
	panel_dsi_dcs_send_seq(panel, 0x6E,0x74);
	panel_dsi_dcs_send_seq(panel, 0x6F,0xE2);
	panel_dsi_dcs_send_seq(panel, 0x70,0x6B);
	panel_dsi_dcs_send_seq(panel, 0x71,0x6B);
	panel_dsi_dcs_send_seq(panel, 0x72,0x94);
	panel_dsi_dcs_send_seq(panel, 0x73,0x10);
	panel_dsi_dcs_send_seq(panel, 0x74,0x42);
	panel_dsi_dcs_send_seq(panel, 0x75,0x08);
	panel_dsi_dcs_send_seq(panel, 0x76,0x00);
	panel_dsi_dcs_send_seq(panel, 0x77,0x00);
	panel_dsi_dcs_send_seq(panel, 0x78,0x0F);
	panel_dsi_dcs_send_seq(panel, 0x79,0xE0);
	panel_dsi_dcs_send_seq(panel, 0x7A,0x01);
	panel_dsi_dcs_send_seq(panel, 0x7B,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x7C,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x7D,0x0F);
	panel_dsi_dcs_send_seq(panel, 0x7E,0x41);
	panel_dsi_dcs_send_seq(panel, 0x7F,0xFE);
	panel_dsi_dcs_send_seq(panel, 0xB1,0x02);
	panel_dsi_dcs_send_seq(panel, 0x00,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x01,0x05);
	panel_dsi_dcs_send_seq(panel, 0x02,0xC8);
	panel_dsi_dcs_send_seq(panel, 0x03,0x00);
	panel_dsi_dcs_send_seq(panel, 0x04,0x14);
	panel_dsi_dcs_send_seq(panel, 0x05,0x4B);
	panel_dsi_dcs_send_seq(panel, 0x06,0x64);
	panel_dsi_dcs_send_seq(panel, 0x07,0x0A);
	panel_dsi_dcs_send_seq(panel, 0x08,0xC0);
	panel_dsi_dcs_send_seq(panel, 0x09,0x00);
	panel_dsi_dcs_send_seq(panel, 0x0A,0x00);
	panel_dsi_dcs_send_seq(panel, 0x0B,0x10);
	panel_dsi_dcs_send_seq(panel, 0x0C,0xE6);
	panel_dsi_dcs_send_seq(panel, 0x0D,0x0D);
	panel_dsi_dcs_send_seq(panel, 0x0F,0x00);
	panel_dsi_dcs_send_seq(panel, 0x10,0x3D);
	panel_dsi_dcs_send_seq(panel, 0x11,0x4C);
	panel_dsi_dcs_send_seq(panel, 0x12,0xCF);
	panel_dsi_dcs_send_seq(panel, 0x13,0xAD);
	panel_dsi_dcs_send_seq(panel, 0x14,0x4A);
	panel_dsi_dcs_send_seq(panel, 0x15,0x92);
	panel_dsi_dcs_send_seq(panel, 0x16,0x24);
	panel_dsi_dcs_send_seq(panel, 0x17,0x55);
	panel_dsi_dcs_send_seq(panel, 0x18,0x73);
	panel_dsi_dcs_send_seq(panel, 0x19,0xE9);
	panel_dsi_dcs_send_seq(panel, 0x1A,0x70);
	panel_dsi_dcs_send_seq(panel, 0x1B,0x0E);
	panel_dsi_dcs_send_seq(panel, 0x1C,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x1D,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x1E,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x1F,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x20,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x21,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x22,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x23,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x24,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x25,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x26,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x27,0x1F);
	panel_dsi_dcs_send_seq(panel, 0x28,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x29,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x2A,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x2B,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x2C,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x2D,0x07);
	panel_dsi_dcs_send_seq(panel, 0x33,0x3F);
	panel_dsi_dcs_send_seq(panel, 0x35,0x7F);
	panel_dsi_dcs_send_seq(panel, 0x36,0x3F);
	panel_dsi_dcs_send_seq(panel, 0x38,0xFF);
	panel_dsi_dcs_send_seq(panel, 0x3A,0x80);
	panel_dsi_dcs_send_seq(panel, 0x3B,0x01);
	panel_dsi_dcs_send_seq(panel, 0x3C,0x80);
	panel_dsi_dcs_send_seq(panel, 0x3D,0x2C);
	panel_dsi_dcs_send_seq(panel, 0x3E,0x00);
	panel_dsi_dcs_send_seq(panel, 0x3F,0x90);
	panel_dsi_dcs_send_seq(panel, 0x40,0x05);
	panel_dsi_dcs_send_seq(panel, 0x41,0x00);
	panel_dsi_dcs_send_seq(panel, 0x42,0xB2);
	panel_dsi_dcs_send_seq(panel, 0x43,0x00);
	panel_dsi_dcs_send_seq(panel, 0x44,0x40);
	panel_dsi_dcs_send_seq(panel, 0x45,0x06);
	panel_dsi_dcs_send_seq(panel, 0x46,0x00);
	panel_dsi_dcs_send_seq(panel, 0x47,0x00);
	panel_dsi_dcs_send_seq(panel, 0x48,0x9B);
	panel_dsi_dcs_send_seq(panel, 0x49,0xD2);
	panel_dsi_dcs_send_seq(panel, 0x4A,0x21);
	panel_dsi_dcs_send_seq(panel, 0x4B,0x43);
	panel_dsi_dcs_send_seq(panel, 0x4C,0x16);
	panel_dsi_dcs_send_seq(panel, 0x4D,0xC0);
	panel_dsi_dcs_send_seq(panel, 0x4E,0x0F);
	panel_dsi_dcs_send_seq(panel, 0x4F,0xF1);
	panel_dsi_dcs_send_seq(panel, 0x50,0x78);
	panel_dsi_dcs_send_seq(panel, 0x51,0x7A);
	panel_dsi_dcs_send_seq(panel, 0x52,0x34);
	panel_dsi_dcs_send_seq(panel, 0x53,0x99);
	panel_dsi_dcs_send_seq(panel, 0x54,0xA2);
	panel_dsi_dcs_send_seq(panel, 0x55,0x02);
	panel_dsi_dcs_send_seq(panel, 0x56,0x14);
	panel_dsi_dcs_send_seq(panel, 0x57,0xB8);
	panel_dsi_dcs_send_seq(panel, 0x58,0xDC);
	panel_dsi_dcs_send_seq(panel, 0x59,0xD4);
	panel_dsi_dcs_send_seq(panel, 0x5A,0xEF);
	panel_dsi_dcs_send_seq(panel, 0x5B,0xF7);
	panel_dsi_dcs_send_seq(panel, 0x5C,0xFB);
	panel_dsi_dcs_send_seq(panel, 0x5D,0xFD);
	panel_dsi_dcs_send_seq(panel, 0x5E,0x7E);
	panel_dsi_dcs_send_seq(panel, 0x5F,0xBF);
	panel_dsi_dcs_send_seq(panel, 0x60,0xEF);
	panel_dsi_dcs_send_seq(panel, 0x61,0xE6);
	panel_dsi_dcs_send_seq(panel, 0x62,0x76);
	panel_dsi_dcs_send_seq(panel, 0x63,0x73);
	panel_dsi_dcs_send_seq(panel, 0x64,0xBB);
	panel_dsi_dcs_send_seq(panel, 0x65,0xDD);
	panel_dsi_dcs_send_seq(panel, 0x66,0x6E);
	panel_dsi_dcs_send_seq(panel, 0x67,0x37);
	panel_dsi_dcs_send_seq(panel, 0x68,0x8C);
	panel_dsi_dcs_send_seq(panel, 0x69,0x08);
	panel_dsi_dcs_send_seq(panel, 0x6A,0x31);
	panel_dsi_dcs_send_seq(panel, 0x6B,0xB8);
	panel_dsi_dcs_send_seq(panel, 0x6C,0xB8);
	panel_dsi_dcs_send_seq(panel, 0x6D,0xB8);
	panel_dsi_dcs_send_seq(panel, 0x6E,0xB8);
	panel_dsi_dcs_send_seq(panel, 0x6F,0xB8);
	panel_dsi_dcs_send_seq(panel, 0x70,0x5C);
	panel_dsi_dcs_send_seq(panel, 0x71,0x2E);
	panel_dsi_dcs_send_seq(panel, 0x72,0x17);
	panel_dsi_dcs_send_seq(panel, 0x73,0x00);
	panel_dsi_dcs_send_seq(panel, 0x74,0x00);
	panel_dsi_dcs_send_seq(panel, 0x75,0x00);
	panel_dsi_dcs_send_seq(panel, 0x76,0x00);
	panel_dsi_dcs_send_seq(panel, 0x77,0x00);
	panel_dsi_dcs_send_seq(panel, 0x78,0x00);
	panel_dsi_dcs_send_seq(panel, 0x79,0x00);
	panel_dsi_dcs_send_seq(panel, 0x7A,0xDC);
	panel_dsi_dcs_send_seq(panel, 0x7B,0xDC);
	panel_dsi_dcs_send_seq(panel, 0x7C,0xDC);
	panel_dsi_dcs_send_seq(panel, 0x7D,0xDC);
	panel_dsi_dcs_send_seq(panel, 0x7E,0xDC);
	panel_dsi_dcs_send_seq(panel, 0x7F,0x6E);
	panel_dsi_dcs_send_seq(panel, 0x0B,0x00);
	panel_dsi_dcs_send_seq(panel, 0xB1,0x03);
	panel_dsi_dcs_send_seq(panel, 0x2C,0x2C);
	panel_dsi_dcs_send_seq(panel, 0xB1,0x00);
	panel_dsi_dcs_send_seq(panel, 0x89,0x03);

	ret = panel_dsi_dcs_exit_sleep_mode(panel);
	if (ret < 0) {
		pr_err("Failed to exit sleep mode: %d\n", ret);
		return ret;
	}

	aic_delay_ms(200);

	ret = panel_dsi_dcs_set_display_on(panel);
	if (ret < 0) {
		pr_err("Failed to set display on: %d\n", ret);
		return ret;
	}

	aic_delay_ms(120);

	panel_dsi_setup_realmode(panel);
	panel_de_timing_enable(panel, 0);
	panel_backlight_enable(panel, 0);
	return 0;
}

static struct aic_panel_funcs panel_funcs = {
	.prepare = panel_default_prepare,
	.enable = panel_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct fb_videomode panel_vm = {
	.pixclock = 42000000,
	.xres = 400,
	.right_margin = 40,
	.left_margin = 40,
	.hsync_len = 30,
	.yres = 1280,
	.lower_margin = 30,
	.upper_margin = 30,
	.vsync_len = 20,
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST,
	.lane_num = 4,
};

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	struct xm91080 *xm91080;

	xm91080 = malloc(sizeof(*xm91080));
	if (!xm91080)
		return -ENOMEM;

	if (panel_parse_dts(dev) < 0) {
		free(xm91080);
		return -1;
	}

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, xm91080);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-xm91080"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_dsi_xm91080) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
