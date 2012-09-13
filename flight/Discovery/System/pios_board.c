/**
 ******************************************************************************
 * @addtogroup OpenPilotSystem OpenPilot System
 * @{
 * @addtogroup OpenPilotCore OpenPilot Core
 * @{
 *
 * @file       pios_board.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Defines board specific static initializers for hardware for the OpenPilot board.
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/* 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* Pull in the board-specific static HW definitions.
 * Including .c files is a bit ugly but this allows all of
 * the HW definitions to be const and static to limit their
 * scope.  
 *
 * NOTE: THIS IS THE ONLY PLACE THAT SHOULD EVER INCLUDE THIS FILE
 */

#include <pios.h>

#include <openpilot.h>
#include <uavobjectsinit.h>
#include "hwsettings.h"
#include "manualcontrolsettings.h"
#include "gcsreceiver.h"

#include "board_hw_defs.c"


/* One slot per selectable receiver group.
 *  eg. PWM, PPM, GCS, DSMMAINPORT, DSMFLEXIPORT, SBUS
 * NOTE: No slot in this map for NONE.
 */
uint32_t pios_rcvr_group_map[MANUALCONTROLSETTINGS_CHANNELGROUPS_NONE];

#define PIOS_COM_TELEM_RF_RX_BUF_LEN 512
#define PIOS_COM_TELEM_RF_TX_BUF_LEN 512

#define PIOS_COM_GPS_RX_BUF_LEN 32

#define PIOS_COM_TELEM_USB_RX_BUF_LEN 65
#define PIOS_COM_TELEM_USB_TX_BUF_LEN 65

#define PIOS_COM_BRIDGE_RX_BUF_LEN 65
#define PIOS_COM_BRIDGE_TX_BUF_LEN 12

uint32_t pios_com_telem_rf_id;
uint32_t pios_com_telem_usb_id;
uint32_t pios_com_vcp_id;
uint32_t pios_com_gps_id;
uint32_t pios_com_bridge_id;

/* 
 * Setup a com port based on the passed cfg, driver and buffer sizes. tx size of -1 make the port rx only
 */
static void PIOS_Board_configure_com(const struct pios_usart_cfg *usart_port_cfg, size_t rx_buf_len, size_t tx_buf_len,
		const struct pios_com_driver *com_driver, uint32_t *pios_com_id) 
{
	uint32_t pios_usart_id;
	if (PIOS_USART_Init(&pios_usart_id, usart_port_cfg)) {
		PIOS_Assert(0);
	}
	
	uint8_t * rx_buffer = (uint8_t *) pvPortMalloc(rx_buf_len);
	PIOS_Assert(rx_buffer);
	if(tx_buf_len!= -1){ // this is the case for rx/tx ports
		uint8_t * tx_buffer = (uint8_t *) pvPortMalloc(tx_buf_len);
		PIOS_Assert(tx_buffer);
		
		if (PIOS_COM_Init(pios_com_id, com_driver, pios_usart_id,
				rx_buffer, rx_buf_len,
				tx_buffer, tx_buf_len)) {
			PIOS_Assert(0);
		}
	}
	else{ //rx only port
		if (PIOS_COM_Init(pios_com_id, com_driver, pios_usart_id,
				rx_buffer, rx_buf_len,
				NULL, 0)) {
			PIOS_Assert(0);
		}
	}
}

