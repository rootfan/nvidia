/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/version.h>
#include "ether_linux.h"

/**
 * @addtogroup MMC Stats array length.
 *
 * @brief Helper macro to find MMC stats array length.
 * @{
 */
#define OSI_ARRAY_SIZE(x)  ((int)sizeof((x)) / (int)sizeof((x)[0]))
#define ETHER_MMC_STATS_LEN OSI_ARRAY_SIZE(ether_mmc)
/** @} */

/**
 * @brief Ethernet stats
 */
struct ether_stats {
	/** Name of the stat */
	char stat_string[ETH_GSTRING_LEN];
	/** size of the stat */
	size_t sizeof_stat;
	/** stat offset */
	size_t stat_offset;
};

/**
 * @brief Name of pkt_err statistics, with length of name not more than
 * ETH_GSTRING_LEN
 */
#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
#define ETHER_PKT_ERR_STAT(y) \
{ (#y), FIELD_SIZEOF(struct osi_pkt_err_stats, y), \
	offsetof(struct osi_dma_priv_data, pkt_err_stats.y)}
#else
#define ETHER_PKT_ERR_STAT(y) \
{ (#y), sizeof_field(struct osi_pkt_err_stats, y), \
	offsetof(struct osi_dma_priv_data, pkt_err_stats.y)}
#endif
/**
 * @brief ETHER clear pkt_err statistics
 */
static const struct ether_stats ether_cstrings_stats[] = {
	/* Counter for pkt_err stats got cleared */
	ETHER_PKT_ERR_STAT(clear_tx_err),
	ETHER_PKT_ERR_STAT(clear_rx_err),
};

/**
 * @brief clear pkt_err statistics array length
 */
#define ETHER_PKT_ERR_STAT_LEN OSI_ARRAY_SIZE(ether_cstrings_stats)

/**
 * @brief Name of extra DMA stat, with length of name not more than ETH_GSTRING_LEN
 */
#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
#define ETHER_DMA_EXTRA_STAT(a) \
{ (#a), FIELD_SIZEOF(struct osi_xtra_dma_stat_counters, a), \
	offsetof(struct osi_dma_priv_data, dstats.a)}
#else
#define ETHER_DMA_EXTRA_STAT(a) \
{ (#a), sizeof_field(struct osi_xtra_dma_stat_counters, a), \
	offsetof(struct osi_dma_priv_data, dstats.a)}
#endif
/**
 * @brief Ethernet DMA extra statistics
 */
static const struct ether_stats ether_dstrings_stats[] = {
	ETHER_DMA_EXTRA_STAT(tx_clean_n[0]),
	ETHER_DMA_EXTRA_STAT(tx_clean_n[1]),
	ETHER_DMA_EXTRA_STAT(tx_clean_n[2]),
	ETHER_DMA_EXTRA_STAT(tx_clean_n[3]),

	/* Tx/Rx frames */
	ETHER_DMA_EXTRA_STAT(tx_pkt_n),
	ETHER_DMA_EXTRA_STAT(rx_pkt_n),
	ETHER_DMA_EXTRA_STAT(tx_vlan_pkt_n),
	ETHER_DMA_EXTRA_STAT(rx_vlan_pkt_n),
	ETHER_DMA_EXTRA_STAT(tx_tso_pkt_n),

	/* Tx/Rx frames per channels/queues */
	ETHER_DMA_EXTRA_STAT(q_tx_pkt_n[0]),
	ETHER_DMA_EXTRA_STAT(q_tx_pkt_n[1]),
	ETHER_DMA_EXTRA_STAT(q_tx_pkt_n[2]),
	ETHER_DMA_EXTRA_STAT(q_tx_pkt_n[3]),
	ETHER_DMA_EXTRA_STAT(q_rx_pkt_n[0]),
	ETHER_DMA_EXTRA_STAT(q_rx_pkt_n[1]),
	ETHER_DMA_EXTRA_STAT(q_rx_pkt_n[2]),
	ETHER_DMA_EXTRA_STAT(q_rx_pkt_n[3]),
};

/**
 * @brief Ethernet extra DMA statistics array length
 */
#define ETHER_EXTRA_DMA_STAT_LEN OSI_ARRAY_SIZE(ether_dstrings_stats)

/**
 * @brief Name of extra Ethernet stats, with length of name not more than
 * ETH_GSTRING_LEN MAC
 */
#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
#define ETHER_EXTRA_STAT(b) \
{ #b, FIELD_SIZEOF(struct osi_xtra_stat_counters, b), \
	offsetof(struct osi_core_priv_data, xstats.b)}
#else
#define ETHER_EXTRA_STAT(b) \
{ #b, sizeof_field(struct osi_xtra_stat_counters, b), \
	offsetof(struct osi_core_priv_data, xstats.b)}
#endif
/**
 * @brief Ethernet extra statistics
 */
static const struct ether_stats ether_gstrings_stats[] = {
	ETHER_EXTRA_STAT(re_alloc_rxbuf_failed[0]),
	ETHER_EXTRA_STAT(re_alloc_rxbuf_failed[1]),
	ETHER_EXTRA_STAT(re_alloc_rxbuf_failed[2]),
	ETHER_EXTRA_STAT(re_alloc_rxbuf_failed[3]),

	/* Tx/Rx IRQ error info */
	ETHER_EXTRA_STAT(tx_proc_stopped_irq_n[0]),
	ETHER_EXTRA_STAT(tx_proc_stopped_irq_n[1]),
	ETHER_EXTRA_STAT(tx_proc_stopped_irq_n[2]),
	ETHER_EXTRA_STAT(tx_proc_stopped_irq_n[3]),
	ETHER_EXTRA_STAT(rx_proc_stopped_irq_n[0]),
	ETHER_EXTRA_STAT(rx_proc_stopped_irq_n[1]),
	ETHER_EXTRA_STAT(rx_proc_stopped_irq_n[2]),
	ETHER_EXTRA_STAT(rx_proc_stopped_irq_n[3]),
	ETHER_EXTRA_STAT(tx_buf_unavail_irq_n[0]),
	ETHER_EXTRA_STAT(tx_buf_unavail_irq_n[1]),
	ETHER_EXTRA_STAT(tx_buf_unavail_irq_n[2]),
	ETHER_EXTRA_STAT(tx_buf_unavail_irq_n[3]),
	ETHER_EXTRA_STAT(rx_buf_unavail_irq_n[0]),
	ETHER_EXTRA_STAT(rx_buf_unavail_irq_n[1]),
	ETHER_EXTRA_STAT(rx_buf_unavail_irq_n[2]),
	ETHER_EXTRA_STAT(rx_buf_unavail_irq_n[3]),
	ETHER_EXTRA_STAT(rx_watchdog_irq_n),
	ETHER_EXTRA_STAT(fatal_bus_error_irq_n),

	/* Tx/Rx IRQ Events */
	ETHER_EXTRA_STAT(tx_normal_irq_n[0]),
	ETHER_EXTRA_STAT(tx_normal_irq_n[1]),
	ETHER_EXTRA_STAT(tx_normal_irq_n[2]),
	ETHER_EXTRA_STAT(tx_normal_irq_n[3]),
	ETHER_EXTRA_STAT(tx_usecs_swtimer_n[0]),
	ETHER_EXTRA_STAT(tx_usecs_swtimer_n[1]),
	ETHER_EXTRA_STAT(tx_usecs_swtimer_n[2]),
	ETHER_EXTRA_STAT(tx_usecs_swtimer_n[3]),
	ETHER_EXTRA_STAT(rx_normal_irq_n[0]),
	ETHER_EXTRA_STAT(rx_normal_irq_n[1]),
	ETHER_EXTRA_STAT(rx_normal_irq_n[2]),
	ETHER_EXTRA_STAT(rx_normal_irq_n[3]),
	ETHER_EXTRA_STAT(link_disconnect_count),
	ETHER_EXTRA_STAT(link_connect_count),
};

/**
 * @brief Ethernet extra statistics array length
 */
#define ETHER_EXTRA_STAT_LEN OSI_ARRAY_SIZE(ether_gstrings_stats)

/**
 * @brief HW MAC Management counters
 * 	  Structure variable name MUST up to MAX length of ETH_GSTRING_LEN
 */
#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
#define ETHER_MMC_STAT(c) \
{ #c, FIELD_SIZEOF(struct osi_mmc_counters, c), \
	offsetof(struct osi_core_priv_data, mmc.c)}
#else
#define ETHER_MMC_STAT(c) \
{ #c, sizeof_field(struct osi_mmc_counters, c), \
	offsetof(struct osi_core_priv_data, mmc.c)}
#endif

/**
 * @brief MMC statistics
 */
static const struct ether_stats ether_mmc[] = {
	/* MMC TX counters */
	ETHER_MMC_STAT(mmc_tx_octetcount_gb),
	ETHER_MMC_STAT(mmc_tx_framecount_gb),
	ETHER_MMC_STAT(mmc_tx_broadcastframe_g),
	ETHER_MMC_STAT(mmc_tx_multicastframe_g),
	ETHER_MMC_STAT(mmc_tx_64_octets_gb),
	ETHER_MMC_STAT(mmc_tx_65_to_127_octets_gb),
	ETHER_MMC_STAT(mmc_tx_128_to_255_octets_gb),
	ETHER_MMC_STAT(mmc_tx_256_to_511_octets_gb),
	ETHER_MMC_STAT(mmc_tx_512_to_1023_octets_gb),
	ETHER_MMC_STAT(mmc_tx_1024_to_max_octets_gb),
	ETHER_MMC_STAT(mmc_tx_unicast_gb),
	ETHER_MMC_STAT(mmc_tx_multicast_gb),
	ETHER_MMC_STAT(mmc_tx_broadcast_gb),
	ETHER_MMC_STAT(mmc_tx_underflow_error),
	ETHER_MMC_STAT(mmc_tx_singlecol_g),
	ETHER_MMC_STAT(mmc_tx_multicol_g),
	ETHER_MMC_STAT(mmc_tx_deferred),
	ETHER_MMC_STAT(mmc_tx_latecol),
	ETHER_MMC_STAT(mmc_tx_exesscol),
	ETHER_MMC_STAT(mmc_tx_carrier_error),
	ETHER_MMC_STAT(mmc_tx_octetcount_g),
	ETHER_MMC_STAT(mmc_tx_framecount_g),
	ETHER_MMC_STAT(mmc_tx_excessdef),
	ETHER_MMC_STAT(mmc_tx_pause_frame),
	ETHER_MMC_STAT(mmc_tx_vlan_frame_g),

	/* MMC RX counters */
	ETHER_MMC_STAT(mmc_rx_framecount_gb),
	ETHER_MMC_STAT(mmc_rx_octetcount_gb),
	ETHER_MMC_STAT(mmc_rx_octetcount_g),
	ETHER_MMC_STAT(mmc_rx_broadcastframe_g),
	ETHER_MMC_STAT(mmc_rx_multicastframe_g),
	ETHER_MMC_STAT(mmc_rx_crc_error),
	ETHER_MMC_STAT(mmc_rx_align_error),
	ETHER_MMC_STAT(mmc_rx_runt_error),
	ETHER_MMC_STAT(mmc_rx_jabber_error),
	ETHER_MMC_STAT(mmc_rx_undersize_g),
	ETHER_MMC_STAT(mmc_rx_oversize_g),
	ETHER_MMC_STAT(mmc_rx_64_octets_gb),
	ETHER_MMC_STAT(mmc_rx_65_to_127_octets_gb),
	ETHER_MMC_STAT(mmc_rx_128_to_255_octets_gb),
	ETHER_MMC_STAT(mmc_rx_256_to_511_octets_gb),
	ETHER_MMC_STAT(mmc_rx_512_to_1023_octets_gb),
	ETHER_MMC_STAT(mmc_rx_1024_to_max_octets_gb),
	ETHER_MMC_STAT(mmc_rx_unicast_g),
	ETHER_MMC_STAT(mmc_rx_length_error),
	ETHER_MMC_STAT(mmc_rx_outofrangetype),
	ETHER_MMC_STAT(mmc_rx_pause_frames),
	ETHER_MMC_STAT(mmc_rx_fifo_overflow),
	ETHER_MMC_STAT(mmc_rx_vlan_frames_gb),
	ETHER_MMC_STAT(mmc_rx_watchdog_error),
	ETHER_MMC_STAT(mmc_rx_receive_error),
	ETHER_MMC_STAT(mmc_rx_ctrl_frames_g),

	/* LPI */
	ETHER_MMC_STAT(mmc_tx_lpi_usec_cntr),
	ETHER_MMC_STAT(mmc_tx_lpi_tran_cntr),
	ETHER_MMC_STAT(mmc_rx_lpi_usec_cntr),
	ETHER_MMC_STAT(mmc_rx_lpi_tran_cntr),

	/* IPv4 */
	ETHER_MMC_STAT(mmc_rx_ipv4_gd),
	ETHER_MMC_STAT(mmc_rx_ipv4_hderr),
	ETHER_MMC_STAT(mmc_rx_ipv4_nopay),
	ETHER_MMC_STAT(mmc_rx_ipv4_frag),
	ETHER_MMC_STAT(mmc_rx_ipv4_udsbl),

	/* IPV6 */
	ETHER_MMC_STAT(mmc_rx_ipv6_gd_octets),
	ETHER_MMC_STAT(mmc_rx_ipv6_hderr_octets),
	ETHER_MMC_STAT(mmc_rx_ipv6_nopay_octets),

	/* Protocols */
	ETHER_MMC_STAT(mmc_rx_udp_gd),
	ETHER_MMC_STAT(mmc_rx_udp_err),
	ETHER_MMC_STAT(mmc_rx_tcp_gd),
	ETHER_MMC_STAT(mmc_rx_tcp_err),
	ETHER_MMC_STAT(mmc_rx_icmp_gd),
	ETHER_MMC_STAT(mmc_rx_icmp_err),

	/* IPv4 */
	ETHER_MMC_STAT(mmc_rx_ipv4_gd_octets),
	ETHER_MMC_STAT(mmc_rx_ipv4_hderr_octets),
	ETHER_MMC_STAT(mmc_rx_ipv4_nopay_octets),
	ETHER_MMC_STAT(mmc_rx_ipv4_frag_octets),
	ETHER_MMC_STAT(mmc_rx_ipv4_udsbl_octets),

	/* IPV6 */
	ETHER_MMC_STAT(mmc_rx_ipv6_gd),
	ETHER_MMC_STAT(mmc_rx_ipv6_hderr),
	ETHER_MMC_STAT(mmc_rx_ipv6_nopay),

	/* Protocols */
	ETHER_MMC_STAT(mmc_rx_udp_gd_octets),
	ETHER_MMC_STAT(mmc_rx_udp_err_octets),
	ETHER_MMC_STAT(mmc_rx_tcp_gd_octets),
	ETHER_MMC_STAT(mmc_rx_tcp_err_octets),
	ETHER_MMC_STAT(mmc_rx_icmp_gd_octets),
	ETHER_MMC_STAT(mmc_rx_icmp_err_octets),
};

/**
 * @brief This function is invoked by kernel when user requests to get the
 *  extended statistics about the device.
 *
 *  Algorithm: read mmc register and create strings
 *
 * @param[in] dev: pointer to net device structure.
 * @param[in] dummy: dummy parameter of ethtool_stats type.
 * @param[in] data: Pointer in which MMC statistics should be put.
 *
 * @note Network device needs to created.
 */
static void ether_get_ethtool_stats(struct net_device *dev,
				    struct ethtool_stats *dummy,
				    u64 *data)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct osi_ioctl ioctl_data = {};
	int i, j = 0;
	int ret;

	if (!netif_running(dev)) {
		netdev_err(pdata->ndev, "%s: iface not up\n", __func__);
		return;
	}

	if (pdata->hw_feat.mmc_sel == 1U) {
		ioctl_data.cmd = OSI_CMD_READ_MMC;
		ret = osi_handle_ioctl(osi_core, &ioctl_data);
		if (ret == -1) {
			dev_err(pdata->dev, "Error in reading MMC counter\n");
			return;
		}

		for (i = 0; i < ETHER_MMC_STATS_LEN; i++) {
			char *p = (char *)osi_core + ether_mmc[i].stat_offset;

			data[j++] = (ether_mmc[i].sizeof_stat ==
					sizeof(u64)) ? (*(u64 *)p) :
				     (*(u32 *)p);
		}

		for (i = 0; i < ETHER_EXTRA_STAT_LEN; i++) {
			char *p = (char *)osi_core +
				  ether_gstrings_stats[i].stat_offset;

			data[j++] = (ether_gstrings_stats[i].sizeof_stat ==
				     sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
		}

		for (i = 0; i < ETHER_EXTRA_DMA_STAT_LEN; i++) {
			char *p = (char *)osi_dma +
				  ether_dstrings_stats[i].stat_offset;

			data[j++] = (ether_dstrings_stats[i].sizeof_stat ==
				     sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
		}

		for (i = 0; i < ETHER_PKT_ERR_STAT_LEN; i++) {
			char *p = (char *)osi_dma +
				  ether_cstrings_stats[i].stat_offset;

			data[j++] = (ether_cstrings_stats[i].sizeof_stat ==
				     sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
		}
	}
}

/**
 * @brief This function gets number of strings
 *
 * Algorithm: return number of strings.
 *
 * @param[in] dev: Pointer to net device structure.
 * @param[in] sset: String set value.
 *
 * @note Network device needs to created.
 *
 * @return Numbers of strings(total length)
 */
static int ether_get_sset_count(struct net_device *dev, int sset)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	int len = 0;

	if (sset == ETH_SS_STATS) {
		if (pdata->hw_feat.mmc_sel == OSI_ENABLE) {
			if (INT_MAX < ETHER_MMC_STATS_LEN) {
				/* do nothing*/
			} else {
				len = ETHER_MMC_STATS_LEN;
			}
		}
		if (INT_MAX - ETHER_EXTRA_STAT_LEN < len) {
			/* do nothing */
		} else {
			len += ETHER_EXTRA_STAT_LEN;
		}
		if (INT_MAX - ETHER_EXTRA_DMA_STAT_LEN < len) {
			/* do nothing */
		} else {
			len += ETHER_EXTRA_DMA_STAT_LEN;
		}
		if (INT_MAX - ETHER_PKT_ERR_STAT_LEN < len) {
			/* do nothing */
		} else {
			len += ETHER_PKT_ERR_STAT_LEN;
		}
	} else if (sset == ETH_SS_TEST) {
		len = ether_selftest_get_count(pdata);
	} else {
		len = -EOPNOTSUPP;
	}

	return len;
}

/**	
 * @brief This function returns a set of strings that describe
 * the requested objects.
 *
 * Algorithm: return number of strings.
 *
 * @param[in] dev: Pointer to net device structure.
 * @param[in] stringset:  String set value.
 * @param[in] data: Pointer in which requested string should be put.
 *
 * @note Network device needs to created.
 */
static void ether_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	u8 *p = data;
	u8 *str;
	int i;

	if (stringset == (u32)ETH_SS_STATS) {
		if (pdata->hw_feat.mmc_sel == OSI_ENABLE) {
			for (i = 0; i < ETHER_MMC_STATS_LEN; i++) {
				str = (u8 *)ether_mmc[i].stat_string;
				if (memcpy(p, str, ETH_GSTRING_LEN) ==
				    OSI_NULL) {
					return;
				}
				p += ETH_GSTRING_LEN;
			}

			for (i = 0; i < ETHER_EXTRA_STAT_LEN; i++) {
				str = (u8 *)ether_gstrings_stats[i].stat_string;
				if (memcpy(p, str, ETH_GSTRING_LEN) ==
				    OSI_NULL) {
					return;
				}
				p += ETH_GSTRING_LEN;
			}
			for (i = 0; i < ETHER_EXTRA_DMA_STAT_LEN; i++) {
				str = (u8 *)ether_dstrings_stats[i].stat_string;
				if (memcpy(p, str, ETH_GSTRING_LEN) ==
				    OSI_NULL) {
					return;
				}
				p += ETH_GSTRING_LEN;
			}
			for (i = 0; i < ETHER_PKT_ERR_STAT_LEN; i++) {
				str = (u8 *)ether_cstrings_stats[i].stat_string;
				if (memcpy(p, str, ETH_GSTRING_LEN) ==
				    OSI_NULL) {
					return;
				}
				p += ETH_GSTRING_LEN;
			}
		}
	} else if (stringset == (u32)ETH_SS_TEST) {
		ether_selftest_get_strings(pdata, p);
	} else {
		dev_err(pdata->dev, "%s() Unsupported stringset\n", __func__);
	}
}

/**
 * @brief Get pause frame settings
 *
 * Algorithm: Gets pause frame configuration
 *
 * @param[in] ndev: network device instance
 * @param[out] pause: Pause parameters that are set currently
 *
 * @note Network device needs to created.
 */
static void ether_get_pauseparam(struct net_device *ndev,
				 struct ethtool_pauseparam *pause)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct phy_device *phydev = pdata->phydev;

	if (!netif_running(ndev)) {
		netdev_err(pdata->ndev, "interface must be up\n");
		return;
	}

	/* return if pause frame is not supported */
	if ((pdata->osi_core->pause_frames == OSI_PAUSE_FRAMES_DISABLE) ||
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	    (!linkmode_test_bit(SUPPORTED_Pause, phydev->supported) ||
	    !linkmode_test_bit(SUPPORTED_Asym_Pause, phydev->supported))) {
#else
	    (!(phydev->supported & SUPPORTED_Pause) ||
	    !(phydev->supported & SUPPORTED_Asym_Pause))) {
#endif
		dev_err(pdata->dev, "FLOW control not supported\n");
		return;
	}

	/* update auto negotiation */
	pause->autoneg = phydev->autoneg;

	/* update rx pause parameter */
	if ((pdata->osi_core->flow_ctrl & OSI_FLOW_CTRL_RX) ==
	    OSI_FLOW_CTRL_RX) {
		pause->rx_pause = 1;
	}

	/* update tx pause parameter */
	if ((pdata->osi_core->flow_ctrl & OSI_FLOW_CTRL_TX) ==
	    OSI_FLOW_CTRL_TX) {
		pause->tx_pause = 1;
	}
}

/**
 * @brief Set pause frame settings
 *
 * Algorithm: Sets pause frame settings
 *
 * @param[in] ndev: network device instance
 * @param[in] pause: Pause frame settings
 *
 * @note Network device needs to created.
 *
 * @retval 0 on Sucess
 * @retval "negative value" on failure.
 */
static int ether_set_pauseparam(struct net_device *ndev,
				struct ethtool_pauseparam *pause)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_ioctl ioctl_data = {};
	struct phy_device *phydev = pdata->phydev;
	int curflow_ctrl = OSI_FLOW_CTRL_DISABLE;
	int ret;

	if (!netif_running(ndev)) {
		netdev_err(pdata->ndev, "interface must be up\n");
		return -EINVAL;
	}

	/* return if pause frame is not supported */
	if ((pdata->osi_core->pause_frames == OSI_PAUSE_FRAMES_DISABLE) ||
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	    (!linkmode_test_bit(SUPPORTED_Pause, phydev->supported) ||
	    !linkmode_test_bit(SUPPORTED_Asym_Pause, phydev->supported))) {
#else
	    (!(phydev->supported & SUPPORTED_Pause) ||
	    !(phydev->supported & SUPPORTED_Asym_Pause))) {
#endif
		dev_err(pdata->dev, "FLOW control not supported\n");
		return -EOPNOTSUPP;
	}

	dev_err(pdata->dev, "autoneg = %d tx_pause = %d rx_pause = %d\n",
		pause->autoneg, pause->tx_pause, pause->rx_pause);

	if (pause->tx_pause)
		curflow_ctrl |= OSI_FLOW_CTRL_TX;

	if (pause->rx_pause)
		curflow_ctrl |= OSI_FLOW_CTRL_RX;

	/* update flow control setting */
	pdata->osi_core->flow_ctrl = curflow_ctrl;
	/* update autonegotiation */
	phydev->autoneg = pause->autoneg;

	/*If autonegotiation is enabled,start auto-negotiation
	 * for this PHY device and return, so that flow control
	 * settings will be done once we receive the link changed
	 * event i.e in ether_adjust_link
	 */
	if (phydev->autoneg && netif_running(ndev)) {
		return phy_start_aneg(phydev);
	}

	/* Configure current flow control settings */
	ioctl_data.cmd = OSI_CMD_FLOW_CTRL;
	ioctl_data.arg1_u32 = pdata->osi_core->flow_ctrl;
	ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(pdata->dev, "Setting flow control failed\n");
		return -EFAULT;
	}

	return ret;
}

/**
 * @brief Get HW supported time stamping.
 *
 * Algorithm: Function used to query the PTP capabilities for given netdev.
 *
 * @param[in] ndev: Net device data.
 * @param[in] info: Holds device supported timestamping types
 *
 * @note HW need to support PTP functionality.
 *
 * @return zero on success
 */
static int ether_get_ts_info(struct net_device *ndev,
		struct ethtool_ts_info *info)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);

	info->so_timestamping = SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_TX_SOFTWARE |
				SOF_TIMESTAMPING_RX_SOFTWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE |
				SOF_TIMESTAMPING_SOFTWARE;

	if (pdata->ptp_clock) {
		info->phc_index = ptp_clock_index(pdata->ptp_clock);
	}

	info->tx_types = (1 << HWTSTAMP_TX_OFF) | (1 << HWTSTAMP_TX_ON);

	info->rx_filters |= ((1 << HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
			     (1 << HWTSTAMP_FILTER_PTP_V2_L2_SYNC) |
			     (1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
			     (1 << HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
			     (1 << HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ) |
			     (1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
			     (1 << HWTSTAMP_FILTER_PTP_V2_EVENT) |
			     (1 << HWTSTAMP_FILTER_NONE));

	return 0;
}

/**
 * @brief Set interrupt coalescing parameters.
 *
 * Algorithm: This function is invoked by kernel when user request to set
 * interrupt coalescing parameters. This driver maintains same coalescing
 * parameters for all the channels, hence same changes will be applied to
 * all the channels.
 *
 * @param[in] dev: Net device data.
 * @param[in] ec: pointer to ethtool_coalesce structure
 *
 * @note Interface need to be bring down for setting these parameters
 *
 * @retval 0 on Sucess
 * @retval "negative value" on failure.
 */
static int ether_set_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *ec)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;

	if (netif_running(dev)) {
		netdev_err(dev, "Coalesce parameters can be changed"
			   " only if interface is down\n");
		return -EINVAL;
	}

	/* Check for not supported parameters  */
	if ((ec->rx_coalesce_usecs_irq) ||
	    (ec->rx_max_coalesced_frames_irq) || (ec->tx_coalesce_usecs_irq) ||
	    (ec->use_adaptive_rx_coalesce) || (ec->use_adaptive_tx_coalesce) ||
	    (ec->pkt_rate_low) || (ec->rx_coalesce_usecs_low) ||
	    (ec->rx_max_coalesced_frames_low) || (ec->tx_coalesce_usecs_high) ||
	    (ec->tx_max_coalesced_frames_low) || (ec->pkt_rate_high) ||
	    (ec->tx_coalesce_usecs_low) || (ec->rx_coalesce_usecs_high) ||
	    (ec->rx_max_coalesced_frames_high) ||
	    (ec->tx_max_coalesced_frames_irq)  ||
	    (ec->stats_block_coalesce_usecs)   ||
	    (ec->tx_max_coalesced_frames_high) || (ec->rate_sample_interval)) {
		return -EOPNOTSUPP;
	}

	if (ec->tx_max_coalesced_frames == OSI_DISABLE) {
		osi_dma->use_tx_frames = OSI_DISABLE;
	} else if ((ec->tx_max_coalesced_frames > ETHER_TX_MAX_FRAME) ||
		(ec->tx_max_coalesced_frames < OSI_MIN_TX_COALESCE_FRAMES)) {
		netdev_err(dev,
			   "invalid tx-frames, must be in the range of"
			   " %d to %ld frames\n", OSI_MIN_TX_COALESCE_FRAMES,
			   ETHER_TX_MAX_FRAME);
		return -EINVAL;
	} else {
		osi_dma->use_tx_frames = OSI_ENABLE;
	}

	if (ec->tx_coalesce_usecs == OSI_DISABLE) {
		osi_dma->use_tx_usecs = OSI_DISABLE;
	} else if ((ec->tx_coalesce_usecs > OSI_MAX_TX_COALESCE_USEC) ||
		   (ec->tx_coalesce_usecs < OSI_MIN_TX_COALESCE_USEC)) {
		netdev_err(dev,
			   "invalid tx_usecs, must be in a range of"
			   " %d to %d usec\n", OSI_MIN_TX_COALESCE_USEC,
			   OSI_MAX_TX_COALESCE_USEC);
		return -EINVAL;
	} else {
		osi_dma->use_tx_usecs = OSI_ENABLE;
	}

	netdev_err(dev, "TX COALESCING USECS is %s\n", osi_dma->use_tx_usecs ?
		   "ENABLED" : "DISABLED");

	netdev_err(dev, "TX COALESCING FRAMES is %s\n", osi_dma->use_tx_frames ?
		   "ENABLED" : "DISABLED");

	if (ec->rx_max_coalesced_frames == OSI_DISABLE) {
		osi_dma->use_rx_frames = OSI_DISABLE;
	} else if ((ec->rx_max_coalesced_frames > RX_DESC_CNT) ||
		(ec->rx_max_coalesced_frames < OSI_MIN_RX_COALESCE_FRAMES)) {
		netdev_err(dev,
			   "invalid rx-frames, must be in the range of"
			   " %d to %d frames\n", OSI_MIN_RX_COALESCE_FRAMES,
			   RX_DESC_CNT);
		return -EINVAL;
	} else {
		osi_dma->use_rx_frames = OSI_ENABLE;
	}

	if (ec->rx_coalesce_usecs == OSI_DISABLE) {
		osi_dma->use_riwt = OSI_DISABLE;
	} else if ((ec->rx_coalesce_usecs > OSI_MAX_RX_COALESCE_USEC) ||
		   (ec->rx_coalesce_usecs < OSI_MIN_RX_COALESCE_USEC)) {
		netdev_err(dev,
			   "invalid rx_usecs, must be in a range of"
			   " %d to %d usec\n", OSI_MIN_RX_COALESCE_USEC,
			   OSI_MAX_RX_COALESCE_USEC);
		return -EINVAL;
	} else {
		osi_dma->use_riwt = OSI_ENABLE;
	}

	if (osi_dma->use_tx_usecs == OSI_DISABLE &&
	    osi_dma->use_tx_frames == OSI_ENABLE) {
		netdev_err(dev, "invalid settings : tx-frames must be enabled"
			   " along with tx-usecs\n");
		return -EINVAL;
	}
	if (osi_dma->use_riwt == OSI_DISABLE &&
	    osi_dma->use_rx_frames == OSI_ENABLE) {
		netdev_err(dev, "invalid settings : rx-frames must be enabled"
			   " along with rx-usecs\n");
		return -EINVAL;
	}
	netdev_err(dev, "RX COALESCING USECS is %s\n", osi_dma->use_riwt ?
		   "ENABLED" : "DISABLED");

	netdev_err(dev, "RX COALESCING FRAMES is %s\n", osi_dma->use_rx_frames ?
		   "ENABLED" : "DISABLED");

	osi_dma->rx_riwt = ec->rx_coalesce_usecs;
	osi_dma->rx_frames = ec->rx_max_coalesced_frames;
	osi_dma->tx_usecs = ec->tx_coalesce_usecs;
	osi_dma->tx_frames = ec->tx_max_coalesced_frames;
	return 0;
}

/**
 * @brief Set interrupt coalescing parameters.
 *
 * Algorithm: This function is invoked by kernel when user request to get
 * interrupt coalescing parameters. As coalescing parameters are same
 * for all the channels, so this function will get coalescing
 * details from channel zero and return.
 *
 * @param[in] dev: Net device data.
 * @param[in] ec: pointer to ethtool_coalesce structure
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval 0 on Success.
 */
static int ether_get_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *ec)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;

	memset(ec, 0, sizeof(struct ethtool_coalesce));
	ec->rx_coalesce_usecs = osi_dma->rx_riwt;
	ec->rx_max_coalesced_frames = osi_dma->rx_frames;
	ec->tx_coalesce_usecs = osi_dma->tx_usecs;
	ec->tx_max_coalesced_frames = osi_dma->tx_frames;

	return 0;
}

