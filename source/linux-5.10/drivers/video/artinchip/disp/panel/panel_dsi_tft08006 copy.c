// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for tft08006 DSI panel.
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_tft08006"

struct tft08006 {
	struct gpio_desc *reset;
};

static inline struct tft08006 *panel_to_tft08006(struct aic_panel *panel)
{
	return (struct tft08006 *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct tft08006 *tft08006 = panel_to_tft08006(panel);
	int ret;

	//panel_di_enable(panel, 0);
	//aic_delay_ms(20);
	gpiod_direction_output(tft08006->reset, 1);
	aic_delay_ms(10);
	panel_di_enable(panel, 0);
	aic_delay_ms(10);
	panel_di_enable(panel, 1);
	aic_delay_ms(1);
	gpiod_direction_output(tft08006->reset, 0);
	aic_delay_ms(1);
	gpiod_direction_output(tft08006->reset, 1);
	aic_delay_ms(1);

	panel_dsi_send_perpare(panel);
	aic_delay_ms(150);


	panel_dsi_dcs_send_seq(panel, 0xFF, 0xff, 0x91, 0x06, 0x04, 0x01);

	panel_dsi_dcs_send_seq(panel, 0x08,0x10);
	panel_dsi_dcs_send_seq(panel, 0x20,0x00);
	panel_dsi_dcs_send_seq(panel, 0x21,0x01);
	panel_dsi_dcs_send_seq(panel, 0x30,0x02);
	panel_dsi_dcs_send_seq(panel, 0x31,0x02);
	panel_dsi_dcs_send_seq(panel, 0x60,0x07);
	panel_dsi_dcs_send_seq(panel, 0x61,0x06);
	panel_dsi_dcs_send_seq(panel, 0x62,0x06);
	panel_dsi_dcs_send_seq(panel, 0x63,0x04);
	panel_dsi_dcs_send_seq(panel, 0x40,0x18);
	panel_dsi_dcs_send_seq(panel, 0x41,0x33);
	panel_dsi_dcs_send_seq(panel, 0x42,0x11);
	panel_dsi_dcs_send_seq(panel, 0x43,0x09);
	panel_dsi_dcs_send_seq(panel, 0x44,0x0c);
	panel_dsi_dcs_send_seq(panel, 0x46,0x55);
	panel_dsi_dcs_send_seq(panel, 0x47,0x55);
	panel_dsi_dcs_send_seq(panel, 0x45,0x14);
	panel_dsi_dcs_send_seq(panel, 0x50,0x50);
	panel_dsi_dcs_send_seq(panel, 0x51,0x50);
	panel_dsi_dcs_send_seq(panel, 0x52,0x00);	
	panel_dsi_dcs_send_seq(panel, 0x53,0x38);
	
	panel_dsi_dcs_send_seq(panel, 0xA0,0x00);
	panel_dsi_dcs_send_seq(panel, 0xa1,0x09);
	panel_dsi_dcs_send_seq(panel, 0xa2,0x0c);
	panel_dsi_dcs_send_seq(panel, 0xa3,0x0f);
	panel_dsi_dcs_send_seq(panel, 0xa4,0x06);
	panel_dsi_dcs_send_seq(panel, 0xa5,0x09);
	panel_dsi_dcs_send_seq(panel, 0xa6,0x07);
	panel_dsi_dcs_send_seq(panel, 0xa7,0x16);
	panel_dsi_dcs_send_seq(panel, 0xa8,0x06);
	panel_dsi_dcs_send_seq(panel, 0xa9,0x09);
	panel_dsi_dcs_send_seq(panel, 0xaa,0x11);
	panel_dsi_dcs_send_seq(panel, 0xab,0x06);
	panel_dsi_dcs_send_seq(panel, 0xac,0x0e);
	panel_dsi_dcs_send_seq(panel, 0xad,0x19);
	panel_dsi_dcs_send_seq(panel, 0xae,0x0e);
	panel_dsi_dcs_send_seq(panel, 0xaf,0x00);
	panel_dsi_dcs_send_seq(panel, 0xc0,0x00);
	panel_dsi_dcs_send_seq(panel, 0xc1,0x09);
	panel_dsi_dcs_send_seq(panel, 0xc2,0x0c);
	panel_dsi_dcs_send_seq(panel, 0xc3,0x0f);

	panel_dsi_dcs_send_seq(panel, 0xc4,0x06);
	panel_dsi_dcs_send_seq(panel, 0xc5,0x09);
	panel_dsi_dcs_send_seq(panel, 0xc6,0x07);
	panel_dsi_dcs_send_seq(panel, 0xc7,0x16);
	panel_dsi_dcs_send_seq(panel, 0xc8,0x06);
	panel_dsi_dcs_send_seq(panel, 0xc9,0x09);
	panel_dsi_dcs_send_seq(panel, 0xca,0x11);
	panel_dsi_dcs_send_seq(panel, 0xcb,0x06);
	panel_dsi_dcs_send_seq(panel, 0xcc,0x0e);
	panel_dsi_dcs_send_seq(panel, 0xcd,0x19);
	panel_dsi_dcs_send_seq(panel, 0xce,0x0e);
	panel_dsi_dcs_send_seq(panel, 0xcf,0x00);
	
	panel_dsi_dcs_send_seq(panel, 0xff,0xff,0x98,0x06,0x04,0x06);
	
	panel_dsi_dcs_send_seq(panel, 0x00,0xa0);
	panel_dsi_dcs_send_seq(panel, 0x01,0x05);
	panel_dsi_dcs_send_seq(panel, 0x02,0x00);
	panel_dsi_dcs_send_seq(panel, 0x03,0x00);
	panel_dsi_dcs_send_seq(panel, 0x04,0x01);
	panel_dsi_dcs_send_seq(panel, 0x05,0x01);
	panel_dsi_dcs_send_seq(panel, 0x06,0x88);
	panel_dsi_dcs_send_seq(panel, 0x07,0x04);
	panel_dsi_dcs_send_seq(panel, 0x08,0x01);
	panel_dsi_dcs_send_seq(panel, 0x09,0x90);
	panel_dsi_dcs_send_seq(panel, 0x0a,0x04);
	panel_dsi_dcs_send_seq(panel, 0x0b,0x01);
	panel_dsi_dcs_send_seq(panel, 0x0c,0x01);
	panel_dsi_dcs_send_seq(panel, 0x0d,0x01);
	panel_dsi_dcs_send_seq(panel, 0x0e,0x00);
	panel_dsi_dcs_send_seq(panel, 0x0f,0x00);
	panel_dsi_dcs_send_seq(panel, 0x10,0x55);
	panel_dsi_dcs_send_seq(panel, 0x11,0x50);
	panel_dsi_dcs_send_seq(panel, 0x12,0x01);
	panel_dsi_dcs_send_seq(panel, 0x13,0x85);
	panel_dsi_dcs_send_seq(panel, 0x14,0x85);
	panel_dsi_dcs_send_seq(panel, 0x15,0xc0);
	panel_dsi_dcs_send_seq(panel, 0x16,0x0b);
	panel_dsi_dcs_send_seq(panel, 0x17,0x00);
	panel_dsi_dcs_send_seq(panel, 0x18,0x00);
	panel_dsi_dcs_send_seq(panel, 0x19,0x00);
	panel_dsi_dcs_send_seq(panel, 0x1a,0x00);
	panel_dsi_dcs_send_seq(panel, 0x1b,0x00);
	panel_dsi_dcs_send_seq(panel, 0x1c,0x00);
	panel_dsi_dcs_send_seq(panel, 0x1d,0x00);
	panel_dsi_dcs_send_seq(panel, 0x20,0x01);

	panel_dsi_dcs_send_seq(panel, 0x21,0x23);
	panel_dsi_dcs_send_seq(panel, 0x22,0x45);
	panel_dsi_dcs_send_seq(panel, 0x23,0x67);
	panel_dsi_dcs_send_seq(panel, 0x24,0x01);
	panel_dsi_dcs_send_seq(panel, 0x25,0x23);
	panel_dsi_dcs_send_seq(panel, 0x26,0x45);
	panel_dsi_dcs_send_seq(panel, 0x27,0x67);
	panel_dsi_dcs_send_seq(panel, 0x30,0x02);
	panel_dsi_dcs_send_seq(panel, 0x31,0x22);
	panel_dsi_dcs_send_seq(panel, 0x32,0x11);
	panel_dsi_dcs_send_seq(panel, 0x33,0xaa);
	panel_dsi_dcs_send_seq(panel, 0x34,0xbb);
	panel_dsi_dcs_send_seq(panel, 0x35,0x66);
	panel_dsi_dcs_send_seq(panel, 0x36,0x00);
	panel_dsi_dcs_send_seq(panel, 0x37,0x22);
	panel_dsi_dcs_send_seq(panel, 0x38,0x22);
	panel_dsi_dcs_send_seq(panel, 0x39,0x22);
	panel_dsi_dcs_send_seq(panel, 0x3a,0x22);
	panel_dsi_dcs_send_seq(panel, 0x3b,0x22);
	panel_dsi_dcs_send_seq(panel, 0x3c,0x22);

	panel_dsi_dcs_send_seq(panel, 0x3d,0x20);
	panel_dsi_dcs_send_seq(panel, 0x3e,0x22);
	panel_dsi_dcs_send_seq(panel, 0x3f,0x22);
	panel_dsi_dcs_send_seq(panel, 0x40,0x22);
	panel_dsi_dcs_send_seq(panel, 0x52,0x12);
	panel_dsi_dcs_send_seq(panel, 0x53,0x12);
	
	panel_dsi_dcs_send_seq(panel, 0xff,0xff,0x98,0x06,0x04,0x07);
	panel_dsi_dcs_send_seq(panel, 0x17,0x32);
	panel_dsi_dcs_send_seq(panel, 0x02,0x17);
	panel_dsi_dcs_send_seq(panel, 0x18,0x1d);
	panel_dsi_dcs_send_seq(panel, 0xe1,0x79);
	
	panel_dsi_dcs_send_seq(panel, 0xff,0xff,0x98,0x06,0x04,0x00);
	panel_dsi_dcs_send_seq(panel, 0x3a,0x70);
	panel_dsi_dcs_send_seq(panel, 0x11);

	aic_delay_ms(150);
	panel_dsi_dcs_send_seq(panel, 0x29);
	aic_delay_ms(150);

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

static int panel_disable(struct aic_panel *panel)
{
	struct tft08006 *tft08006 = panel_to_tft08006(panel);

	panel_default_disable(panel);

	gpiod_direction_output(tft08006->reset, 0);
	aic_delay_ms(10);

	return 0;
}

static struct aic_panel_funcs panel_funcs = {
	.disable = panel_disable,
	.unprepare = panel_default_unprepare,
	.prepare = panel_default_prepare,
	.enable = panel_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct videomode panel_vm = {
	.pixelclock = 25600000,
	.hactive = 480,
	.hfront_porch = 10,
	.hback_porch = 20,
	.hsync_len = 5,
	.vactive = 800,
	.vfront_porch = 5,
	.vback_porch = 20,
	.vsync_len = 5,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST,
	.lane_num = 2,
};

static int panel_bind(struct device *dev, struct device *master, void *data)
{
	struct panel_comp *p;
	struct tft08006 *tft08006;
		
	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	tft08006 = devm_kzalloc(dev, sizeof(*tft08006), GFP_KERNEL);
	if (!tft08006)
		return -ENOMEM;

	tft08006->reset = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(tft08006->reset)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(tft08006->reset);
	}

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	p->panel.dsi = &dsi;
	panel_init(p, dev, &panel_vm, &panel_funcs, tft08006);

	dev_set_drvdata(dev, p);
	return 0;
}

static const struct component_ops panel_com_ops = {
	.bind	= panel_bind,
	.unbind	= panel_default_unbind,
};

static int panel_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &panel_com_ops);
}

static int panel_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &panel_com_ops);
	return 0;
}

static const struct of_device_id panal_of_table[] = {
	{.compatible = "artinchip,dsi_panel_tft08006"},
	{ /* sentinel */}
};
MODULE_DEVICE_TABLE(of, panal_of_table);

static struct platform_driver panel_driver = {
	.probe = panel_probe,
	.remove = panel_remove,
	.driver = {
		.name = PANEL_DEV_NAME,
		.of_match_table = panal_of_table,
	},
};

module_platform_driver(panel_driver);

MODULE_AUTHOR("huahui.mai <huahui.mai@artinchip.com>");
MODULE_DESCRIPTION("AIC-" PANEL_DEV_NAME);
MODULE_LICENSE("GPL");
