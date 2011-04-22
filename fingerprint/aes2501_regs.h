
/*
 * aes2501_regs.h -- AuthenTec AES2501 Fingerprint Sensor Driver for Linux
 *
 * Maintainer: Cyrille Bagard <nocbos@gmail.com>
 *
 * Copyright (C) 2007 Cyrille Bagard
 *
 * This file is part of the AES2501 driver.
 *
 * the AES2501 driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * the AES2501 driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the AES2501 driver; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef __AES2501_REGS_H
#define __AES2501_REGS_H



/*
 * AES2501 Control Register 1 (CTRL1)
 */

#define AES2501_REG_CTRL1 0x80

#define AES2501_CTRL1_MASTER_RESET	0x01	/* Master reset, same as Power On Reset */
#define AES2501_CTRL1_SCAN_RESET	0x02	/* Scan reset: stop and restart the scan sequencer */
#define AES2501_CTRL1_REG_UPDATE	0x04	/* 1 = continuously updated, 0 = updated prior to starting a scan */


/*
 * AES2501 Control Register 2 (CTRL2)
 */

#define AES2501_REG_CTRL2 0x81

#define AES2501_CTRL2_CONTINUOUS	0x01	/* 1 = continuous scans, 0 = single scans */
#define AES2501_CTRL2_READ_REGS		0x02	/* Read the current state of the local registers in the Sensor IC */
#define AES2501_CTRL2_SET_ONE_SHOT	0x04	/* Set the one-shot flip-flop */
#define AES2501_CTRL2_CLR_ONE_SHOT	0x08	/* Clear the one-shot flip-flop */
#define AES2501_CTRL2_READ_ID		0x10	/* Read the ID register of the chip */


/*
 * AES2501 Excitation Common Control Register (EXCITCTRL)
 */

#define AES2501_REG_EXCITCTRL 0x82

#define AES2501_EXCITCTRL_LO_PWR	0x01	/* If set, enable detection in sleep/suspend mode */
#define AES2501_EXCITCTRL_AUTO_CAL	0x02	/* If set, perform finger detection calibration */
#define AES2501_EXCITCTRL_SGC_ENABLE	0x04	/* ??? */
#define AES2501_EXCITCTRL_SGC_RESTART	0x08	/* ??? */

#define AES2501_EXCITCTRL_EXCIT_SQR	0x20	/* Select the (1=square | 0=sine) wave finger drive signal */
#define AES2501_EXCITCTRL_EXCIT_BOOST	0x10	/* TODO: understand this part */


/*
 * AES2501 Detect Control Register (DETCTRL)
 */

#define AES2501_REG_DETCTRL 0x83

enum aes2501_detection_rate {

	AES2501_DETCTRL_DRATE_CONTINUOUS	= 0x00,	/* Detection cycles occur continuously */
	AES2501_DETCTRL_DRATE_16_MS		= 0x01,	/* Detection cycles occur every 16.62 ms */
	AES2501_DETCTRL_DRATE_31_MS		= 0x02,	/* Detection cycles occur every 31.24 ms */
	AES2501_DETCTRL_DRATE_62_MS		= 0x03,	/* Detection cycles occur every 62.50 ms */
	AES2501_DETCTRL_DRATE_125_MS		= 0x04,	/* Detection cycles occur every 125.0 ms */
	AES2501_DETCTRL_DRATE_250_MS		= 0x05,	/* Detection cycles occur every 250.0 ms */
	AES2501_DETCTRL_DRATE_500_MS		= 0x06,	/* Detection cycles occur every 500.0 ms */
	AES2501_DETCTRL_DRATE_1_S		= 0x07	/* Detection cycles occur every 1 s */

};

enum aes2501_settling_delay {

	AES2501_DETCTRL_SDELAY_31_MS	= 0x00,	/* 31.25 ms */
	AES2501_DETCTRL_SSDELAY_62_MS	= 0x10,	/* 62.5 ms */
	AES2501_DETCTRL_SSDELAY_125_MS	= 0x20,	/* 125 ms */
	AES2501_DETCTRL_SSDELAY_250_MS	= 0x30	/* 250 ms */

};





/*
 * AES2501 Column Scan Rate Register (COLSCAN)
 */

