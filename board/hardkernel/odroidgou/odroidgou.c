/*
 * board/hardkernel/odroidn2/odroidn2.c
 *
 * (C) Copyright 2018 Hardkernel Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <environment.h>
#include <fdt_support.h>
#include <libfdt.h>
#include <asm/cpu_id.h>
#include <asm/arch/secure_apb.h>
#ifdef CONFIG_SYS_I2C_AML
#include <amlogic/i2c.h>
#include <aml_i2c.h>
#endif
#ifdef CONFIG_AML_VPU
#include <vpu.h>
#endif
#include <vpp.h>
#ifdef CONFIG_AML_LCD
#include <amlogic/aml_lcd.h>
#endif
#include <asm/arch/eth_setup.h>
#include <phy.h>
#include <linux/mtd/partitions.h>
#include <linux/sizes.h>
#include <asm-generic/gpio.h>
#include <dm.h>
#include <asm/arch/usb-v2.h>
#include <odroid-common.h>
#include "display.h"
#include "recovery.h"
#include "odroid_pmic.h"

DECLARE_GLOBAL_DATA_PTR;

int serial_set_pin_port(unsigned long port_base)
{
	//UART in "Always On Module"
	//GPIOAO_0==tx,GPIOAO_1==rx
	//setbits_le32(P_AO_RTI_PIN_MUX_REG,3<<11);
	return 0;
}

int dram_init(void)
{
	gd->ram_size = (((readl(AO_SEC_GP_CFG0)) & 0xFFFF0000) << 4);
	return 0;
}

/* secondary_boot_func
 * this function should be write with asm, here, is is only for compiling pass
 * */
void secondary_boot_func(void)
{
}

int board_eth_init(bd_t *bis)
{
	return 0;
}

#if CONFIG_AML_SD_EMMC
#include <mmc.h>
#include <asm/arch/sd_emmc.h>
static int  sd_emmc_init(unsigned port)
{
	switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
			//todo add card detect
			/* check card detect */
			clrbits_le32(P_PERIPHS_PIN_MUX_9, 0xF << 24);
			setbits_le32(P_PREG_PAD_GPIO1_EN_N, 1 << 6);
			setbits_le32(P_PAD_PULL_UP_EN_REG1, 1 << 6);
			setbits_le32(P_PAD_PULL_UP_REG1, 1 << 6);
			break;
		case SDIO_PORT_C:
			//enable pull up
			//clrbits_le32(P_PAD_PULL_UP_REG3, 0xff<<0);
			break;
		default:
			break;
	}

	return cpu_sd_emmc_init(port);
}

extern unsigned sd_debug_board_1bit_flag;


static void sd_emmc_pwr_prepare(unsigned port)
{
	cpu_sd_emmc_pwr_prepare(port);
}

static void sd_emmc_pwr_on(unsigned port)
{
	switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
			//            clrbits_le32(P_PREG_PAD_GPIO5_O,(1<<31)); //CARD_8
			//            clrbits_le32(P_PREG_PAD_GPIO5_EN_N,(1<<31));
			/// @todo NOT FINISH
			break;
		case SDIO_PORT_C:
			break;
		default:
			break;
	}
	return;
}
static void sd_emmc_pwr_off(unsigned port)
{
	/// @todo NOT FINISH
	switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
			//            setbits_le32(P_PREG_PAD_GPIO5_O,(1<<31)); //CARD_8
			//            clrbits_le32(P_PREG_PAD_GPIO5_EN_N,(1<<31));
			break;
		case SDIO_PORT_C:
			break;
		default:
			break;
	}
	return;
}

// #define CONFIG_TSD      1
static void board_mmc_register(unsigned port)
{
	struct aml_card_sd_info *aml_priv=cpu_sd_emmc_get(port);
	if (aml_priv == NULL)
		return;

	aml_priv->sd_emmc_init=sd_emmc_init;
	aml_priv->sd_emmc_detect=sd_emmc_detect;
	aml_priv->sd_emmc_pwr_off=sd_emmc_pwr_off;
	aml_priv->sd_emmc_pwr_on=sd_emmc_pwr_on;
	aml_priv->sd_emmc_pwr_prepare=sd_emmc_pwr_prepare;
	aml_priv->desc_buf = malloc(NEWSD_MAX_DESC_MUN*(sizeof(struct sd_emmc_desc_info)));

	if (NULL == aml_priv->desc_buf)
		printf(" desc_buf Dma alloc Fail!\n");
	else
		printf("aml_priv->desc_buf = 0x%p\n",aml_priv->desc_buf);

	sd_emmc_register(aml_priv);
}
int board_mmc_init(bd_t	*bis)
{
	board_mmc_register(SDIO_PORT_C);	// eMMC
	board_mmc_register(SDIO_PORT_B);	// SD card

	return 0;
}
#endif

#ifdef CONFIG_SYS_I2C_AML
struct aml_i2c_platform g_aml_i2c_plat[] = {
	{
		.wait_count         = 1000000,
		.wait_ack_interval  = 5,
		.wait_read_interval = 5,
		.wait_xfer_interval = 5,
		.master_no          = AML_I2C_MASTER_AO,
		.use_pio            = 0,
		.master_i2c_speed   = AML_I2C_SPPED_400K,
		.master_ao_pinmux = {
			.scl_reg    = (unsigned long)MESON_I2C_MASTER_AO_GPIOAO_2_REG,
			.scl_bit    = MESON_I2C_MASTER_AO_GPIOAO_2_BIT,
			.sda_reg    = (unsigned long)MESON_I2C_MASTER_AO_GPIOAO_3_REG,
			.sda_bit    = MESON_I2C_MASTER_AO_GPIOAO_3_BIT,
		},
	},
	{
		.wait_count         = 1000000,
		.wait_ack_interval  = 5,
		.wait_read_interval = 5,
		.wait_xfer_interval = 5,
		.master_no          = AML_I2C_MASTER_D,
		.use_pio            = 0,
		.master_i2c_speed   = AML_I2C_SPPED_400K,
		.master_d_pinmux = {
			.scl_reg    = (unsigned long)MESON_I2C_MASTER_D_GPIOA_15_REG,
			.scl_bit    = MESON_I2C_MASTER_D_GPIOA_15_BIT,
			.sda_reg    = (unsigned long)MESON_I2C_MASTER_D_GPIOA_14_REG,
			.sda_bit    = MESON_I2C_MASTER_D_GPIOA_14_BIT,
		},
	},
};

