/*
 * arch/arm/mach-tegra/edp.c
 *
 * Copyright (C) 2011 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <mach/edp.h>

#include "fuse.h"

static const struct tegra_edp_limits *edp_limits;
static int edp_limits_size;
static unsigned int regulator_cur;

static const unsigned int *system_edp_limits;

/*
 * Temperature step size cannot be less than 4C because of hysteresis
 * delta
 * Code assumes different temperatures for the same speedo_id /
 * regulator_cur are adjacent in the table, and higest regulator_cur
 * comes first
 */
static char __initdata tegra_edp_map[] = {
	0x00, 0x2f, 0x17, 0x7d, 0x73, 0x73, 0x73, 0x00,
	0x2f, 0x2d, 0x82, 0x78, 0x78, 0x78, 0x00, 0x2f,
	0x3c, 0x82, 0x78, 0x78, 0x78, 0x00, 0x2f, 0x4b,
	0x82, 0x78, 0x78, 0x78, 0x00, 0x2f, 0x55, 0x82,
	0x78, 0x78, 0x78, 0x00, 0x28, 0x17, 0x7d, 0x73,
	0x73, 0x73, 0x00, 0x28, 0x2d, 0x82, 0x78, 0x78,
	0x78, 0x00, 0x28, 0x3c, 0x82, 0x78, 0x78, 0x78,
	0x00, 0x28, 0x4b, 0x82, 0x78, 0x78, 0x73, 0x00,
	0x28, 0x55, 0x82, 0x78, 0x78, 0x69, 0x00, 0x23,
	0x17, 0x7d, 0x73, 0x73, 0x73, 0x00, 0x23, 0x2d,
	0x82, 0x78, 0x78, 0x78, 0x00, 0x23, 0x3c, 0x82,
	0x78, 0x78, 0x6e, 0x00, 0x23, 0x4b, 0x82, 0x78,
	0x78, 0x64, 0x00, 0x23, 0x55, 0x82, 0x78, 0x6e,
	0x5a, 0x00, 0x1e, 0x17, 0x7d, 0x73, 0x73, 0x64,
	0x00, 0x1e, 0x2d, 0x82, 0x78, 0x78, 0x69, 0x00,
	0x1e, 0x3c, 0x82, 0x78, 0x78, 0x64, 0x00, 0x1e,
	0x4b, 0x82, 0x78, 0x6e, 0x5a, 0x00, 0x1e, 0x55,
	0x82, 0x78, 0x64, 0x50, 0x00, 0x19, 0x17, 0x7d,
	0x73, 0x69, 0x55, 0x00, 0x19, 0x2d, 0x82, 0x78,
	0x6e, 0x5a, 0x00, 0x19, 0x3c, 0x82, 0x78, 0x69,
	0x55, 0x00, 0x19, 0x4b, 0x82, 0x78, 0x5f, 0x4b,
	0x00, 0x19, 0x55, 0x82, 0x73, 0x55, 0x3c, 0x01,
	0x2f, 0x17, 0x7d, 0x73, 0x73, 0x73, 0x01, 0x2f,
	0x2d, 0x82, 0x78, 0x78, 0x78, 0x01, 0x2f, 0x3c,
	0x82, 0x78, 0x78, 0x78, 0x01, 0x2f, 0x4b, 0x82,
	0x78, 0x78, 0x78, 0x01, 0x2f, 0x55, 0x82, 0x78,
	0x78, 0x78, 0x01, 0x28, 0x17, 0x7d, 0x73, 0x73,
	0x73, 0x01, 0x28, 0x2d, 0x82, 0x78, 0x78, 0x78,
	0x01, 0x28, 0x3c, 0x82, 0x78, 0x78, 0x78, 0x01,
	0x28, 0x4b, 0x82, 0x78, 0x78, 0x73, 0x01, 0x28,
	0x55, 0x82, 0x78, 0x78, 0x69, 0x01, 0x23, 0x17,
	0x7d, 0x73, 0x73, 0x73, 0x01, 0x23, 0x2d, 0x82,
	0x78, 0x78, 0x78, 0x01, 0x23, 0x3c, 0x82, 0x78,
	0x78, 0x6e, 0x01, 0x23, 0x4b, 0x82, 0x78, 0x78,
	0x64, 0x01, 0x23, 0x55, 0x82, 0x78, 0x6e, 0x5a,
	0x01, 0x1e, 0x17, 0x7d, 0x73, 0x73, 0x64, 0x01,
	0x1e, 0x2d, 0x82, 0x78, 0x78, 0x69, 0x01, 0x1e,
	0x3c, 0x82, 0x78, 0x78, 0x64, 0x01, 0x1e, 0x4b,
	0x82, 0x78, 0x6e, 0x5a, 0x01, 0x1e, 0x55, 0x82,
	0x78, 0x64, 0x50, 0x01, 0x19, 0x17, 0x7d, 0x73,
	0x69, 0x55, 0x01, 0x19, 0x2d, 0x82, 0x78, 0x6e,
	0x5a, 0x01, 0x19, 0x3c, 0x82, 0x78, 0x69, 0x55,
	0x01, 0x19, 0x4b, 0x82, 0x78, 0x5f, 0x4b, 0x01,
	0x19, 0x55, 0x82, 0x73, 0x55, 0x3c, 0x02, 0x3d,
	0x17, 0x87, 0x7d, 0x7d, 0x7d, 0x02, 0x3d, 0x2d,
	0x8c, 0x82, 0x82, 0x82, 0x02, 0x3d, 0x3c, 0x8c,
	0x82, 0x82, 0x82, 0x02, 0x3d, 0x4b, 0x8c, 0x82,
	0x82, 0x82, 0x02, 0x3d, 0x55, 0x8c, 0x82, 0x82,
	0x82, 0x02, 0x32, 0x17, 0x87, 0x7d, 0x7d, 0x7d,
	0x02, 0x32, 0x2d, 0x8c, 0x82, 0x82, 0x82, 0x02,
	0x32, 0x3c, 0x8c, 0x82, 0x82, 0x82, 0x02, 0x32,
	0x4b, 0x8c, 0x82, 0x82, 0x78, 0x02, 0x32, 0x55,
	0x8c, 0x82, 0x82, 0x6e, 0x02, 0x28, 0x17, 0x87,
	0x7d, 0x7d, 0x73, 0x02, 0x28, 0x2d, 0x8c, 0x82,
	0x82, 0x78, 0x02, 0x28, 0x3c, 0x8c, 0x82, 0x82,
	0x73, 0x02, 0x28, 0x4b, 0x8c, 0x82, 0x78, 0x69,
	0x02, 0x28, 0x55, 0x8c, 0x82, 0x6e, 0x5a, 0x02,
	0x23, 0x17, 0x87, 0x7d, 0x7d, 0x69, 0x02, 0x23,
	0x2d, 0x8c, 0x82, 0x82, 0x6e, 0x02, 0x23, 0x3c,
	0x8c, 0x82, 0x78, 0x69, 0x02, 0x23, 0x4b, 0x8c,
	0x82, 0x6e, 0x5a, 0x02, 0x23, 0x55, 0x8c, 0x82,
	0x64, 0x50, 0x03, 0x3d, 0x17, 0x87, 0x7d, 0x7d,
	0x7d, 0x03, 0x3d, 0x2d, 0x8c, 0x82, 0x82, 0x82,
	0x03, 0x3d, 0x3c, 0x8c, 0x82, 0x82, 0x82, 0x03,
	0x3d, 0x4b, 0x8c, 0x82, 0x82, 0x82, 0x03, 0x3d,
	0x55, 0x8c, 0x82, 0x82, 0x82, 0x03, 0x32, 0x17,
	0x87, 0x7d, 0x7d, 0x7d, 0x03, 0x32, 0x2d, 0x8c,
	0x82, 0x82, 0x82, 0x03, 0x32, 0x3c, 0x8c, 0x82,
	0x82, 0x82, 0x03, 0x32, 0x4b, 0x8c, 0x82, 0x82,
	0x78, 0x03, 0x32, 0x55, 0x8c, 0x82, 0x82, 0x6e,
	0x03, 0x28, 0x17, 0x87, 0x7d, 0x7d, 0x73, 0x03,
	0x28, 0x2d, 0x8c, 0x82, 0x82, 0x78, 0x03, 0x28,
	0x3c, 0x8c, 0x82, 0x82, 0x73, 0x03, 0x28, 0x4b,
	0x8c, 0x82, 0x78, 0x69, 0x03, 0x28, 0x55, 0x8c,
	0x82, 0x6e, 0x5a, 0x03, 0x23, 0x17, 0x87, 0x7d,
	0x7d, 0x69, 0x03, 0x23, 0x2d, 0x8c, 0x82, 0x82,
	0x6e, 0x03, 0x23, 0x3c, 0x8c, 0x82, 0x78, 0x69,
	0x03, 0x23, 0x4b, 0x8c, 0x82, 0x6e, 0x5a, 0x03,
	0x23, 0x55, 0x8c, 0x82, 0x64, 0x50, 0x04, 0x32,
	0x17, 0x91, 0x87, 0x87, 0x87, 0x04, 0x32, 0x2d,
	0x96, 0x8c, 0x8c, 0x8c, 0x04, 0x32, 0x3c, 0x96,
	0x8c, 0x8c, 0x8c, 0x04, 0x32, 0x46, 0x96, 0x8c,
	0x8c, 0x8c, 0x04, 0x32, 0x4b, 0x82, 0x78, 0x78,
	0x78, 0x04, 0x32, 0x55, 0x82, 0x78, 0x78, 0x78,
	0x04, 0x2f, 0x17, 0x91, 0x87, 0x87, 0x87, 0x04,
	0x2f, 0x2d, 0x96, 0x8c, 0x8c, 0x8c, 0x04, 0x2f,
	0x3c, 0x96, 0x8c, 0x8c, 0x8c, 0x04, 0x2f, 0x46,
	0x96, 0x8c, 0x8c, 0x82, 0x04, 0x2f, 0x4b, 0x82,
	0x78, 0x78, 0x78, 0x04, 0x2f, 0x55, 0x82, 0x78,
	0x78, 0x78, 0x04, 0x28, 0x17, 0x91, 0x87, 0x87,
	0x87, 0x04, 0x28, 0x2d, 0x96, 0x8c, 0x8c, 0x82,
	0x04, 0x28, 0x3c, 0x96, 0x8c, 0x8c, 0x82, 0x04,
	0x28, 0x46, 0x96, 0x8c, 0x8c, 0x78, 0x04, 0x28,
	0x4b, 0x82, 0x78, 0x78, 0x78, 0x04, 0x28, 0x55,
	0x82, 0x78, 0x78, 0x6e, 0x04, 0x23, 0x17, 0x91,
	0x87, 0x87, 0x73, 0x04, 0x23, 0x2d, 0x96, 0x8c,
	0x8c, 0x78, 0x04, 0x23, 0x3c, 0x96, 0x8c, 0x82,
	0x78, 0x04, 0x23, 0x46, 0x96, 0x8c, 0x82, 0x6e,
	0x04, 0x23, 0x4b, 0x82, 0x78, 0x78, 0x6e, 0x04,
	0x23, 0x55, 0x82, 0x78, 0x78, 0x64, 0x04, 0x1e,
	0x17, 0x91, 0x87, 0x7d, 0x69, 0x04, 0x1e, 0x2d,
	0x96, 0x8c, 0x82, 0x6e, 0x04, 0x1e, 0x3c, 0x96,
	0x8c, 0x78, 0x64, 0x04, 0x1e, 0x46, 0x96, 0x8c,
	0x78, 0x5a, 0x04, 0x1e, 0x4b, 0x82, 0x78, 0x78,
	0x5a, 0x04, 0x1e, 0x55, 0x82, 0x78, 0x64, 0x50,
	0x04, 0x19, 0x17, 0x91, 0x87, 0x69, 0x55, 0x04,
	0x19, 0x2d, 0x96, 0x8c, 0x6e, 0x5a, 0x04, 0x19,
	0x3c, 0x96, 0x82, 0x6e, 0x55, 0x04, 0x19, 0x46,
	0x96, 0x82, 0x64, 0x50, 0x04, 0x19, 0x4b, 0x82,
	0x78, 0x64, 0x50, 0x04, 0x19, 0x55, 0x82, 0x78,
	0x55, 0x3c, 0x05, 0x64, 0x17, 0xa5, 0x9b, 0x9b,
	0x9b, 0x05, 0x64, 0x2d, 0xaa, 0xa0, 0xa0, 0xa0,
	0x05, 0x64, 0x3c, 0xaa, 0xa0, 0xa0, 0xa0, 0x05,
	0x64, 0x46, 0xaa, 0xa0, 0xa0, 0xa0, 0x05, 0x64,
	0x4b, 0x8c, 0x82, 0x82, 0x82, 0x05, 0x64, 0x55,
	0x8c, 0x82, 0x82, 0x82, 0x05, 0x50, 0x17, 0xa5,
	0x9b, 0x9b, 0x9b, 0x05, 0x50, 0x2d, 0xaa, 0xa0,
	0xa0, 0xa0, 0x05, 0x50, 0x3c, 0xaa, 0xa0, 0xa0,
	0x96, 0x05, 0x50, 0x46, 0xaa, 0xa0, 0xa0, 0x96,
	0x05, 0x50, 0x4b, 0x8c, 0x82, 0x82, 0x82, 0x05,
	0x50, 0x55, 0x8c, 0x82, 0x82, 0x82, 0x05, 0x3c,
	0x17, 0xa5, 0x9b, 0x9b, 0x87, 0x05, 0x3c, 0x2d,
	0xaa, 0xa0, 0xa0, 0x8c, 0x05, 0x3c, 0x3c, 0xaa,
	0xa0, 0x96, 0x82, 0x05, 0x3c, 0x46, 0xaa, 0xa0,
	0x96, 0x78, 0x05, 0x3c, 0x4b, 0x8c, 0x82, 0x82,
	0x78, 0x05, 0x3c, 0x55, 0x8c, 0x82, 0x82, 0x6e,
	0x05, 0x28, 0x17, 0xa5, 0x91, 0x7d, 0x69, 0x05,
	0x28, 0x2d, 0xaa, 0x96, 0x82, 0x6e, 0x05, 0x28,
	0x3c, 0xaa, 0x96, 0x78, 0x64, 0x05, 0x28, 0x46,
	0xaa, 0x8c, 0x6e, 0x5a, 0x05, 0x28, 0x4b, 0x8c,
	0x82, 0x6e, 0x5a, 0x05, 0x28, 0x55, 0x8c, 0x82,
	0x64, 0x50, 0x06, 0x3d, 0x17, 0xa5, 0x9b, 0x7d,
	0x7d, 0x06, 0x3d, 0x2d, 0xaa, 0xa0, 0x82, 0x82,
	0x06, 0x3d, 0x3c, 0xaa, 0xa0, 0x82, 0x82, 0x06,
	0x3d, 0x46, 0xaa, 0xa0, 0x82, 0x82, 0x06, 0x3d,
	0x4b, 0x8c, 0x82, 0x82, 0x82, 0x06, 0x3d, 0x55,
	0x8c, 0x82, 0x82, 0x82, 0x06, 0x32, 0x17, 0xa5,
	0x9b, 0x7d, 0x7d, 0x06, 0x32, 0x2d, 0xaa, 0xa0,
	0x82, 0x82, 0x06, 0x32, 0x3c, 0xaa, 0xa0, 0x82,
	0x82, 0x06, 0x32, 0x46, 0xaa, 0xa0, 0x82, 0x78,
	0x06, 0x32, 0x4b, 0x8c, 0x82, 0x82, 0x78, 0x06,
	0x32, 0x55, 0x8c, 0x82, 0x82, 0x6e, 0x06, 0x28,
	0x17, 0xa5, 0x9b, 0x7d, 0x73, 0x06, 0x28, 0x2d,
	0xaa, 0xa0, 0x82, 0x78, 0x06, 0x28, 0x3c, 0xaa,
	0x96, 0x82, 0x73, 0x06, 0x28, 0x46, 0xaa, 0x96,
	0x78, 0x69, 0x06, 0x28, 0x4b, 0x8c, 0x82, 0x78,
	0x69, 0x06, 0x28, 0x55, 0x8c, 0x82, 0x6e, 0x5a,
	0x06, 0x23, 0x17, 0xa5, 0x91, 0x7d, 0x69, 0x06,
	0x23, 0x2d, 0xaa, 0x96, 0x82, 0x6e, 0x06, 0x23,
	0x3c, 0xaa, 0x96, 0x78, 0x69, 0x06, 0x23, 0x46,
	0xaa, 0x8c, 0x6e, 0x5a, 0x06, 0x23, 0x4b, 0x8c,
	0x82, 0x6e, 0x5a, 0x06, 0x23, 0x55, 0x8c, 0x82,
	0x64, 0x50, 0x07, 0x3b, 0x17, 0x7d, 0x73, 0x73,
	0x73, 0x07, 0x3b, 0x2d, 0x82, 0x78, 0x78, 0x78,
	0x07, 0x3b, 0x3c, 0x82, 0x78, 0x78, 0x78, 0x07,
	0x3b, 0x4b, 0x82, 0x78, 0x78, 0x78, 0x07, 0x3b,
	0x5a, 0x82, 0x78, 0x78, 0x78, 0x07, 0x32, 0x17,
	0x7d, 0x73, 0x73, 0x73, 0x07, 0x32, 0x2d, 0x82,
	0x78, 0x78, 0x78, 0x07, 0x32, 0x3c, 0x82, 0x78,
	0x78, 0x78, 0x07, 0x32, 0x4b, 0x82, 0x78, 0x78,
	0x78, 0x07, 0x32, 0x5a, 0x82, 0x78, 0x6e, 0x64,
	0x07, 0x28, 0x17, 0x7d, 0x73, 0x73, 0x69, 0x07,
	0x28, 0x2d, 0x82, 0x78, 0x78, 0x6e, 0x07, 0x28,
	0x3c, 0x82, 0x78, 0x78, 0x64, 0x07, 0x28, 0x4b,
	0x82, 0x78, 0x78, 0x64, 0x07, 0x28, 0x5a, 0x82,
	0x78, 0x64, 0x50, 0x07, 0x23, 0x17, 0x7d, 0x73,
	0x73, 0x5f, 0x07, 0x23, 0x2d, 0x82, 0x78, 0x78,
	0x64, 0x07, 0x23, 0x3c, 0x82, 0x78, 0x78, 0x64,
	0x07, 0x23, 0x4b, 0x82, 0x78, 0x64, 0x50, 0x07,
	0x23, 0x5a, 0x82, 0x78, 0x5a, 0x46, 0x08, 0x3b,
	0x17, 0x7d, 0x73, 0x73, 0x73, 0x08, 0x3b, 0x2d,
	0x82, 0x78, 0x78, 0x78, 0x08, 0x3b, 0x3c, 0x82,
	0x78, 0x78, 0x78, 0x08, 0x3b, 0x4b, 0x82, 0x78,
	0x78, 0x78, 0x08, 0x3b, 0x5a, 0x82, 0x78, 0x78,
	0x78, 0x08, 0x32, 0x17, 0x7d, 0x73, 0x73, 0x73,
	0x08, 0x32, 0x2d, 0x82, 0x78, 0x78, 0x78, 0x08,
	0x32, 0x3c, 0x82, 0x78, 0x78, 0x78, 0x08, 0x32,
	0x4b, 0x82, 0x78, 0x78, 0x78, 0x08, 0x32, 0x5a,
	0x82, 0x78, 0x6e, 0x64, 0x08, 0x28, 0x17, 0x7d,
	0x73, 0x73, 0x69, 0x08, 0x28, 0x2d, 0x82, 0x78,
	0x78, 0x6e, 0x08, 0x28, 0x3c, 0x82, 0x78, 0x78,
	0x64, 0x08, 0x28, 0x4b, 0x82, 0x78, 0x78, 0x64,
	0x08, 0x28, 0x5a, 0x82, 0x78, 0x64, 0x50, 0x08,
	0x23, 0x17, 0x7d, 0x73, 0x73, 0x5f, 0x08, 0x23,
	0x2d, 0x82, 0x78, 0x78, 0x64, 0x08, 0x23, 0x3c,
	0x82, 0x78, 0x78, 0x64, 0x08, 0x23, 0x4b, 0x82,
	0x78, 0x64, 0x50, 0x08, 0x23, 0x5a, 0x82, 0x78,
	0x5a, 0x46, 0x0c, 0x52, 0x17, 0xa5, 0x9b, 0x9b,
	0x9b, 0x0c, 0x52, 0x2d, 0xaa, 0xa0, 0xa0, 0xa0,
	0x0c, 0x52, 0x3c, 0xaa, 0xa0, 0xa0, 0xa0, 0x0c,
	0x52, 0x46, 0xaa, 0xa0, 0xa0, 0xa0, 0x0c, 0x52,
	0x4b, 0x8c, 0x82, 0x82, 0x82, 0x0c, 0x52, 0x55,
	0x8c, 0x82, 0x82, 0x82, 0x0c, 0x42, 0x17, 0xa5,
	0x9b, 0x9b, 0x91, 0x0c, 0x42, 0x2d, 0xaa, 0xa0,
	0xa0, 0x96, 0x0c, 0x42, 0x3c, 0xaa, 0xa0, 0xa0,
	0x96, 0x0c, 0x42, 0x46, 0xaa, 0xa0, 0xa0, 0x96,
	0x0c, 0x42, 0x4b, 0x8c, 0x82, 0x82, 0x82, 0x0c,
	0x42, 0x55, 0x8c, 0x82, 0x82, 0x82, 0x0c, 0x3d,
	0x17, 0xa5, 0x9b, 0x9b, 0x91, 0x0c, 0x3d, 0x2d,
	0xaa, 0xa0, 0xa0, 0x96, 0x0c, 0x3d, 0x3c, 0xaa,
	0xa0, 0xa0, 0x8c, 0x0c, 0x3d, 0x46, 0xaa, 0xa0,
	0x96, 0x8c, 0x0c, 0x3d, 0x4b, 0x8c, 0x82, 0x82,
	0x82, 0x0c, 0x3d, 0x55, 0x8c, 0x82, 0x82, 0x82,
	0x0c, 0x32, 0x17, 0xa5, 0x9b, 0x91, 0x87, 0x0c,
	0x32, 0x2d, 0xaa, 0xa0, 0x96, 0x8c, 0x0c, 0x32,
	0x3c, 0xaa, 0xa0, 0x96, 0x82, 0x0c, 0x32, 0x46,
	0xaa, 0xa0, 0x8c, 0x78, 0x0c, 0x32, 0x4b, 0x8c,
	0x82, 0x82, 0x78, 0x0c, 0x32, 0x55, 0x8c, 0x82,
	0x82, 0x6e, 0x0c, 0x28, 0x17, 0xa5, 0x9b, 0x87,
	0x73, 0x0c, 0x28, 0x2d, 0xaa, 0xa0, 0x8c, 0x78,
	0x0c, 0x28, 0x3c, 0xaa, 0x96, 0x82, 0x73, 0x0c,
	0x28, 0x46, 0xaa, 0x96, 0x78, 0x69, 0x0c, 0x28,
	0x4b, 0x8c, 0x82, 0x78, 0x69, 0x0c, 0x28, 0x55,
	0x8c, 0x82, 0x6e, 0x5a, 0x0c, 0x23, 0x17, 0xa5,
	0x91, 0x7d, 0x69, 0x0c, 0x23, 0x2d, 0xaa, 0x96,
	0x82, 0x6e, 0x0c, 0x23, 0x3c, 0xaa, 0x96, 0x78,
	0x69, 0x0c, 0x23, 0x46, 0xaa, 0x8c, 0x6e, 0x5a,
	0x0c, 0x23, 0x4b, 0x8c, 0x82, 0x6e, 0x5a, 0x0c,
	0x23, 0x55, 0x8c, 0x82, 0x64, 0x50, 0x0d, 0x64,
	0x17, 0xa5, 0x9b, 0x9b, 0x9b, 0x0d, 0x64, 0x2d,
	0xaa, 0xa0, 0xa0, 0xa0, 0x0d, 0x64, 0x3c, 0xaa,
	0xa0, 0xa0, 0xa0, 0x0d, 0x64, 0x46, 0xaa, 0xa0,
	0xa0, 0xa0, 0x0d, 0x64, 0x4b, 0x8c, 0x82, 0x82,
	0x82, 0x0d, 0x64, 0x55, 0x8c, 0x82, 0x82, 0x82,
	0x0d, 0x50, 0x17, 0xa5, 0x9b, 0x9b, 0x9b, 0x0d,
	0x50, 0x2d, 0xaa, 0xa0, 0xa0, 0xa0, 0x0d, 0x50,
	0x3c, 0xaa, 0xa0, 0xa0, 0x96, 0x0d, 0x50, 0x46,
	0xaa, 0xa0, 0xa0, 0x96, 0x0d, 0x50, 0x4b, 0x8c,
	0x82, 0x82, 0x82, 0x0d, 0x50, 0x55, 0x8c, 0x82,
	0x82, 0x82, 0x0d, 0x3c, 0x17, 0xa5, 0x9b, 0x9b,
	0x87, 0x0d, 0x3c, 0x2d, 0xaa, 0xa0, 0xa0, 0x8c,
	0x0d, 0x3c, 0x3c, 0xaa, 0xa0, 0x96, 0x82, 0x0d,
	0x3c, 0x46, 0xaa, 0xa0, 0x96, 0x78, 0x0d, 0x3c,
	0x4b, 0x8c, 0x82, 0x82, 0x78, 0x0d, 0x3c, 0x55,
	0x8c, 0x82, 0x82, 0x6e, 0x0d, 0x28, 0x17, 0xa5,
	0x91, 0x7d, 0x69, 0x0d, 0x28, 0x2d, 0xaa, 0x96,
	0x82, 0x6e, 0x0d, 0x28, 0x3c, 0xaa, 0x96, 0x78,
	0x64, 0x0d, 0x28, 0x46, 0xaa, 0x8c, 0x6e, 0x5a,
	0x0d, 0x28, 0x4b, 0x8c, 0x82, 0x6e, 0x5a, 0x0d,
	0x28, 0x55, 0x8c, 0x82, 0x64, 0x50,
};


