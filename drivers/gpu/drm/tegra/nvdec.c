// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, NVIDIA Corporation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/host1x-next.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <soc/tegra/pmc.h>

#include "drm.h"
#include "falcon.h"
#include "vic.h"

struct nvdec_config {
	const char *firmware;
	unsigned int version;
	bool supports_sid;
};

struct nvdec {
	struct falcon falcon;

	void __iomem *regs;
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct device *dev;
	struct clk *clk;
	struct reset_control *rst;

	/* Platform configuration */
	const struct nvdec_config *config;
};

static inline struct nvdec *to_vic(struct tegra_drm_client *client)
{
	return container_of(client, struct nvdec, client);
}

static void nvdec_writel(struct nvdec *nvdec, u32 value, unsigned int offset)
{
	writel(value, nvdec->regs + offset);
}

static int nvdec_boot(struct nvdec *nvdec)
{
#ifdef CONFIG_IOMMU_API
	struct iommu_fwspec *spec = dev_iommu_fwspec_get(nvdec->dev);
#endif
	int err = 0;

#ifdef CONFIG_IOMMU_API
	if (nvdec->config->supports_sid && spec) {
		u32 value;

		value = TRANSCFG_ATT(1, TRANSCFG_SID_FALCON) |
			TRANSCFG_ATT(0, TRANSCFG_SID_HW);
		nvdec_writel(nvdec, value, VIC_TFBIF_TRANSCFG);

		if (spec->num_ids > 0) {
			value = spec->ids[0] & 0xffff;

			nvdec_writel(nvdec, value, VIC_THI_STREAMID0);
			nvdec_writel(nvdec, value, VIC_THI_STREAMID1);
		}
	}
#endif

	err = falcon_boot(&nvdec->falcon);
	if (err < 0)
		return err;

	err = falcon_wait_idle(&nvdec->falcon);
	if (err < 0) {
		dev_err(nvdec->dev,
			"failed to set application ID and FCE base\n");
		return err;
	}

	return 0;
}

static int nvdec_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->host);
	struct tegra_drm *tegra = dev->dev_private;
	struct nvdec *nvdec = to_vic(drm);
	int err;

	err = host1x_client_iommu_attach(client);
	if (err < 0 && err != -ENODEV) {
		dev_err(nvdec->dev, "failed to attach to domain: %d\n", err);
		return err;
	}

	nvdec->channel = host1x_channel_request(client);
	if (!nvdec->channel) {
		err = -ENOMEM;
		goto detach;
	}

	client->syncpts[0] = host1x_syncpt_request(client, 0);
	if (!client->syncpts[0]) {
		err = -ENOMEM;
		goto free_channel;
	}

	err = tegra_drm_register_client(tegra, drm);
	if (err < 0)
		goto free_syncpt;

	/*
	 * Inherit the DMA parameters (such as maximum segment size) from the
	 * parent host1x device.
	 */
	client->dev->dma_parms = client->host->dma_parms;

	return 0;

free_syncpt:
	host1x_syncpt_put(client->syncpts[0]);
free_channel:
	host1x_channel_put(nvdec->channel);
detach:
	host1x_client_iommu_detach(client);

	return err;
}

static int nvdec_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->host);
	struct tegra_drm *tegra = dev->dev_private;
	struct nvdec *nvdec = to_vic(drm);
	int err;

	/* avoid a dangling pointer just in case this disappears */
	client->dev->dma_parms = NULL;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	host1x_syncpt_put(client->syncpts[0]);
	host1x_channel_put(nvdec->channel);
	host1x_client_iommu_detach(client);

	if (client->group) {
		dma_unmap_single(nvdec->dev, nvdec->falcon.firmware.phys,
				 nvdec->falcon.firmware.size, DMA_TO_DEVICE);
		tegra_drm_free(tegra, nvdec->falcon.firmware.size,
			       nvdec->falcon.firmware.virt,
			       nvdec->falcon.firmware.iova);
	} else {
		dma_free_coherent(nvdec->dev, nvdec->falcon.firmware.size,
				  nvdec->falcon.firmware.virt,
				  nvdec->falcon.firmware.iova);
	}

	return 0;
}

static const struct host1x_client_ops nvdec_client_ops = {
	.init = nvdec_init,
	.exit = nvdec_exit,
};

