# Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

OSI_COMMON := nvethernetrm/osi/common
OSI_CORE := nvethernetrm/osi/core
OSI_DMA := nvethernetrm/osi/dma

obj-$(CONFIG_NVETHERNET) += nvethernet.o

ccflags-y += -DTHERMAL_CAL -DLINUX_IVC -I$(srctree.nvidia)/drivers/net/ethernet/nvidia/nvethernet/nvethernetrm/include \
	     -I$(srctree.nvidia)/drivers/net/ethernet/nvidia/nvethernet/nvethernetrm/osi/common/include

nvethernet-objs:= ether_linux.o \
		  osd.o \
		  ethtool.o \
		  sysfs.o \
		  ioctl.o \
		  ptp.o \
		  $(OSI_CORE)/osi_core.o \
		  $(OSI_COMMON)/osi_common.o \
		  $(OSI_COMMON)/eqos_common.o \
		  $(OSI_DMA)/osi_dma.o \
		  $(OSI_DMA)/osi_dma_txrx.o \
		  $(OSI_CORE)/eqos_core.o \
		  $(OSI_CORE)/ivc_core.o \
		  $(OSI_CORE)/eqos_mmc.o \
		  $(OSI_DMA)/eqos_dma.o

nvethernet-$(CONFIG_NVETHERNET_SELFTESTS) += selftests.o