/*
 * @brief Get current EEE configuration in MAC/PHY
 *
 * Algorithm: This function is invoked by kernel when user request to get
 * current EEE parameters. The function invokes the PHY framework to fill
 * the supported & advertised EEE modes, as well as link partner EEE modes
 * if it is available.
 *
 * @param[in] ndev: Net device data.
 * @param[in] cur_eee: Pointer to struct ethtool_eee
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval 0 on Success.
 * @retval -ve on Failure
 */
static int ether_get_eee(struct net_device *ndev,
			 struct ethtool_eee *cur_eee)
{
	int ret;
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct phy_device *phydev = pdata->phydev;

	if (!pdata->hw_feat.eee_sel) {
		return -EOPNOTSUPP;
	}

	if (!netif_running(ndev)) {
		netdev_err(pdata->ndev, "interface not up\n");
		return -EINVAL;
	}

	ret = phy_ethtool_get_eee(phydev, cur_eee);
	if (ret) {
		netdev_warn(pdata->ndev, "Cannot get PHY EEE config\n");
		return ret;
	}

	cur_eee->eee_enabled = pdata->eee_enabled;
	cur_eee->tx_lpi_enabled = pdata->tx_lpi_enabled;
	cur_eee->eee_active = pdata->eee_active;
	cur_eee->tx_lpi_timer = pdata->tx_lpi_timer;