static void PIOS_Board_configure_dsm(const struct pios_usart_cfg *pios_usart_dsm_cfg, const struct pios_dsm_cfg *pios_dsm_cfg, 
		const struct pios_com_driver *pios_usart_com_driver,enum pios_dsm_proto *proto, 
		ManualControlSettingsChannelGroupsOptions channelgroup,uint8_t *bind)
{
	uint32_t pios_usart_dsm_id;
	if (PIOS_USART_Init(&pios_usart_dsm_id, pios_usart_dsm_cfg)) {
		PIOS_Assert(0);
	}
	
	uint32_t pios_dsm_id;
	if (PIOS_DSM_Init(&pios_dsm_id, pios_dsm_cfg, pios_usart_com_driver,
			pios_usart_dsm_id, *proto, *bind)) {
		PIOS_Assert(0);
	}
	
	uint32_t pios_dsm_rcvr_id;
	if (PIOS_RCVR_Init(&pios_dsm_rcvr_id, &pios_dsm_rcvr_driver, pios_dsm_id)) {
		PIOS_Assert(0);
	}
	pios_rcvr_group_map[channelgroup] = pios_dsm_rcvr_id;
}


/**
 * Configuration for the MPU6050 chip
 */
#if defined(PIOS_INCLUDE_MPU6050)
#include "pios_mpu6050.h"
static const struct pios_exti_cfg pios_exti_mpu6050_cfg __exti_config = {
	.vector = PIOS_MPU6050_IRQHandler,
	.line = EXTI_Line11,
	.pin = {
		.gpio = GPIOD,
		.init = {
			.GPIO_Pin = GPIO_Pin_11,
			.GPIO_Speed = GPIO_Speed_100MHz,
			.GPIO_Mode = GPIO_Mode_IN,
			.GPIO_OType = GPIO_OType_OD,
			.GPIO_PuPd = GPIO_PuPd_NOPULL,
		},
	},
	.irq = {
		.init = {
			.NVIC_IRQChannel = EXTI15_10_IRQn,
			.NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_MID,
			.NVIC_IRQChannelSubPriority = 0,
			.NVIC_IRQChannelCmd = ENABLE,
		},
	},
	.exti = {
		.init = {
			.EXTI_Line = EXTI_Line11, // matches above GPIO pin
			.EXTI_Mode = EXTI_Mode_Interrupt,
			.EXTI_Trigger = EXTI_Trigger_Rising,
			.EXTI_LineCmd = ENABLE,
		},
	},
};

static const struct pios_mpu6050_cfg pios_mpu6050_cfg = {
	.exti_cfg = &pios_exti_mpu6050_cfg,
	.Fifo_store = PIOS_MPU6050_FIFO_TEMP_OUT | PIOS_MPU6050_FIFO_GYRO_X_OUT | PIOS_MPU6050_FIFO_GYRO_Y_OUT | PIOS_MPU6050_FIFO_GYRO_Z_OUT,
	// Clock at 8kHz, downsampled by 8 for 1kHz
	.Smpl_rate_div = 8 - 1,
	.interrupt_cfg = PIOS_MPU6050_INT_CLR_ANYRD,
	.interrupt_en = PIOS_MPU6050_INTEN_DATA_RDY,
	.User_ctl = PIOS_MPU6050_USERCTL_FIFO_EN,
	.Pwr_mgmt_clk = PIOS_MPU6050_PWRMGMT_PLL_X_CLK,
	.accel_range = PIOS_MPU6050_ACCEL_8G,
	.gyro_range = PIOS_MPU6050_SCALE_500_DEG,
	.filter = PIOS_MPU6050_LOWPASS_256_HZ
};
#endif /* PIOS_INCLUDE_MPU6050 */

static const struct flashfs_cfg flashfs_m25p_cfg = {
	.table_magic = 0x85FB3D35,
	.obj_magic = 0x3015A371,
	.obj_table_start = 0x00000010,
	.obj_table_end = 0x00010000,
	.sector_size = 0x00010000,
	.chip_size = 0x00800000,
};

static const struct pios_flash_jedec_cfg flash_m25p_cfg = {
	.sector_erase = 0xD8,
	.chip_erase = 0xC7
};

#include <pios_board_info.h>
/**
 * PIOS_Board_Init()
 * initializes all the core subsystems on this specific hardware
 * called from System/openpilot.c
 */
