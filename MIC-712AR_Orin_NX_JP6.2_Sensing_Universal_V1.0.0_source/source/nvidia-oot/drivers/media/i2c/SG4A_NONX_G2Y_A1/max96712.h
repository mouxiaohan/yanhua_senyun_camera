/*
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 * <b>MAX96712 API: For Maxim Integrated MAX96712 derializer</b>
 *
 * @b Description: Defines elements used to set up and use a
 *  Maxim Integrated MAX96712 derializer.
 */

#ifndef __MAX96712_H__
#define __MAX96712_H__

#include <media/gmsl-link.h>
#include <media/camera_common.h>

/**
 * \defgroup max9295 MAX96712 serializer driver
 *
 * Controls the MAX96712 serializer module.
 *
 * @ingroup serdes_group
 * @{
 */

int max96712_read_reg_Dser(int device_id, int slaveAddr,
		u16 addr, unsigned int *val);

int max96712_write_reg_Dser(int device_id, int slaveAddr,
			u16 addr, u8 val);

int max96712_write_reg_reg16_val16(int device_id, int slaveAddr,
			struct reg_16 table[], u16 wait_ms_addr, u16 end_addr);


#endif  /* __MAX96712_H__ */