#define AES2501_REG_COLSCAN 0x88

enum aes2501_col_scan_rate {

    AES2501_COLSCAN_SRATE_32_US		= 0x00,	/* 32 us */
    AES2501_COLSCAN_SRATE_64_US		= 0x01,	/* 64 us */
    AES2501_COLSCAN_SRATE_128_US	= 0x02,	/* 128 us */
    AES2501_COLSCAN_SRATE_256_US	= 0x03,	/* 256 us */
    AES2501_COLSCAN_SRATE_512_US	= 0x04,	/* 512 us */
    AES2501_COLSCAN_SRATE_1024_US	= 0x05,	/* 1024 us */
    AES2501_COLSCAN_SRATE_2048_US	= 0x06,	/* 2048 us */

};


/*
 * AES2501 Measure Drive Register (MEASDRV)
 */

#define AES2501_REG_MEASDRV 0x89

enum aes2501_mesure_drive {

	AES2501_MEASDRV_MDRIVE_0_325	= 0x00,	/* 0.325 Vpp */
	AES2501_MEASDRV_MDRIVE_0_65	= 0x01,	/* 0.65 Vpp */
	AES2501_MEASDRV_MDRIVE_1_3	= 0x02,	/* 1.3 Vpp */
	AES2501_MEASDRV_MDRIVE_2_6	= 0x03	/* 2.6 Vpp */

};

#define AES2501_MEASDRV_SQUARE		0x20	/* Select (1=square | 0=sine) wave drive during measure */
#define AES2501_MEASDRV_MEASURE_SQUARE	0x10	/* 0 = use mesure drive setting, 1 = when sine wave is selected */


/*
 * AES2501 Measure Frequency Register (MEASFREQ)
 */

#define AES2501_REG_MEASFREQ 0x8a

enum aes2501_measure_freq {

	AES2501_MEASFREQ_125K	= 0x01,	/* 125 kHz */
	AES2501_MEASFREQ_250K	= 0x02,	/* 250 kHz */
	AES2501_MEASFREQ_500K	= 0x03,	/* 500 kHz */
	AES2501_MEASFREQ_1M	= 0x04,	/* 1 MHz */
	AES2501_MEASFREQ_2M	= 0x05	/* 2 MHz */

};




/*
 * AES2501 Demod Phase 2 Register (DEMODPHASE2)
 */

#define AES2501_REG_DEMODPHASE2 0x8c

#define DEMODPHASE_NONE		0x00

#define DEMODPHASE_180_00	0x40	/* 180 degrees */
#define DEMODPHASE_2_81		0x01	/* 2.8125 degrees */


/*
 * AES2501 Demod Phase 1 Register (DEMODPHASE1)
 */

#define AES2501_REG_DEMODPHASE1 0x8d

#define DEMODPHASE_1_40		0x40	/* 1.40625 degrees */
#define DEMODPHASE_0_02		0x01	/* 0.02197256 degrees */


/*
 * AES2501 Channel Gain Register (CHANGAIN)
 */

#define AES2501_REG_CHANGAIN 0x8e

enum aes2501_sensor_gain1 {

	AES2501_CHANGAIN_STAGE1_2X	= 0x00,	/* 2x */
	AES2501_CHANGAIN_STAGE1_4X	= 0x01,	/* 4x */
	AES2501_CHANGAIN_STAGE1_8X	= 0x02,	/* 8x */
	AES2501_CHANGAIN_STAGE1_16X	= 0x03	/* 16x */

};

enum aes2501_sensor_gain2 {

	AES2501_CHANGAIN_STAGE2_2X	= 0x00,	/* 2x */
	AES2501_CHANGAIN_STAGE2_4X	= 0x10,	/* 4x */
	AES2501_CHANGAIN_STAGE2_8X	= 0x20,	/* 8x */
	AES2501_CHANGAIN_STAGE2_16X	= 0x30	/* 16x */

};



/*
 * AES2501 A/D Reference High Register (ADREFHI)
 */

#define AES2501_REG_ADREFHI 0x91


/*
 * AES2501 A/D Reference Low Register (ADREFLO)
 */

#define AES2501_REG_ADREFLO 0x92


/*
 * AES2501 Start Row Register (STRTROW)
 */

#define AES2501_REG_STRTROW 0x93