void PIOS_Board_Init(void) {

	/* Delay system */
	PIOS_DELAY_Init();
	
#if defined(PIOS_INCLUDE_LED)
	PIOS_LED_Init(&pios_led_cfg);
#endif	/* PIOS_INCLUDE_LED */
	

	if (PIOS_SPI_Init(&pios_spi_flash_id, &pios_spi_flash_cfg)) {
		PIOS_Assert(0);
	}
	PIOS_Flash_Jedec_Init(pios_spi_flash_id, 0, &flash_m25p_cfg);	
	PIOS_FLASHFS_Init(&flashfs_m25p_cfg);
	
	
	/* Initialize UAVObject libraries */
	EventDispatcherInitialize();
	UAVObjInitialize();

#if defined(PIOS_INCLUDE_RTC)
	/* Initialize the real-time clock and its associated tick */
	PIOS_RTC_Init(&pios_rtc_main_cfg);
#endif

	HwSettingsInitialize();

#ifndef ERASE_FLASH
	/* Initialize watchdog as early as possible to catch faults during init */
#ifndef DEBUG
	PIOS_WDG_Init();
#endif
#endif

	/* Initialize the alarms library */
	AlarmsInitialize();

	/* Initialize the task monitor library */
	TaskMonitorInitialize();

	/* Set up pulse timers */
	//inputs
	PIOS_TIM_InitClock(&tim_1_cfg);
	PIOS_TIM_InitClock(&tim_3_cfg);
	//outputs
	PIOS_TIM_InitClock(&tim_2_cfg);
	PIOS_TIM_InitClock(&tim_4_cfg);
	PIOS_TIM_InitClock(&tim_8_cfg);
	PIOS_TIM_InitClock(&tim_9_cfg);

	/* Check for repeated boot failures */
	PIOS_IAP_Init();
	uint16_t boot_count = PIOS_IAP_ReadBootCount();
	if (boot_count < 3) {
		PIOS_IAP_WriteBootCount(++boot_count);
		AlarmsClear(SYSTEMALARMS_ALARM_BOOTFAULT);
	} else {
		/* Too many failed boot attempts, force hwsettings to defaults */
		HwSettingsSetDefaults(HwSettingsHandle(), 0);
		AlarmsSet(SYSTEMALARMS_ALARM_BOOTFAULT, SYSTEMALARMS_ALARM_CRITICAL);
	}

#if defined(PIOS_INCLUDE_USB)
	/* Initialize board specific USB data */
	PIOS_USB_BOARD_DATA_Init();


	/* Flags to determine if various USB interfaces are advertised */
	bool usb_hid_present = false;
	bool usb_cdc_present = false;

	if (PIOS_USB_DESC_HID_ONLY_Init()) {
		PIOS_Assert(0);
	}
	usb_hid_present = true;

	uint32_t pios_usb_id;
	PIOS_USB_Init(&pios_usb_id, &pios_usb_main_cfg);

#if defined(PIOS_INCLUDE_USB_HID)
	/* Configure the usb HID port */
	uint8_t hwsettings_usb_hidport;
	HwSettingsUSB_HIDPortGet(&hwsettings_usb_hidport);

	if (!usb_hid_present) {
		/* Force HID port function to disabled if we haven't advertised HID in our USB descriptor */
		hwsettings_usb_hidport = HWSETTINGS_USB_HIDPORT_DISABLED;
	}

	switch (hwsettings_usb_hidport) {
	case HWSETTINGS_USB_HIDPORT_DISABLED:
// always enable USB HID because CDC is not ported to F4XX yet
//		break;
	case HWSETTINGS_USB_HIDPORT_USBTELEMETRY:
#if defined(PIOS_INCLUDE_COM)
		{
			uint32_t pios_usb_hid_id;
			if (PIOS_USB_HID_Init(&pios_usb_hid_id, &pios_usb_hid_cfg, pios_usb_id)) {
				PIOS_Assert(0);
			}
			uint8_t * rx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_TELEM_USB_RX_BUF_LEN);
			uint8_t * tx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_TELEM_USB_TX_BUF_LEN);
			PIOS_Assert(rx_buffer);
			PIOS_Assert(tx_buffer);
			if (PIOS_COM_Init(&pios_com_telem_usb_id, &pios_usb_hid_com_driver, pios_usb_hid_id,
						rx_buffer, PIOS_COM_TELEM_USB_RX_BUF_LEN,
						tx_buffer, PIOS_COM_TELEM_USB_TX_BUF_LEN)) {
				PIOS_Assert(0);
			}
		}