static struct system_edp_entry __initdata tegra_system_edp_map[] = {

/* {SKU, power-limit (in 100mW), {freq limits (in 10Mhz)} } */

	{  1,  49, {130, 120, 120, 120} },
	{  1,  44, {130, 120, 120, 110} },
	{  1,  37, {130, 120, 110, 100} },
	{  1,  35, {130, 120, 110,  90} },
	{  1,  29, {130, 120, 100,  80} },
	{  1,  27, {130, 120,  90,  80} },
	{  1,  25, {130, 110,  80,  60} },
	{  1,  21, {130, 100,  80,  40} },

	{  4,  49, {130, 120, 120, 120} },
	{  4,  44, {130, 120, 120, 110} },
	{  4,  37, {130, 120, 110, 100} },
	{  4,  35, {130, 120, 110,  90} },
	{  4,  29, {130, 120, 100,  80} },
	{  4,  27, {130, 120,  90,  80} },
	{  4,  25, {130, 110,  80,  60} },
	{  4,  21, {130, 100,  80,  40} },
};

/*
 * "Safe entry" to be used when no match for speedo_id /
 * regulator_cur is found; must be the last one
 */
static struct tegra_edp_limits edp_default_limits[] = {
	{85, {1000000, 1000000, 1000000, 1000000} },
};