	return ret;
}

/**
 * @brief Helper routing to validate EEE configuration requested via ethtool
 *
 * Algorithm: Check for invalid combinations of ethtool_eee fields. If any
 *	invalid combination found, override it.
 *
 * @param[in] ndev: Net device data.
 * @param[in] eee_req: Pointer to struct ethtool_eee configuration requested
 *
 * @retval none
 */
static inline void validate_eee_conf(struct net_device *ndev,
				     struct ethtool_eee *eee_req,
				     struct ethtool_eee cur_eee)
{
	/* These are the invalid combinations that can be requested.
	 * EEE | Tx LPI | Rx LPI
	 *----------------------
	 * 0   | 0      | 1
	 * 0   | 1      | 0
	 * 0   | 1      | 1
	 * 1   | 0      | 0
	 *
	 * These combinations can be entered from a state where either EEE was
	 * enabled or disabled originally. Hence decide next valid state based
	 * on whether EEE has toggled or not.
	 */
	if (!eee_req->eee_enabled && !eee_req->tx_lpi_enabled &&
	    eee_req->advertised) {
		if (eee_req->eee_enabled != cur_eee.eee_enabled) {
			netdev_warn(ndev, "EEE off. Set Rx LPI off\n");
			eee_req->advertised = OSI_DISABLE;
		} else {
			netdev_warn(ndev, "Rx LPI on. Set EEE on\n");
			eee_req->eee_enabled = OSI_ENABLE;
		}
	}