#endif	/* PIOS_INCLUDE_COM */
		break;
	}

#endif	/* PIOS_INCLUDE_USB_HID */
	
	if (usb_hid_present || usb_cdc_present) {
		PIOS_USBHOOK_Activate();
	}
	
#endif	/* PIOS_INCLUDE_USB */



	/* Configure the main IO port */
	uint8_t hwsettings_DSMxBind;
	HwSettingsDSMxBindGet(&hwsettings_DSMxBind);
	uint8_t hwsettings_cc_mainport;
	HwSettingsCC_MainPortGet(&hwsettings_cc_mainport);

	switch (hwsettings_cc_mainport) {
	case HWSETTINGS_CC_MAINPORT_DISABLED:
		break;
	case HWSETTINGS_CC_MAINPORT_TELEMETRY:
		//not possible because main port (usart1 is rx only)
		break;
	case HWSETTINGS_CC_MAINPORT_SBUS:
		//not possible because no inverter
		break;
	case HWSETTINGS_CC_MAINPORT_GPS:
		PIOS_Board_configure_com(&pios_usart1_cfg, PIOS_COM_GPS_RX_BUF_LEN, -1, &pios_usart_com_driver, &pios_com_gps_id);
		break;
	case HWSETTINGS_CC_MAINPORT_DSM2:
	case HWSETTINGS_CC_MAINPORT_DSMX10BIT:
	case HWSETTINGS_CC_MAINPORT_DSMX11BIT:
#if defined(PIOS_INCLUDE_DSM)
		{
			enum pios_dsm_proto proto;
			switch (hwsettings_cc_mainport) {
			case HWSETTINGS_CC_MAINPORT_DSM2:
				proto = PIOS_DSM_PROTO_DSM2;
				break;
			case HWSETTINGS_CC_MAINPORT_DSMX10BIT:
				proto = PIOS_DSM_PROTO_DSMX10BIT;
				break;
			case HWSETTINGS_CC_MAINPORT_DSMX11BIT:
				proto = PIOS_DSM_PROTO_DSMX11BIT;
				break;
			default:
				PIOS_Assert(0);
				break;
			}
			
			PIOS_Board_configure_dsm(&pios_usart1_dsm_cfg, &pios_usart1_dsm_aux_cfg, &pios_usart_com_driver,
				&proto, MANUALCONTROLSETTINGS_CHANNELGROUPS_DSMMAINPORT, &hwsettings_DSMxBind);
		}
#endif	/* PIOS_INCLUDE_DSM */
		break;
	case HWSETTINGS_CC_MAINPORT_COMAUX:
		break;
	case HWSETTINGS_CC_MAINPORT_COMBRIDGE:
		//not possible because main port (usart1 is rx only)
		break;
	}

	/* Configure the flexi port */
	uint8_t hwsettings_cc_flexiport;
	HwSettingsCC_FlexiPortGet(&hwsettings_cc_flexiport);

	switch (hwsettings_cc_flexiport) {
	case HWSETTINGS_CC_FLEXIPORT_DISABLED:
		break;
	case HWSETTINGS_CC_FLEXIPORT_TELEMETRY:
#if defined(PIOS_INCLUDE_TELEMETRY_RF)
		PIOS_Board_configure_com(&pios_usart2_cfg, PIOS_COM_TELEM_RF_RX_BUF_LEN, PIOS_COM_TELEM_RF_TX_BUF_LEN, &pios_usart_com_driver, &pios_com_telem_rf_id);
/*
		{
			uint32_t pios_usart_generic_id;
			if (PIOS_USART_Init(&pios_usart_generic_id, &pios_usart_generic_flexi_cfg)) {
				PIOS_Assert(0);
			}
			uint8_t * rx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_TELEM_RF_RX_BUF_LEN);
			uint8_t * tx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_TELEM_RF_TX_BUF_LEN);
			PIOS_Assert(rx_buffer);
			PIOS_Assert(tx_buffer);
			if (PIOS_COM_Init(&pios_com_telem_rf_id, &pios_usart_com_driver, pios_usart_generic_id,
  					  rx_buffer, PIOS_COM_TELEM_RF_RX_BUF_LEN,
					  tx_buffer, PIOS_COM_TELEM_RF_TX_BUF_LEN)) {
				PIOS_Assert(0);
			}
		}
*/
#endif /* PIOS_INCLUDE_TELEMETRY_RF */
		break;
	case HWSETTINGS_CC_FLEXIPORT_COMBRIDGE:
		PIOS_Board_configure_com(&pios_usart2_cfg, PIOS_COM_BRIDGE_RX_BUF_LEN, PIOS_COM_BRIDGE_TX_BUF_LEN, &pios_usart_com_driver, &pios_com_bridge_id);
/*
		{
			uint32_t pios_usart_generic_id;
			if (PIOS_USART_Init(&pios_usart_generic_id, &pios_usart2_cfg)) {
				PIOS_Assert(0);
			}

			uint8_t * rx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_BRIDGE_RX_BUF_LEN);
			uint8_t * tx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_BRIDGE_TX_BUF_LEN);
			PIOS_Assert(rx_buffer);
			PIOS_Assert(tx_buffer);
			if (PIOS_COM_Init(&pios_com_bridge_id, &pios_usart_com_driver, pios_usart_generic_id,
						rx_buffer, PIOS_COM_BRIDGE_RX_BUF_LEN,
						tx_buffer, PIOS_COM_BRIDGE_TX_BUF_LEN)) {
				PIOS_Assert(0);
			}
		}
*/
		break;
	case HWSETTINGS_CC_FLEXIPORT_GPS:
#if defined(PIOS_INCLUDE_GPS)
		PIOS_Board_configure_com(&pios_usart2_cfg, PIOS_COM_GPS_RX_BUF_LEN, -1, &pios_usart_com_driver, &pios_com_gps_id);
/*
		{
			uint32_t pios_usart_generic_id;
			if (PIOS_USART_Init(&pios_usart_generic_id, &pios_usart2_cfg)) {
				PIOS_Assert(0);
			}
			uint8_t * rx_buffer = (uint8_t *) pvPortMalloc(PIOS_COM_GPS_RX_BUF_LEN);
			PIOS_Assert(rx_buffer);
			if (PIOS_COM_Init(&pios_com_gps_id, &pios_usart_com_driver, pios_usart_generic_id,
					  rx_buffer, PIOS_COM_GPS_RX_BUF_LEN,
					  NULL, 0)) {
				PIOS_Assert(0);
			}
		}
*/
#endif	/* PIOS_INCLUDE_GPS */
		break;
	case HWSETTINGS_CC_FLEXIPORT_DSM2:
	case HWSETTINGS_CC_FLEXIPORT_DSMX10BIT:
	case HWSETTINGS_CC_FLEXIPORT_DSMX11BIT:
#if defined(PIOS_INCLUDE_DSM)
	{
		enum pios_dsm_proto proto;
		switch (hwsettings_cc_flexiport) {
		case HWSETTINGS_CC_FLEXIPORT_DSM2:
			proto = PIOS_DSM_PROTO_DSM2;
			break;
		case HWSETTINGS_CC_FLEXIPORT_DSMX10BIT:
			proto = PIOS_DSM_PROTO_DSMX10BIT;
			break;
		case HWSETTINGS_CC_FLEXIPORT_DSMX11BIT:
			proto = PIOS_DSM_PROTO_DSMX11BIT;
			break;
		default:
			PIOS_Assert(0);
			break;
		}

		PIOS_Board_configure_dsm(&pios_usart2_dsm_cfg, &pios_usart2_dsm_aux_cfg, &pios_usart_com_driver,
			&proto, MANUALCONTROLSETTINGS_CHANNELGROUPS_DSMFLEXIPORT, &hwsettings_DSMxBind);
	}
#endif	/* PIOS_INCLUDE_DSM */
		break;
	case HWSETTINGS_CC_FLEXIPORT_COMAUX:
		break;
	case HWSETTINGS_CC_FLEXIPORT_I2C:
		// not supported for this hardware
		break;
	}




	/* Configure the rcvr port */
	uint8_t hwsettings_rcvrport;
	HwSettingsCC_RcvrPortGet(&hwsettings_rcvrport);

	switch (hwsettings_rcvrport) {
	case HWSETTINGS_CC_RCVRPORT_DISABLED:
		break;
	case HWSETTINGS_CC_RCVRPORT_PWM:
#if defined(PIOS_INCLUDE_PWM)
		{
			uint32_t pios_pwm_id;
			PIOS_PWM_Init(&pios_pwm_id, &pios_pwm_cfg);

			uint32_t pios_pwm_rcvr_id;
			if (PIOS_RCVR_Init(&pios_pwm_rcvr_id, &pios_pwm_rcvr_driver, pios_pwm_id)) {
				PIOS_Assert(0);
			}
			pios_rcvr_group_map[MANUALCONTROLSETTINGS_CHANNELGROUPS_PWM] = pios_pwm_rcvr_id;
		}
#endif	/* PIOS_INCLUDE_PWM */
		break;
	case HWSETTINGS_CC_RCVRPORT_PPM:
	case HWSETTINGS_CC_RCVRPORT_PPMOUTPUTS:
#if defined(PIOS_INCLUDE_PPM)
		{
			uint32_t pios_ppm_id;
			PIOS_PPM_Init(&pios_ppm_id, &pios_ppm_cfg);

			uint32_t pios_ppm_rcvr_id;
			if (PIOS_RCVR_Init(&pios_ppm_rcvr_id, &pios_ppm_rcvr_driver, pios_ppm_id)) {
				PIOS_Assert(0);
			}
			pios_rcvr_group_map[MANUALCONTROLSETTINGS_CHANNELGROUPS_PPM] = pios_ppm_rcvr_id;
		}
#endif	/* PIOS_INCLUDE_PPM */
		break;
	}




#if defined(PIOS_INCLUDE_SERVO)
	/* Set up the servo outputs */
	PIOS_Servo_Init(&pios_servo_cfg);
#endif


#if defined(PIOS_INCLUDE_MPU6050)
	/* Set up the I2C interface to the accelerometer*/
	if (PIOS_I2C_Init(&pios_i2c_gyro_accel_id, &pios_i2c_gyro_accel_cfg)) {
		PIOS_DEBUG_Assert(0);
	}
	
	PIOS_MPU6050_Init(pios_i2c_gyro_accel_id, PIOS_MPU6050_I2C_ADD_A0_LOW, &pios_mpu6050_cfg);
	{
		uint8_t init_test;
		init_test = PIOS_MPU6050_Test();
		++init_test;
	}
#endif /* PIOS_INCLUDE_MPU6050 */

#if defined(PIOS_INCLUDE_GPIO)
	PIOS_GPIO_Init();
#endif

	/* Make sure we have at least one telemetry link configured or else fail initialization */
	PIOS_Assert(pios_com_telem_rf_id || pios_com_telem_usb_id);
}

/**
 * @}
 */