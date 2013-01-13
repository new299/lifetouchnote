/*
 * Copyright (C) 2010 QCI
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *
 */
#if !defined(QCI_FINPOINT_H)
#define QCI_FINPOINT_H

#define	OFN_PRODUCT_ID		0x00	//	Product_ID	R	0x83
#define	OFN_REVISION_ID		0x01	//	Revision_ID	R	0x01
#define OFN_MOTION_REG		0x02	//	Motion	R/W	0x00
#define OFN_DELTA_X_REG		0x03	//	Delta_X	R	Any
#define OFN_DELTA_Y_REG		0x04	//	Delta_Y	R	Any
#define OFN_SQUAL_REG		0x05	//	SQUAL	R	Any
#define OFN_SHUTTER_UPPER	0x06	//	Shutter_Upper	R	Any
#define OFN_SHUTTER_LOWER	0x07	//	Shutter_Lower	R	Any
#define OFN_MAX_PIXEL		0x08	//	Maximum_Pixel	R	0xD0
#define OFN_PIXEL_SUM		0x09	//	Pixel_Sum	R	0x80
#define OFN_MIN_PIXEL		0x0a	//	Minimum_Pixel	R	0x00
#define OFN_PIXEL_Grab		0x0b	//	Pixel_Grab	R/W	0x00
#define OFN_CRC0_REG		0x0c	//	CRC0	R	0x00
#define OFN_CRC1_REG		0x0d	//	CRC1	R	0x00
#define OFN_CRC2_REG		0x0e	//	CRC2	R	0x00
#define OFN_CRC3_REG		0x0f	//	CRC3	R	0x00
#define OFN_SELF_TEST_REG	0x10	//	Self_Test	W
#define OFN_CONFIGURATION_BITS	0x11	//	Configuration_Bits	R/W	0x03
#define OFN_LED_CONTROL		0x1a	//	LED_Control	R/W	0x00
#define OFN_IO_MODE_REG		0x1c	//	IO_Mode	R	0x00
#define OFN_MOTION_CONTROL	0x1d	//	Motin_Control	W	0x00
#define OFN_OBSERVATION		0x2e	//	Observation	R/W	Any
#define OFN_SOFT_RESET_REG	0x3a	//	Soft_RESET	W	0x00
#define OFN_SHUTTER_MAX_Hi	0x3b	//	Shutter_Max_Hi	R/W	0x0b
#define OFN_SHUTTER_MAX_Lo	0x3c	//	Shutter_Max_Lo	R/W	0x71
#define OFN_INVERSE_REVISION_ID	0x3e	//	Inverse_Revision_ID	R	0xFE
#define OFN_INVERSE_PRODUCT_ID	0x3f	//	Inverse_Product_ID	R	0x7C
#define	OFN_ENGINE_REG		0x60	//	OFN_Engine	R/W	0x00
#define	OFN_RESOLUTION		0x62	//	OFN_Resolution	R/W	0x1a
#define	OFN_SPEED_CONTROL	0x63	//	OFN_Speed_Control	R/W	0x04
#define	OFN_SPEED_ST12		0x64	//	OFN_Speed_ST12	R/W	0x08
#define	OFN_SPEED_ST21		0x65	//	OFN_Speed_ST21	R/W	0x06
#define	OFN_SPEED_ST23		0x66	//	OFN_Speed_ST23	R/W	0x40
#define	OFN_SPEED_ST32		0x67	//	OFN_Speed_ST32	R/W	0x08
#define	OFN_SPEED_ST34		0x68	//	OFN_Speed_ST34	R/W	0x48
#define	OFN_SPEED_ST43		0x69	//	OFN_Speed_ST43	R/W	0x0a
#define	OFN_SPEED_ST45		0x6a	//	OFN_Speed_ST45	R/W	0x50
#define	OFN_SPEED_ST54		0x6b	//	OFN_Speed_ST54	R/W	0x48
#define	OFN_AD_CTRL		0x6d	//	OFN_AD_CTRL	R/W	0xc4
#define	OFN_AD_ATH_HIGH		0x6e	//	OFN_AD_ATH_HIGH	R/W	0x34
#define	OFN_AD_DTH_HIGH		0x6f	//	OFN_AD_DTH_HIGH	R/W	0x3c
#define	OFN_AD_ATH_LOW		0x70	//	OFN_AD_ATH_LOW	R/W	0x18
#define	OFN_AD_DTH_LOW		0x71	//	OFN_AD_DTH_LOW	R/W	0x20
#define	OFN_QUANTIZE_CTRL	0x73	//	OFN_Quantize_CTRL	R/W	0x99
#define	OFN_XYQ_THRESH		0x74	//	OFN_XYQ_THRESH	R/W	0x02
#define	OFN_FPD_CTRL		0x75	//	OFN_FPD_CTRL	R/W	0x50
#define	OFN_ORIENTATION_CTRL	0x77	//	OFN_Orientation_CTRL	R/W	0x01

// Sam modify +++
#if 0
//QCI Ryan +++
//#define GPIO_IR_NRST 36
#define GPIO_IR_DOME_DRIVE 34   // Not yet to do.
#define GPIO_IR_DOME_SENSE 36   // Not yet to do.
//#define GPIO_IR_SHTDWN 38
//#define GPIO_IR_MOTION 37

#define GPIO_IR_NRST 131
#define GPIO_IR_SHTDWN 94
#define GPIO_IR_MOTION 124
//QCI Ryan --- 
#endif
// Sam modify ---

#define SENSOR_DATA_LEN 		9
#define SENSOR_DATA_FILTER		1
#define SENSOR_BUTTON_EVENT		1

typedef enum
{
	XPos = 0,
	XNeg,
	YPos,
	YNeg,
	None
}MoveType;

typedef enum
{
	MOTION = 0,
	X,
	Y,
	SQUAL,
	SHT_UP,
	SHT_DW,
	PIX_MAX,
	PIX_SUM,
	PIX_MIN
} IRSensorReg;

#endif