	if (!eee_req->eee_enabled && eee_req->tx_lpi_enabled &&
	    !eee_req->advertised) {
		if (eee_req->eee_enabled != cur_eee.eee_enabled) {
			netdev_warn(ndev, "EEE off. Set Tx LPI off\n");
			eee_req->tx_lpi_enabled = OSI_DISABLE;
		} else {
			/* phy_init_eee will fail if Rx LPI advertisement is
			 * disabled. Hence change the adv back to enable,
			 * so that Tx LPI will be set.
			 */
			netdev_warn(ndev, "Tx LPI on. Set EEE & Rx LPI on\n");
			eee_req->eee_enabled = OSI_ENABLE;
			eee_req->advertised = eee_req->supported;
		}
	}

	if (!eee_req->eee_enabled && eee_req->tx_lpi_enabled &&
	    eee_req->advertised) {
		if (eee_req->eee_enabled != cur_eee.eee_enabled) {
			netdev_warn(ndev, "EEE off. Set Tx & Rx LPI off\n");
			eee_req->tx_lpi_enabled = OSI_DISABLE;
			eee_req->advertised = OSI_DISABLE;
		} else {
			netdev_warn(ndev, "Tx & Rx LPI on. Set EEE on\n");
			eee_req->eee_enabled = OSI_ENABLE;
		}
	}