static int nvdec_load_firmware(struct nvdec *nvdec)
{
	struct host1x_client *client = &nvdec->client.base;
	struct tegra_drm *tegra = nvdec->client.drm;
	dma_addr_t iova;
	size_t size;
	void *virt;
	int err;

	if (nvdec->falcon.firmware.virt)
		return 0;

	err = falcon_read_firmware(&nvdec->falcon, nvdec->config->firmware);
	if (err < 0)
		return err;

	size = nvdec->falcon.firmware.size;

	if (!client->group) {
		virt = dma_alloc_coherent(nvdec->dev, size, &iova, GFP_KERNEL);

		err = dma_mapping_error(nvdec->dev, iova);
		if (err < 0)
			return err;
	} else {
		virt = tegra_drm_alloc(tegra, size, &iova);
	}

	nvdec->falcon.firmware.virt = virt;
	nvdec->falcon.firmware.iova = iova;

	err = falcon_load_firmware(&nvdec->falcon);
	if (err < 0)
		goto cleanup;

	/*
	 * In this case we have received an IOVA from the shared domain, so we
	 * need to make sure to get the physical address so that the DMA API
	 * knows what memory pages to flush the cache for.
	 */
	if (client->group) {
		dma_addr_t phys;

		phys = dma_map_single(nvdec->dev, virt, size, DMA_TO_DEVICE);

		err = dma_mapping_error(nvdec->dev, phys);
		if (err < 0)
			goto cleanup;

		nvdec->falcon.firmware.phys = phys;
	}

	return 0;

cleanup:
	if (!client->group)
		dma_free_coherent(nvdec->dev, size, virt, iova);
	else
		tegra_drm_free(tegra, size, virt, iova);

	return err;
}


static int nvdec_runtime_resume(struct device *dev)
{
	struct nvdec *nvdec = dev_get_drvdata(dev);
	int err;

	err = clk_prepare_enable(nvdec->clk);
	if (err < 0)
		return err;

	usleep_range(10, 20);

	err = reset_control_deassert(nvdec->rst);
	if (err < 0)
		goto disable;

	usleep_range(10, 20);

	err = nvdec_load_firmware(nvdec);
	if (err < 0)
		goto assert;

	err = nvdec_boot(nvdec);
	if (err < 0)
		goto assert;

	return 0;

assert:
	reset_control_assert(nvdec->rst);
disable:
	clk_disable_unprepare(nvdec->clk);
	return err;
}

static int nvdec_runtime_suspend(struct device *dev)
{
	struct nvdec *nvdec = dev_get_drvdata(dev);
	int err;

	err = reset_control_assert(nvdec->rst);
	if (err < 0)
		return err;

	usleep_range(2000, 4000);

	clk_disable_unprepare(nvdec->clk);

	return 0;
}

static int nvdec_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context)
{
	struct nvdec *nvdec = to_vic(client);
	int err;

	err = pm_runtime_get_sync(nvdec->dev);
	if (err < 0) {
		pm_runtime_put(nvdec->dev);
		return err;
	}

	context->channel = host1x_channel_get(nvdec->channel);
	if (!context->channel) {
		pm_runtime_put(nvdec->dev);
		return -ENOMEM;
	}

	return 0;
}

static void nvdec_close_channel(struct tegra_drm_context *context)
{
	struct nvdec *nvdec = to_vic(context->client);

	host1x_channel_put(context->channel);
	pm_runtime_put(nvdec->dev);
}

static const struct tegra_drm_client_ops nvdec_ops = {
	.open_channel = nvdec_open_channel,
	.close_channel = nvdec_close_channel,
	.submit = tegra_drm_submit,
};

#define NVIDIA_TEGRA_210_NVDEC_FIRMWARE "nvidia/tegra210/nvdec.bin"

static const struct nvdec_config nvdec_t210_config = {
	.firmware = NVIDIA_TEGRA_210_NVDEC_FIRMWARE,
	.version = 0x21,
	.supports_sid = false,
};

#define NVIDIA_TEGRA_186_NVDEC_FIRMWARE "nvidia/tegra186/nvdec.bin"

static const struct nvdec_config nvdec_t186_config = {
	.firmware = NVIDIA_TEGRA_186_NVDEC_FIRMWARE,
	.version = 0x18,
	.supports_sid = true,
};

#define NVIDIA_TEGRA_194_NVDEC_FIRMWARE "nvidia/tegra194/nvdec.bin"