/*
 * AES2501 End Row Register (ENDROW)
 */

#define AES2501_REG_ENDROW 0x94


/*
 * AES2501 Start Column Register (STRTCOL)
 */

#define AES2501_REG_STRTCOL 0x95


/*
 * AES2501 End Column Register (ENDCOL)
 */

#define AES2501_REG_ENDCOL 0x96


/*
 * AES2501 Data Format Register (DATFMT)
 */

#define AES2501_REG_DATFMT 0x97

#define AES2501_DATFMT_EIGHT	0x40	/* 1 = 8-bit data, 0 = 4-bit data */
#define AES2501_DATFMT_LOW_RES	0x20	/* TODO: understand this part */
#define AES2501_DATFMT_BIN_IMG	0x10	/* TODO: understand this part */

/* TODO: Threshold */


/*
 * AES2501 Image Data Control Register (IMAGCTRL)
 */

#define AES2501_REG_IMAGCTRL 0x98

#define AES2501_IMAGCTRL_IMG_DATA_DISABLE	0x01	/* If set, image data message and authentication message are not returned when imaging */
#define AES2501_IMAGCTRL_HISTO_DATA_ENABLE	0x02	/* if set, send histogram message when imaging */
#define AES2501_IMAGCTRL_HISTO_EACH_ROW		0x04	/* A histo message is sent at the end of (1=each row | 0 = scanning) */  
#define AES2501_IMAGCTRL_HISTO_FULL_ARRAY	0x08	/* 1 = full image array, 0 = 64x64 center */  
#define AES2501_IMAGCTRL_REG_FIRST		0x10	/* Registers are returned (1=before | 0=after) image data */
#define AES2501_IMAGCTRL_TST_REG_ENABLE		0x20	/* If set, Test Registers are returned with register messages */








/*
 * AES2501 Status Register (STAT)
 */

#define AES2501_REG_STAT 0x9a

#define AES2501_STAT_SCAN	0x0f	/* Scan state */
#define AES2501_STAT_ERROR	0x10	/* Framing error */
#define AES2501_STAT_PAUSED	0x20	/* Scan paused */
#define AES2501_STAT_RESET	0x40	/* Reset occurred */

enum aes2501_scan_state {

	STATE_WAITING_FOR_FINGER	= 0x00,	/* Waiting for finger */
	STATE_FINGER_SETTLING_DELAY	= 0x01,	/* In Finger settling delay */
	STATE_POWER_UP_DELAY		= 0x02,	/* In power up delay */
	STATE_WAITING_TO_START_SCAN	= 0x03,	/* Waiting to start image scan */
	STATE_PRELOADING_SUBARRAY_0	= 0x04,	/* Pre-loading subarray 0 */
	STATE_SETUP_FOR_ROW_ADVANCE	= 0x05,	/* Setup for row advance */
	STATE_WAITING_FOR_ROW_ADVANCE	= 0x06,	/* Waiting for row advance */
	STATE_PRELOADING_COL_0		= 0x07,	/* Pre-loading column 0 */
	STATE_SETUP_FOR_COL_ADVANCE	= 0x08,	/* Setup for column advance */
	STATE_WAITING_FOR_COL_ADVANCE	= 0x09,	/* Waiting for column advance */
	STATE_WAITING_FOR_SCAN_START	= 0x0a,	/* Waiting for scan start */
	STATE_WAITING_FOR_SCAN_END	= 0x0b,	/* Waiting for scan end */
	STATE_WAITING_FOR_ROW_SETUP	= 0x0c,	/* Waiting for row setup */
	STATE_WAITING_FOR_COL_TIME	= 0x0d,	/* Waiting for one column time (depends on scan rate) */
	STATE_WAITING_FOR_QUEUED_DATA	= 0x0e,	/* Waiting for queued data transmission to be completed */
	STATE_WAIT_FOR_128_US		= 0x0f	/* Wait for 128 us */

};


/*
 * AES2501 Challenge Word 1 Register (CHWORD1)
 */

#define AES2501_REG_CHWORD1 0x9b

#define AES2501_CHWORD1_IS_FINGER	0x01	/* If set, finger is present */


/*
 * AES2501 Challenge Word 2 Register (CHWORD2)
 */