static void board_i2c_init(void)
{
	//Amlogic I2C controller initialized
	//note: it must be call before any I2C operation
	//aml_i2c_init();

	extern void aml_i2c_set_ports(struct aml_i2c_platform *i2c_plat);
	aml_i2c_set_ports(g_aml_i2c_plat);
}

#endif

#if defined(CONFIG_BOARD_EARLY_INIT_F)
int board_early_init_f(void){
	/* setup the default voltage : VDD_EE */
	if (board_revision() < BOARD_REVISION(2018, 12, 6))
		writel(0x100016, AO_PWM_PWM_B);	/* 0.8420V */

	return 0;
}
#endif

#ifdef CONFIG_USB_XHCI_AMLOGIC_V2
#include <asm/arch/usb-v2.h>
#include <asm/arch/gpio.h>
#define CONFIG_GXL_USB_U2_PORT_NUM	2

#ifdef CONFIG_USB_XHCI_AMLOGIC_USB3_V2
#define CONFIG_GXL_USB_U3_PORT_NUM	1
#else
#define CONFIG_GXL_USB_U3_PORT_NUM	0
#endif

static void gpio_set_vbus_power(char is_power_on)
{
	return;
}

struct amlogic_usb_config g_usb_config_GXL_skt={
	CONFIG_GXL_XHCI_BASE,
	USB_ID_MODE_HARDWARE,
	gpio_set_vbus_power,//gpio_set_vbus_power, //set_vbus_power
	CONFIG_GXL_USB_PHY2_BASE,
	CONFIG_GXL_USB_PHY3_BASE,
	CONFIG_GXL_USB_U2_PORT_NUM,
	CONFIG_GXL_USB_U3_PORT_NUM,
	.usb_phy2_pll_base_addr = {
		CONFIG_USB_PHY_20,
		CONFIG_USB_PHY_21,
	}
};

#endif /*CONFIG_USB_XHCI_AMLOGIC*/



int board_init(void)
{
	board_led_alive(1);

#ifdef CONFIG_USB_XHCI_AMLOGIC_V2
	board_usb_pll_disable(&g_usb_config_GXL_skt);
	board_usb_init(&g_usb_config_GXL_skt,BOARD_USB_MODE_HOST);
#endif /*CONFIG_USB_XHCI_AMLOGIC*/

	return 0;
}

#if !defined(CONFIG_FASTBOOT_FLASH_MMC_DEV)
#define CONFIG_FASTBOOT_FLASH_MMC_DEV		0
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#if defined(CONFIG_FASTBOOT_FLASH_MMC_DEV)
	/* select the default mmc device */
	int mmc_devnum = CONFIG_FASTBOOT_FLASH_MMC_DEV;

	if (get_boot_device() == BOOT_DEVICE_EMMC)
		mmc_devnum = 0;
	else if (get_boot_device() == BOOT_DEVICE_SD)
		mmc_devnum = 1;

	/* select the default mmc device */
	mmc_select_hwpart(mmc_devnum, 0);
#endif
#ifdef CONFIG_SYS_I2C_AML
	board_i2c_init();
#endif

#ifdef CONFIG_ODROID_PMIC
	odroid_pmic_init();
#endif

#ifdef CONFIG_AML_VPU
	vpu_probe();
#endif
	vpp_init();

	check_hotkey();

	if((get_bootmode() != BOOTMODE_NORMAL) || (board_check_power() < 0)) {
#ifdef CONFIG_AML_LCD
		gou_init_lcd();
#endif
	}

	setenv("variant", "gou");
	board_set_dtbfile("meson64_odroid%s.dtb");

	if (board_check_recovery() < 0) {
		gou_bmp_display(DISP_SYS_ERR);
		mdelay(4000);
		run_command("poweroff", 0);
	} else {
		if (board_check_power() < 0) {
			gou_bmp_display(DISP_BATT_LOW);
			mdelay(2000);
			run_command("poweroff", 0);
		}
	}
	switch (get_bootmode()) {
		case BOOTMODE_RECOVERY :
			gou_bmp_display(DISP_RECOVERY);
			mdelay(2000);
		break;
		case BOOTMODE_TEST :
			gou_bmp_display(DISP_TEST);
			mdelay(2000);
		break;
		default :
			gou_bmp_display(DISP_LOGO);
		break;
	}
	return 0;
}
#endif

/* SECTION_SHIFT is 29 that means 512MB size */
#define SECTION_SHIFT		29
phys_size_t get_effective_memsize(void)
{
	phys_size_t size_aligned;

	size_aligned = (((readl(AO_SEC_GP_CFG0)) & 0xFFFF0000) << 4);
	size_aligned = ((size_aligned >> SECTION_SHIFT) << SECTION_SHIFT);

#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	size_aligned = size_aligned - CONFIG_SYS_MEM_TOP_HIDE;
#endif

	return size_aligned;
}