/*
 * Specify regulator current in mA, e.g. 5000mA
 * Use 0 for default
 */
void __init tegra_init_cpu_edp_limits(unsigned int regulator_mA)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int i, j;
	struct tegra_edp_limits *e;
	struct tegra_edp_entry *t = (struct tegra_edp_entry *)tegra_edp_map;
	int tsize = sizeof(tegra_edp_map)/sizeof(struct tegra_edp_entry);

	if (!regulator_mA) {
		edp_limits = edp_default_limits;
		edp_limits_size = ARRAY_SIZE(edp_default_limits);
		return;
	}
	regulator_cur = regulator_mA;

	for (i = 0; i < tsize; i++) {
		if (t[i].speedo_id == cpu_speedo_id &&
		    t[i].regulator_100mA <= regulator_mA / 100)
			break;
	}

	/* No entry found in tegra_edp_map */
	if (i >= tsize) {
		edp_limits = edp_default_limits;
		edp_limits_size = ARRAY_SIZE(edp_default_limits);
		return;
	}

	/* Find all rows for this entry */
	for (j = i + 1; j < tsize; j++) {
		if (t[i].speedo_id != t[j].speedo_id ||
		    t[i].regulator_100mA != t[j].regulator_100mA)
			break;
	}

	edp_limits_size = j - i;
	e = kmalloc(sizeof(struct tegra_edp_limits) * edp_limits_size,
		    GFP_KERNEL);
	BUG_ON(!e);

	for (j = 0; j < edp_limits_size; j++) {
		e[j].temperature = (int)t[i+j].temperature;
		e[j].freq_limits[0] = (unsigned int)t[i+j].freq_limits[0] * 10000;
		e[j].freq_limits[1] = (unsigned int)t[i+j].freq_limits[1] * 10000;
		e[j].freq_limits[2] = (unsigned int)t[i+j].freq_limits[2] * 10000;
		e[j].freq_limits[3] = (unsigned int)t[i+j].freq_limits[3] * 10000;
	}

	if (edp_limits != edp_default_limits)
		kfree(edp_limits);

	edp_limits = e;
}