#define AES2501_REG_CHWORD2 0x9c


/*
 * AES2501 Challenge Word 3 Register (CHWORD3)
 */

#define AES2501_REG_CHWORD3 0x9d


/*
 * AES2501 Challenge Word 4 Register (CHWORD4)
 */

#define AES2501_REG_CHWORD4 0x9e


/*
 * AES2501 Challenge Word 5 Register (CHWORD5)
 */

#define AES2501_REG_CHWORD5 0x9f





/*
 * AES2501 Test Register 1 (TREG1)
 */

#define AES2501_REG_TREG1 0xa1

#define AES2501_TREG1_SBIAS_UNLCK	0x10	/* 1 = unlock the controlling of sense amp bias, 0 = sense amp bias changes */

enum aes2501_sense_amp_bias {

	AES2501_TREG1_SAMP_BIAS_2_UA	= 0x00,	/* 2.5 uA */
	AES2501_TREG1_SAMP_BIAS_5_UA	= 0x01,	/* 5 uA */
	AES2501_TREG1_SAMP_BIAS_8_UA	= 0x02,	/* 8 uA */
	AES2501_TREG1_SAMP_BIAS_10_UA	= 0x03	/* 10 uA */

};




/*
 * AES2501 Auto-Calibration Offset Register (AUTOCALOFFSET)
 */

#define AES2501_REG_AUTOCALOFFSET 0xa8





/*
 * AES2501 Test Register C (TREGC)
 */

#define AES2501_REG_TREGC 0xac

#define AES2501_TREGC_ENABLE	0x01	/* Enable the reading of the register in TREGD */


/*
 * AES2501 Test Register D (TREGD)
 */

#define AES2501_REG_TREGD 0xad





/*
 * AES2501 Low Power Oscillator On Time Register (LPONT)
 */

#define AES2501_REG_LPONT 0xb4

/*
 * This register sets the low power oscillator on time.
 * Units are roughly equivalent to milliseconds.
 */

#define AES2501_LPONT_MIN_VALUE 0x00	/* 0 ms */
#define AES2501_LPONT_MAX_VALUE 0x1f	/* About 16 ms */



#define ENUM_REG(reg) _ ## reg = reg

typedef enum _Aes2501Registers
{
	ENUM_REG(AES2501_REG_CTRL1),
	ENUM_REG(AES2501_REG_CTRL2),
	ENUM_REG(AES2501_REG_EXCITCTRL),
	ENUM_REG(AES2501_REG_DETCTRL),
	ENUM_REG(AES2501_REG_COLSCAN),
	ENUM_REG(AES2501_REG_MEASDRV),
	ENUM_REG(AES2501_REG_MEASFREQ),
	ENUM_REG(AES2501_REG_DEMODPHASE2),
	ENUM_REG(AES2501_REG_DEMODPHASE1),
	ENUM_REG(AES2501_REG_CHANGAIN),
	ENUM_REG(AES2501_REG_ADREFHI),
	ENUM_REG(AES2501_REG_ADREFLO),
	ENUM_REG(AES2501_REG_STRTROW),
	ENUM_REG(AES2501_REG_ENDROW),
	ENUM_REG(AES2501_REG_STRTCOL),
	ENUM_REG(AES2501_REG_ENDCOL),
	ENUM_REG(AES2501_REG_DATFMT),
	ENUM_REG(AES2501_REG_IMAGCTRL),
	ENUM_REG(AES2501_REG_STAT),
	ENUM_REG(AES2501_REG_CHWORD1),
	ENUM_REG(AES2501_REG_CHWORD2),
	ENUM_REG(AES2501_REG_CHWORD3),
	ENUM_REG(AES2501_REG_CHWORD4),
	ENUM_REG(AES2501_REG_CHWORD5),
	ENUM_REG(AES2501_REG_TREG1),
	ENUM_REG(AES2501_REG_AUTOCALOFFSET),
	ENUM_REG(AES2501_REG_TREGC),
	ENUM_REG(AES2501_REG_TREGD),
	ENUM_REG(AES2501_REG_LPONT)

} Aes2501Registers;


#define FIRST_AES2501_REG	0x80
#define LAST_AES2501_REG	0x9f



#endif	/* __AES2501_REGS_H */