	if (eee_req->eee_enabled && !eee_req->tx_lpi_enabled &&
	    !eee_req->advertised) {
		if (eee_req->eee_enabled != cur_eee.eee_enabled) {
			netdev_warn(ndev, "EEE on. Set Tx & Rx LPI on\n");
			eee_req->tx_lpi_enabled = OSI_ENABLE;
			eee_req->advertised = eee_req->supported;
		} else {
			netdev_warn(ndev, "Tx,Rx LPI off. Set EEE off\n");
			eee_req->eee_enabled = OSI_DISABLE;
		}
	}
}

/**
 * @brief Set current EEE configuration
 *
 * Algorithm: This function is invoked by kernel when user request to Set
 * current EEE parameters.
 *
 * @param[in] ndev: Net device data.
 * @param[in] eee_req: Pointer to struct ethtool_eee
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval 0 on Success.
 * @retval -ve on Failure
 */
static int ether_set_eee(struct net_device *ndev,
			 struct ethtool_eee *eee_req)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct phy_device *phydev = pdata->phydev;
	struct ethtool_eee cur_eee;

	if (!pdata->hw_feat.eee_sel) {
		return -EOPNOTSUPP;
	}

	if (!netif_running(ndev)) {
		netdev_err(pdata->ndev, "interface not up\n");
		return -EINVAL;
	}

	if (ether_get_eee(ndev, &cur_eee)) {
		return -EOPNOTSUPP;
	}

	/* Validate args
	 * 1. Validate the tx lpi timer for acceptable range */
	if (cur_eee.tx_lpi_timer != eee_req->tx_lpi_timer) {
		if (eee_req->tx_lpi_timer == 0) {
			pdata->tx_lpi_timer = OSI_DEFAULT_TX_LPI_TIMER;
		} else if (eee_req->tx_lpi_timer <= OSI_MAX_TX_LPI_TIMER &&
			   eee_req->tx_lpi_timer >= OSI_MIN_TX_LPI_TIMER &&
			   !(eee_req->tx_lpi_timer % OSI_MIN_TX_LPI_TIMER)) {
			pdata->tx_lpi_timer = eee_req->tx_lpi_timer;
		} else {
			netdev_err(ndev, "Tx LPI timer has to be < %u usec "
				   "in %u usec steps\n", OSI_MAX_TX_LPI_TIMER,
				   OSI_MIN_TX_LPI_TIMER);
			return -EINVAL;
		}
	}

	/* 2. Override invalid combinations of requested configuration */
	validate_eee_conf(ndev, eee_req, cur_eee);

	/* First store the requested & validated EEE configuration */
	pdata->eee_enabled = eee_req->eee_enabled;
	pdata->tx_lpi_enabled = eee_req->tx_lpi_enabled;
	pdata->tx_lpi_timer = eee_req->tx_lpi_timer;
	pdata->eee_active = eee_req->eee_active;

	/* If EEE adv has changed, inform PHY framework. PHY will
	 * restart ANEG and the ether_adjust_link callback will take care of
	 * enabling Tx LPI as needed.
	 */
	if (cur_eee.advertised != eee_req->advertised) {
		return phy_ethtool_set_eee(phydev, eee_req);
	}

	/* If no advertisement change, and only local Tx LPI changed, then
	 * configure the MAC right away.
	 */
	if (cur_eee.tx_lpi_enabled != eee_req->tx_lpi_enabled) {
		eee_req->eee_active = ether_conf_eee(pdata,
						     eee_req->tx_lpi_enabled);
		pdata->eee_active = eee_req->eee_active;
	}

	return 0;
}

