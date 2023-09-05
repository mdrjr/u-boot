/*
 * board/hardkernel/odroidgou/odroid_pmic.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * add by: renjun.xu@amlogic.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <asm-generic/gpio.h>

#ifdef CONFIG_OF_LIBFDT
#include <libfdt.h>
#endif

#include <power/battery.h>
#include <power/rk818_pmic.h>
#include <power/rockchip_power.h>

#include "odroid_pmic.h"
#include "display.h"
#include "recovery.h"

void i2c_check_bus(unsigned int master_num)
{
	extern unsigned int i2c_get_bus_num(void);
	extern int i2c_set_bus_num(unsigned int busnum);

	if (master_num != i2c_get_bus_num())
		i2c_set_bus_num(master_num);
}

#define CHG_LED GPIOAO(GPIOAO_6)  // red
#define SYS_LED GPIOAO(GPIOAO_11) // blue

void charger_led_bilnk(unsigned char with_sysled)
{
	gpio_request(CHG_LED, "chg_led");
	gpio_request(SYS_LED, "sys_led");
	int onoff = !gpio_get_value(CHG_LED);

	gpio_direction_output(CHG_LED, onoff);
	if(with_sysled)
		gpio_direction_output(SYS_LED, !onoff);
	else
		gpio_direction_output(SYS_LED, 1);

	gpio_free(CHG_LED);
	gpio_free(SYS_LED);
}
void charger_led_off(void)
{
	gpio_request(CHG_LED, "chg_led");
	gpio_direction_output(CHG_LED, 0);
	gpio_free(CHG_LED);
}

int check_charge_exit_key(void)
{
	int m_left,m_right;
	int exit = 0;

	gpio_request(KEY_MENU_LEFT, "menu_left");
	gpio_request(KEY_MENU_RIGHT, "menu_right");

	gpio_direction_input(KEY_MENU_LEFT);
	gpio_direction_input(KEY_MENU_RIGHT);

	//key active low
	m_left = !gpio_get_value(KEY_MENU_LEFT);
	m_right = !gpio_get_value(KEY_MENU_RIGHT);

	if (m_left && m_right) {
		exit = 1;
		printf("Charge Exit. Normal Boot\n");
	} else {
		exit = 0;
	}
	return exit;
}


static int rk818_i2c_write(int32_t command, uint8_t val)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	struct i2c_msg msg;

	i2c_check_bus(RK818_I2C_BUS);

	msg.addr = RK818_CHIP_ADDR;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	buf[0] = command & 0xff;
	buf[1] = val & 0xff;

	ret = aml_i2c_xfer(&msg, 1);
	if (ret < 0)
		printf("i2c write failed [addr 0x%02x]\n", msg.addr);

	return ret;
}

static int rk817_i2c_write(int32_t command, uint8_t val)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	struct i2c_msg msg;

	i2c_check_bus(RK817_I2C_BUS);

	msg.addr = RK817_CHIP_ADDR;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	buf[0] = command & 0xff;
	buf[1] = val & 0xff;

	ret = aml_i2c_xfer(&msg, 1);
	if (ret < 0)
		printf("i2c write failed [addr 0x%02x]\n", msg.addr);

	return ret;
}

void rk817_shutdown(void)
{
	/* RK817 LDO disable */
	rk817_i2c_write(RK817_POWER_EN1, 0xf0);
	rk817_i2c_write(RK817_POWER_EN2, 0xf0);
	rk817_i2c_write(RK817_POWER_EN3, 0xf0);
}

int odroid_check_pwron_src(void)
{
	int ret = PWRON_KEY;
	u8 reg = rk818_pwron_source();

	if ( reg & 0x80) {
		printf("PWRON source : PWRON_KEY\n");
		ret = PWRON_KEY;
	} else if ( reg & 0x40) {
		printf("PWRON source : PWRON_USB\n");
		ret = PWRON_USB;
	} else if ( reg & 0x20) {
		printf("PWRON source : PWRON_RTC\n");
		ret = PWRON_RTC;
	} else if ( reg & 0x10) {
		printf("PWRON source : PWRON_RESET\n");
		ret = PWRON_RESET;
	} else if ( reg & 0x08) {
		printf("PWRON source : PWRON_KEY Long press\n");
		ret = PWRON_KEY_LP;
	} else if ( reg & 0x04) {
		printf("PWRON source : PWRON_KEY Recovery\n");
		ret = PWRON_RECOVER;
	} else {
		printf("PWRON source : Unknown source, set default PWRON_KEY\n");
		ret = PWRON_KEY;
	}

	return ret;
}

int odroid_charge_enable(struct pmic *p_fg)
{
	if (p_fg->pbat->battery_charge)
		p_fg->pbat->battery_charge(p_fg);

	if (p_fg->fg->fg_battery_update)
		p_fg->fg->fg_battery_update(p_fg, p_fg);

	return 0;
}