void __init tegra_init_system_edp_limits(unsigned int power_limit_mW)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int i;
	unsigned int *e;
	struct system_edp_entry *t =
		(struct system_edp_entry *)tegra_system_edp_map;
	int tsize = sizeof(tegra_system_edp_map) /
		sizeof(struct system_edp_entry);

	if (!power_limit_mW) {
		e = NULL;
		goto out;
	}

	for (i = 0; i < tsize; i++)
		if (t[i].speedo_id == cpu_speedo_id)
			break;

	if (i >= tsize) {
		e = NULL;
		goto out;
	}

	do {
		if (t[i].power_limit_100mW <= power_limit_mW / 100)
			break;
		i++;
	} while (i < tsize && t[i].speedo_id == cpu_speedo_id);

	if (i >= tsize || t[i].speedo_id != cpu_speedo_id)
		i--; /* No low enough entry in the table, use best possible */

	e = kmalloc(sizeof(unsigned int) * 4, GFP_KERNEL);
	BUG_ON(!e);

	e[0] = (unsigned int)t[i].freq_limits[0] * 10000;
	e[1] = (unsigned int)t[i].freq_limits[1] * 10000;
	e[2] = (unsigned int)t[i].freq_limits[2] * 10000;
	e[3] = (unsigned int)t[i].freq_limits[3] * 10000;

