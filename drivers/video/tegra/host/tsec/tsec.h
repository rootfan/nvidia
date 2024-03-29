/*
 * drivers/video/tegra/host/tsec/tsec.h
 *
 * Tegra TSEC Module Support
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVHOST_TSEC_H__
#define __NVHOST_TSEC_H__

#include <linux/types.h>
#include <linux/nvhost.h>

int nvhost_tsec_finalize_poweron_t186(struct platform_device *dev);
int nvhost_tsec_finalize_poweron(struct platform_device *dev);
int nvhost_tsec_prepare_poweroff(struct platform_device *dev);
void nvhost_tsec_isr(void);
void nvhost_tsec_isr_thread(void);

/* Would have preferred a static inline here... but we're using this
 * in a place where a constant initializer is required */
#define NVHOST_ENCODE_TSEC_VER(maj, min)	( (((maj) & 0xff) << 8) | ((min) & 0xff) )

static inline void decode_tsec_ver(int version, u8 *maj, u8 *min)
{
	u32 uv32 = (u32)version;
	*maj = (u8)((uv32 >> 8) & 0xff);
	*min = (u8)(uv32 & 0xff);
}

enum tsec_host1x_state_t {
	tsec_host1x_none,
	tsec_host1x_request_access,
	tsec_host1x_access_granted,
	tsec_host1x_release_access,
};

#endif