int board_check_power(void)
{
	int pwron_src, bootmode;
	unsigned int cap = 0, offset=0;

	// check pwr on source
	pwron_src = odroid_check_pwron_src();
	bootmode = get_bootmode();
	printf("PWRON source : %d\n",pwron_src);

	if ((pwron_src != PWRON_KEY) && (bootmode == BOOTMODE_NORMAL)) {
		if((pwron_src == PWRON_USB) && (get_battery_cap() > 0))
			return 0;
		else
			gou_init_lcd();
		
		/* RK817 BOOST, OTG_POWER(USB A-type VBUS) disable */
		rk817_i2c_write(RK817_POWER_EN3, 0xf0);
		printf("battery charge state\n");
		while(1) {
			cap = get_battery_cap();
			printf("rk818_bat: capacity=[%d]\n",cap);
			if (cap < 25)
				offset = DISP_BATT_0;
			else if (cap < 60)
				offset = DISP_BATT_1;
			else if (cap < 95)
				offset = DISP_BATT_2;
			else
				offset = DISP_BATT_3;

			gou_bmp_display(offset);
			mdelay(750);
			charger_led_bilnk(0);
			if ( offset < DISP_BATT_3)
				gou_bmp_display(offset+1);
			else 
				gou_bmp_display(offset);

			mdelay(750);
			charger_led_bilnk(0);

			if(check_charge_exit_key())
				break;

			if(!is_charging())
				run_command("poweroff", 0);
		}
	}
	if(is_power_low() && !is_charging()) return -1;

	return 0;
}
/* Set default buck, ldo voltage */
void odroid_pmic_init(void)
{
	printf("enter odroid_pmic_init.\n");

	/* RK817 DCDC, LDO reset */
	rk817_i2c_write(RK817_SYS_CFG(3), 0x64);

#if defined(CONFIG_POWER_RK818)
	int ret = -1;

	/* RK818 BUCK4(vddao_3v3) : 3.3V */
	rk818_i2c_write(RK818_BUCK4_ON_VSEL, 0x0c);
	/* RK818 BUCK1(vddcpu_a) : 1.0375V */
	rk818_i2c_write(RK818_BUCK1_ON_VSEL, 0x1a);
	/* RK818 BUCK2(vdd_ee) : 0.875V */
	rk818_i2c_write(RK818_BUCK2_ON_VSEL, 0x0d);
	/* RK818 LDO9(vddio_c) uSD IO voltage : 3.3V */
	rk818_i2c_write(RK818_LDO9_ON_VSEL, 0xaf);

	/* RK818 BUCK,LDO9,SWITCH enable */
	rk818_i2c_write(RK818_DCDC_EN_REG, 0x6f);
	/* RK818 LDO7,LDO5 enable */
	/* RK818 LDO1,LDO2,LDO3,LDO4,LDO6,LDO8 disable */
	rk818_i2c_write(RK818_LDO_EN_REG, 0x50);

	ret = pmic_init(RK818_I2C_BUS);
	if (ret >= 0) printf("pmic:rk818 init complete.\n");
	else printf("pmic:rk818 init fail....\n");

	if(is_power_low() && is_charging()) {
		while(1) {
			if(!is_power_low())
				break;
			mdelay(750);
			charger_led_bilnk(1);
			if(!is_charging())
				run_command("poweroff", 0);
		}
	}
#endif
	/* RK817 BUCK2(vddcpu_b) : 1.0375V */
	rk817_i2c_write(RK817_BUCK2_ON_VSEL, 0x2b);
	/* RK817 BUCK3 default : 2.4V */
	rk817_i2c_write(RK817_BUCK3_ON_VSEL, 0x59);
	/* RK817 LDO8(vdd_lcd) : 3.3V */
	rk817_i2c_write(RK817_LDO8_ON_VSEL, 0x6c);

	/* RK817 BOOST enable */
	rk817_i2c_write(RK817_POWER_EN3, 0xf7);
	/* RK817 LDO4,LDO8 enable */
	rk817_i2c_write(RK817_POWER_EN2, 0xf8);
	/* RK817 LDO1,LDO2,LDO3 disable */
	/* RK817 LDO5,LDO6,LDO7 disable */
	rk817_i2c_write(RK817_POWER_EN1, 0xf8);
	/* RK817 BUCK2,BUCK3 enable */
	rk817_i2c_write(RK817_POWER_EN0, 0xf6);

	printf("leave odroid_pmic_init.\n");
	return;
}

int do_poweroff(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	rk817_shutdown();
	pmic_rk818_shut_down();
	return (0);
}

U_BOOT_CMD(poweroff, 1, 1, do_poweroff, "Switch off power", "");