out:
	kfree(system_edp_limits);

	system_edp_limits = e;
}


void tegra_get_cpu_edp_limits(const struct tegra_edp_limits **limits, int *size)
{
	*limits = edp_limits;
	*size = edp_limits_size;
}

void tegra_get_system_edp_limits(const unsigned int **limits)
{
	*limits = system_edp_limits;
}

#ifdef CONFIG_DEBUG_FS

#ifdef CONFIG_TEGRA_VARIANT_INFO
extern int orig_cpu_process_id;
extern int orig_core_process_id;
extern int orig_cpu_speedo_id;
extern int orig_soc_speedo_id;

static int t3_variant_debugfs_show(struct seq_file *s, void *data)
{
	int cpu_speedo_id = orig_cpu_speedo_id;
	int soc_speedo_id = orig_soc_speedo_id;
	int cpu_process_id = orig_cpu_process_id;
	int core_process_id = orig_core_process_id;

	seq_printf(s, "cpu_speedo_id => %d\n", cpu_speedo_id);
	seq_printf(s, "soc_speedo_id => %d\n", soc_speedo_id);
	seq_printf(s, "cpu_process_id => %d\n", cpu_process_id);
	seq_printf(s, "core_process_id => %d\n", core_process_id);

	return 0;
}
#endif