static const struct nvdec_config nvdec_t194_config = {
	.firmware = NVIDIA_TEGRA_194_NVDEC_FIRMWARE,
	.version = 0x19,
	.supports_sid = true,
};

static const struct of_device_id tegra_nvdec_of_match[] = {
	{ .compatible = "nvidia,tegra210-nvdec", .data = &nvdec_t210_config },
	{ .compatible = "nvidia,tegra186-nvdec", .data = &nvdec_t186_config },
	{ .compatible = "nvidia,tegra194-nvdec", .data = &nvdec_t194_config },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_nvdec_of_match);

static int nvdec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	struct nvdec *nvdec;
	int err;

	/* inherit DMA mask from host1x parent */
	err = dma_coerce_mask_and_coherent(dev, *dev->parent->dma_mask);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to set DMA mask: %d\n", err);
		return err;
	}

	nvdec = devm_kzalloc(dev, sizeof(*nvdec), GFP_KERNEL);
	if (!nvdec)
		return -ENOMEM;

	nvdec->config = of_device_get_match_data(dev);

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "failed to get registers\n");
		return -ENXIO;
	}

	nvdec->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(nvdec->regs))
		return PTR_ERR(nvdec->regs);

	nvdec->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(nvdec->clk)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		return PTR_ERR(nvdec->clk);
	}

	if (!dev->pm_domain) {
		nvdec->rst = devm_reset_control_get(dev, "nvdec");
		if (IS_ERR(nvdec->rst)) {
			dev_err(&pdev->dev, "failed to get reset\n");
			return PTR_ERR(nvdec->rst);
		}
	}

	nvdec->falcon.dev = dev;
	nvdec->falcon.regs = nvdec->regs;

	err = falcon_init(&nvdec->falcon);
	if (err < 0)
		return err;

	platform_set_drvdata(pdev, nvdec);

	INIT_LIST_HEAD(&nvdec->client.base.list);
	nvdec->client.base.ops = &nvdec_client_ops;
	nvdec->client.base.dev = dev;
	{
		u32 interface;
		err = of_property_read_u32(dev->of_node, "nvidia,interface", &interface);
		nvdec->client.base.class = interface ? HOST1X_CLASS_NVDEC1 : HOST1X_CLASS_NVDEC;
	}
	nvdec->client.base.syncpts = syncpts;
	nvdec->client.base.num_syncpts = 1;
	nvdec->dev = dev;

	INIT_LIST_HEAD(&nvdec->client.list);
	nvdec->client.version = nvdec->config->version;
	nvdec->client.ops = &nvdec_ops;

	err = host1x_client_register(&nvdec->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		goto exit_falcon;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		err = nvdec_runtime_resume(&pdev->dev);
		if (err < 0)
			goto unregister_client;
	}

	return 0;

unregister_client:
	host1x_client_unregister(&nvdec->client.base);
exit_falcon:
	falcon_exit(&nvdec->falcon);

	return err;
}

static int nvdec_remove(struct platform_device *pdev)
{
	struct nvdec *nvdec = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&nvdec->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	else
		nvdec_runtime_suspend(&pdev->dev);

	falcon_exit(&nvdec->falcon);

	return 0;
}

static const struct dev_pm_ops nvdec_pm_ops = {
	SET_RUNTIME_PM_OPS(nvdec_runtime_suspend, nvdec_runtime_resume, NULL)
};

struct platform_driver tegra_nvdec_driver = {
	.driver = {
		.name = "tegra-nvdec",
		.of_match_table = tegra_nvdec_of_match,
		.pm = &nvdec_pm_ops
	},
	.probe = nvdec_probe,
	.remove = nvdec_remove,
};

#if IS_ENABLED(CONFIG_ARCH_TEGRA_210_SOC)
MODULE_FIRMWARE(NVIDIA_TEGRA_210_NVDEC_FIRMWARE);
#endif
#if IS_ENABLED(CONFIG_ARCH_TEGRA_186_SOC)
MODULE_FIRMWARE(NVIDIA_TEGRA_186_NVDEC_FIRMWARE);
#endif
#if IS_ENABLED(CONFIG_ARCH_TEGRA_194_SOC)
MODULE_FIRMWARE(NVIDIA_TEGRA_194_NVDEC_FIRMWARE);
#endif