/**
 * @brief This function is invoked by kernel when user request to set
 * pmt parameters for remote wakeup or magic wakeup
 *
 * Algorithm: Enable or Disable Wake On Lan status based on wol param
 *
 * @param[in] ndev – pointer to net device structure.
 * @param[in] wol – pointer to ethtool_wolinfo structure.
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval zero on success and -ve number on failure.
 */
static int ether_set_wol(struct net_device *ndev, struct ethtool_wolinfo *wol)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	int ret;

	if (!wol)
		return -EINVAL;

	if (!pdata->phydev) {
		netdev_err(pdata->ndev,
			   "%s: phydev is null check iface up status\n",
			   __func__);
		return -ENOTSUPP;
	}

	if (!phy_interrupt_is_valid(pdata->phydev))
		return -ENOTSUPP;

	ret = phy_ethtool_set_wol(pdata->phydev, wol);
	if (ret < 0)
		return ret;

	if (wol->wolopts) {
		ret = enable_irq_wake(pdata->phydev->irq);
		if (ret) {
			dev_err(pdata->dev, "PHY enable irq wake failed, %d\n",
				ret);
			return ret;
		}
		/* enable device wake on WoL set */
		device_init_wakeup(&ndev->dev, true);
	} else {
		ret = disable_irq_wake(pdata->phydev->irq);
		if (ret) {
			dev_info(pdata->dev,
				 "PHY disable irq wake failed, %d\n",
				 ret);
		}
		/* disable device wake on WoL reset */
		device_init_wakeup(&ndev->dev, false);
	}

	return ret;
}