static int edp_limit_debugfs_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%u\n", tegra_get_edp_limit());
	return 0;
}

static int edp_debugfs_show(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "-- CPU %sEDP table (%umA) --\n",
		   edp_limits == edp_default_limits ? "default " : "",
		   regulator_cur);
	for (i = 0; i < edp_limits_size; i++) {
		seq_printf(s, "%4dC: %10u %10u %10u %10u\n",
			   edp_limits[i].temperature,
			   edp_limits[i].freq_limits[0],
			   edp_limits[i].freq_limits[1],
			   edp_limits[i].freq_limits[2],
			   edp_limits[i].freq_limits[3]);
	}

	if (system_edp_limits) {
		seq_printf(s, "\n-- System EDP table --\n");
		seq_printf(s, "%10u %10u %10u %10u\n",
			   system_edp_limits[0],
			   system_edp_limits[1],
			   system_edp_limits[2],
			   system_edp_limits[3]);
	}

	return 0;
}

#ifdef CONFIG_TEGRA_VARIANT_INFO
static int t3_variant_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, t3_variant_debugfs_show, inode->i_private);
}
#endif

static int edp_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_debugfs_show, inode->i_private);
}

static int edp_limit_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, edp_limit_debugfs_show, inode->i_private);
}

#ifdef CONFIG_TEGRA_VARIANT_INFO
static const struct file_operations t3_variant_debugfs_fops = {
	.open		= t3_variant_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static const struct file_operations edp_debugfs_fops = {
	.open		= edp_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations edp_limit_debugfs_fops = {
	.open		= edp_limit_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_edp_debugfs_init(void)
{
	struct dentry *d;

#ifdef CONFIG_TEGRA_VARIANT_INFO
	d = debugfs_create_file("t3_variant", S_IRUGO, NULL, NULL,
				&t3_variant_debugfs_fops);
#endif

	d = debugfs_create_file("edp", S_IRUGO, NULL, NULL,
				&edp_debugfs_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("edp_limit", S_IRUGO, NULL, NULL,
				&edp_limit_debugfs_fops);

	return 0;
}

late_initcall(tegra_edp_debugfs_init);
#endif /* CONFIG_DEBUG_FS */