/**
 * @brief This function is invoked by kernel when user request to get report
 * whether wake-on-lan is enable or not.
 *
 * Algorithm: Return Wake On Lan status in wol param
 *
 * param[in] ndev – pointer to net device structure.
 * param[in] wol – pointer to ethtool_wolinfo structure.
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval none
 */

static void ether_get_wol(struct net_device *ndev, struct ethtool_wolinfo *wol)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);

	wol->supported = 0;
	wol->wolopts = 0;

	if (!wol)
		return;

	if (!pdata->phydev) {
		netdev_err(pdata->ndev,
			   "%s: phydev is null check iface up status\n",
			   __func__);
		return;
	}

	if (!phy_interrupt_is_valid(pdata->phydev))
		return;

	phy_ethtool_get_wol(pdata->phydev, wol);
}

/**
 * @brief Set of ethtool operations
 */
static const struct ethtool_ops ether_ethtool_ops = {
	.get_link = ethtool_op_get_link,
	.get_link_ksettings = phy_ethtool_get_link_ksettings,
	.set_link_ksettings = phy_ethtool_set_link_ksettings,
	.get_pauseparam = ether_get_pauseparam,
	.set_pauseparam = ether_set_pauseparam,
	.get_ts_info = ether_get_ts_info,
	.get_strings = ether_get_strings,
	.get_ethtool_stats = ether_get_ethtool_stats,
	.get_sset_count = ether_get_sset_count,
	.get_coalesce = ether_get_coalesce,
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
	.supported_coalesce_params = (ETHTOOL_COALESCE_USECS |
		ETHTOOL_COALESCE_MAX_FRAMES),
#endif
	.set_coalesce = ether_set_coalesce,
	.get_wol = ether_get_wol,
	.set_wol = ether_set_wol,
	.get_eee = ether_get_eee,
	.set_eee = ether_set_eee,
	.self_test = ether_selftest_run,
};

void ether_set_ethtool_ops(struct net_device *ndev)
{
	ndev->ethtool_ops = &ether_ethtool_ops;
}
