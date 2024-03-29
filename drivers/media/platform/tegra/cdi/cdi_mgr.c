/*
 * cdi manager.
 *
 * Copyright (c) 2015-2021, NVIDIA Corporation. All Rights Reserved.
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

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <linux/sched/signal.h>
#endif
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/pwm.h>
#include <linux/debugfs.h>
#include <linux/nospec.h>
#include <linux/seq_file.h>
#include <media/cdi-dev.h>
#include <media/cdi-mgr.h>

#include <asm/barrier.h>

#include "cdi-mgr-priv.h"

#define PW_ON(flag)	((flag) ? 0 : 1)
#define PW_OFF(flag)	((flag) ? 1 : 0)

/* minor number range would be 0 to 127 */
#define CDI_DEV_MAX	128

/* CDI Dev Debugfs functions
 *
 *    - cdi_mgr_debugfs_init
 *    - cdi_mgr_debugfs_remove
 *    - cdi_mgr_status_show
 *    - cdi_mgr_attr_set
 *    - pwr_on_get
 *    - pwr_on_set
 *    - pwr_off_get
 *    - pwr_off_set
 */
static int cdi_mgr_status_show(struct seq_file *s, void *data)
{
	struct cdi_mgr_priv *cdi_mgr = s->private;
	struct cdi_mgr_client *cdi_dev;

	if (cdi_mgr == NULL)
		return 0;
	pr_info("%s - %s\n", __func__, cdi_mgr->devname);

	if (list_empty(&cdi_mgr->dev_list)) {
		seq_printf(s, "%s: No devices supported.\n", cdi_mgr->devname);
		return 0;
	}

	mutex_lock(&cdi_mgr->mutex);
	list_for_each_entry_reverse(cdi_dev, &cdi_mgr->dev_list, list) {
		seq_printf(s, "    %02d  --  @0x%02x, %02d, %d, %s\n",
			cdi_dev->id,
			cdi_dev->cfg.addr,
			cdi_dev->cfg.reg_bits,
			cdi_dev->cfg.val_bits,
			cdi_dev->cfg.drv_name
			);
	}
	mutex_unlock(&cdi_mgr->mutex);

	return 0;
}

static ssize_t cdi_mgr_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	return count;
}

static int cdi_mgr_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, cdi_mgr_status_show, inode->i_private);
}

static const struct file_operations cdi_mgr_debugfs_fops = {
	.open = cdi_mgr_debugfs_open,
	.read = seq_read,
	.write = cdi_mgr_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pwr_on_get(void *data, u64 *val)
{
	struct cdi_mgr_priv *cdi_mgr = data;

	if (cdi_mgr->pdata == NULL || !cdi_mgr->pdata->num_pwr_gpios) {
		*val = 0ULL;
		return 0;
	}

	*val = (cdi_mgr->pwr_state & (BIT(28) - 1)) |
		((cdi_mgr->pdata->num_pwr_gpios & 0x0f) << 28);
	return 0;
}

static int pwr_on_set(void *data, u64 val)
{
	return cdi_mgr_power_up((struct cdi_mgr_priv *)data, val);
}

DEFINE_SIMPLE_ATTRIBUTE(pwr_on_fops, pwr_on_get, pwr_on_set, "0x%02llx\n");

static int pwr_off_get(void *data, u64 *val)
{
	struct cdi_mgr_priv *cdi_mgr = data;

	if (cdi_mgr->pdata == NULL || !cdi_mgr->pdata->num_pwr_gpios) {
		*val = 0ULL;
		return 0;
	}

	*val = (~cdi_mgr->pwr_state) & (BIT(cdi_mgr->pdata->num_pwr_gpios) - 1);
	*val = (*val & (BIT(28) - 1)) |
		((cdi_mgr->pdata->num_pwr_gpios & 0x0f) << 28);
	return 0;
}

static int pwr_off_set(void *data, u64 val)
{
	return cdi_mgr_power_down((struct cdi_mgr_priv *)data, val);
}

DEFINE_SIMPLE_ATTRIBUTE(pwr_off_fops, pwr_off_get, pwr_off_set, "0x%02llx\n");

int cdi_mgr_debugfs_init(struct cdi_mgr_priv *cdi_mgr)
{
	struct dentry *d;

	dev_dbg(cdi_mgr->dev, "%s %s\n", __func__, cdi_mgr->devname);
	cdi_mgr->d_entry = debugfs_create_dir(
		cdi_mgr->devname, NULL);
	if (cdi_mgr->d_entry == NULL) {
		dev_err(cdi_mgr->dev, "%s: create dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("map", 0644, cdi_mgr->d_entry,
		(void *)cdi_mgr, &cdi_mgr_debugfs_fops);
	if (!d)
		goto debugfs_init_err;

	d = debugfs_create_file("pwr-on", 0644, cdi_mgr->d_entry,
		(void *)cdi_mgr, &pwr_on_fops);
	if (!d)
		goto debugfs_init_err;

	d = debugfs_create_file("pwr-off", 0644, cdi_mgr->d_entry,
		(void *)cdi_mgr, &pwr_off_fops);
	if (!d)
		goto debugfs_init_err;

	return 0;

debugfs_init_err:
	dev_err(cdi_mgr->dev, "%s: create file failed\n", __func__);
	debugfs_remove_recursive(cdi_mgr->d_entry);
	cdi_mgr->d_entry = NULL;
	return -ENOMEM;
}

int cdi_mgr_debugfs_remove(struct cdi_mgr_priv *cdi_mgr)
{
	if (cdi_mgr->d_entry == NULL)
		return 0;
	debugfs_remove_recursive(cdi_mgr->d_entry);
	cdi_mgr->d_entry = NULL;
	return 0;
}

static irqreturn_t cdi_mgr_isr(int irq, void *data)
{
	struct cdi_mgr_priv *cdi_mgr;
	int ret;
	unsigned long flags;

	if (data) {
		cdi_mgr = (struct cdi_mgr_priv *)data;
		cdi_mgr->err_irq_recvd = true;
		wake_up_interruptible(&cdi_mgr->err_queue);
		spin_lock_irqsave(&cdi_mgr->spinlock, flags);
		if (cdi_mgr->sinfo.si_signo && cdi_mgr->t) {
			/* send the signal to user space */
			ret = send_sig_info(cdi_mgr->sinfo.si_signo,
					&cdi_mgr->sinfo,
					cdi_mgr->t);
			if (ret < 0) {
				pr_err("error sending signal\n");
				spin_unlock_irqrestore(&cdi_mgr->spinlock,
							flags);
				return IRQ_HANDLED;
			}
		}
		spin_unlock_irqrestore(&cdi_mgr->spinlock, flags);
	}

	return IRQ_HANDLED;
}

int cdi_delete_lst(struct device *dev, struct i2c_client *client)
{
	struct cdi_mgr_priv *cdi_mgr;
	struct cdi_mgr_client *cdi_dev;

	if (dev == NULL)
		return -EFAULT;

	cdi_mgr = (struct cdi_mgr_priv *)dev_get_drvdata(dev);

	mutex_lock(&cdi_mgr->mutex);
	list_for_each_entry(cdi_dev, &cdi_mgr->dev_list, list) {
		if (cdi_dev->client == client) {
			list_del(&cdi_dev->list);
			break;
		}
	}
	mutex_unlock(&cdi_mgr->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(cdi_delete_lst);

static int cdi_remove_dev(struct cdi_mgr_priv *cdi_mgr, unsigned long arg)
{
	struct cdi_mgr_client *cdi_dev;

	dev_dbg(cdi_mgr->dev, "%s %ld\n", __func__, arg);
	mutex_lock(&cdi_mgr->mutex);
	list_for_each_entry(cdi_dev, &cdi_mgr->dev_list, list) {
		if (cdi_dev->id == arg) {
			list_del(&cdi_dev->list);
			break;
		}
	}
	mutex_unlock(&cdi_mgr->mutex);

	if (&cdi_dev->list != &cdi_mgr->dev_list)
		i2c_unregister_device(cdi_dev->client);
	else
		dev_err(cdi_mgr->dev, "%s: list %lx un-exist\n", __func__, arg);

	return 0;
}

static int __cdi_create_dev(
	struct cdi_mgr_priv *cdi_mgr, struct cdi_mgr_new_dev *new_dev)
{
	struct cdi_mgr_client *cdi_dev;
	struct i2c_board_info brd;
	int err = 0;

	if (new_dev->addr >= 0x80 || new_dev->drv_name[0] == '\0' ||
		(new_dev->val_bits != 8 && new_dev->val_bits != 16) ||
		(new_dev->reg_bits != 0 && new_dev->reg_bits != 8 &&
		new_dev->reg_bits != 16)) {
		dev_err(cdi_mgr->dev,
			"%s: invalid cdi dev params: %s %x %d %d\n",
			__func__, new_dev->drv_name, new_dev->addr,
			new_dev->reg_bits, new_dev->val_bits);
		return -EINVAL;
	}

	cdi_dev = devm_kzalloc(cdi_mgr->dev, sizeof(*cdi_dev), GFP_KERNEL);
	if (!cdi_dev) {
		dev_err(cdi_mgr->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}

	memcpy(&cdi_dev->cfg, new_dev, sizeof(cdi_dev->cfg));
	dev_dbg(cdi_mgr->pdev, "%s - %s @ %x, %d %d\n", __func__,
		cdi_dev->cfg.drv_name, cdi_dev->cfg.addr,
		cdi_dev->cfg.reg_bits, cdi_dev->cfg.val_bits);

	snprintf(cdi_dev->pdata.drv_name, sizeof(cdi_dev->pdata.drv_name),
			"%s.%u.%02x", cdi_dev->cfg.drv_name,
			cdi_mgr->adap->nr, cdi_dev->cfg.addr);
	cdi_dev->pdata.reg_bits = cdi_dev->cfg.reg_bits;
	cdi_dev->pdata.val_bits = cdi_dev->cfg.val_bits;
	cdi_dev->pdata.pdev = cdi_mgr->dev;

	mutex_init(&cdi_dev->mutex);
	INIT_LIST_HEAD(&cdi_dev->list);

	memset(&brd, 0, sizeof(brd));
	strncpy(brd.type, "cdi-dev", sizeof(brd.type));
	brd.addr = cdi_dev->cfg.addr;
	brd.platform_data = &cdi_dev->pdata;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	cdi_dev->client = i2c_new_device(cdi_mgr->adap, &brd);
#else
	 cdi_dev->client = i2c_new_client_device(cdi_mgr->adap, &brd);
#endif
	if (!cdi_dev->client) {
		dev_err(cdi_mgr->dev,
			"%s cannot allocate client: %s bus %d, %x\n", __func__,
			cdi_dev->pdata.drv_name, cdi_mgr->adap->nr, brd.addr);
		err = -EINVAL;
		goto dev_create_err;
	}

	mutex_lock(&cdi_mgr->mutex);
	if (!list_empty(&cdi_mgr->dev_list))
		cdi_dev->id = list_entry(cdi_mgr->dev_list.next,
			struct cdi_mgr_client, list)->id + 1;
	list_add(&cdi_dev->list, &cdi_mgr->dev_list);
	mutex_unlock(&cdi_mgr->mutex);

dev_create_err:
	if (err) {
		devm_kfree(cdi_mgr->dev, cdi_dev);
		return err;
	} else
		return cdi_dev->id;
}

static int cdi_create_dev(struct cdi_mgr_priv *cdi_mgr, const void __user *arg)
{
	struct cdi_mgr_new_dev d_cfg;

	if (copy_from_user(&d_cfg, arg, sizeof(d_cfg))) {
		dev_err(cdi_mgr->pdev,
			"%s: failed to copy from user\n", __func__);
		return -EFAULT;
	}

	return __cdi_create_dev(cdi_mgr, &d_cfg);
}

static int cdi_mgr_write_pid(struct file *file, const void __user *arg)
{
	struct cdi_mgr_priv *cdi_mgr = file->private_data;
	struct cdi_mgr_sinfo sinfo;
	unsigned long flags;

	if (copy_from_user(&sinfo, arg, sizeof(sinfo))) {
		dev_err(cdi_mgr->pdev,
			"%s: failed to copy from user\n", __func__);
		return -EFAULT;
	}

	if (cdi_mgr->sinfo.si_int) {
		dev_err(cdi_mgr->pdev, "exist signal info\n");
		return -EINVAL;
	}

	if ((sinfo.sig_no < SIGRTMIN) || (sinfo.sig_no > SIGRTMAX)) {
		dev_err(cdi_mgr->pdev, "Invalid signal number\n");
		return -EINVAL;
	}

	if (!sinfo.pid) {
		dev_err(cdi_mgr->pdev, "Invalid PID\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&cdi_mgr->spinlock, flags);
	cdi_mgr->sinfo.si_signo = cdi_mgr->sig_no = sinfo.sig_no;
	cdi_mgr->sinfo.si_code = SI_QUEUE;
	cdi_mgr->sinfo.si_ptr = (void __user *)((unsigned long)sinfo.context);
	spin_unlock_irqrestore(&cdi_mgr->spinlock, flags);

	rcu_read_lock();
	cdi_mgr->t = pid_task(find_pid_ns(sinfo.pid, &init_pid_ns),
				PIDTYPE_PID);
	if (cdi_mgr->t == NULL) {
		dev_err(cdi_mgr->pdev, "no such pid\n");
		rcu_read_unlock();
		return -ENODEV;
	}
	rcu_read_unlock();

	return 0;
}

static int cdi_mgr_get_pwr_info(struct cdi_mgr_priv *cdi_mgr,
				void __user *arg)
{
	struct cdi_mgr_platform_data *pd = cdi_mgr->pdata;
	struct cdi_mgr_pwr_info pinfo;
	int err;

	if (copy_from_user(&pinfo, arg, sizeof(pinfo))) {
		dev_err(cdi_mgr->pdev,
			"%s: failed to copy from user\n", __func__);
		return -EFAULT;
	}

	if (!pd->num_pwr_gpios) {
		dev_err(cdi_mgr->pdev,
			"%s: no power gpios\n", __func__);
		pinfo.pwr_status = -1;
		err = -ENODEV;
		goto pwr_info_end;
	}

	if (pinfo.pwr_gpio >= pd->num_pwr_gpios || pinfo.pwr_gpio < 0) {
		dev_err(cdi_mgr->pdev,
			"%s: invalid power gpio provided\n", __func__);
		pinfo.pwr_status = -1;
		err = -EINVAL;
		goto pwr_info_end;
	}

	pinfo.pwr_gpio = array_index_nospec(pinfo.pwr_gpio, pd->num_pwr_gpios);

	pinfo.pwr_status  = gpio_get_value(pd->pwr_gpios[pinfo.pwr_gpio]);
	err = 0;

pwr_info_end:
	if (copy_to_user(arg, &pinfo, sizeof(pinfo))) {
		dev_err(cdi_mgr->pdev,
			"%s: failed to copy to user\n", __func__);
		return -EFAULT;
	}
	return err;
}

int cdi_mgr_power_up(struct cdi_mgr_priv *cdi_mgr, unsigned long arg)
{
	struct cdi_mgr_platform_data *pd = cdi_mgr->pdata;
	int i;
	u32 pwr_gpio;

	dev_dbg(cdi_mgr->pdev, "%s - %lu\n", __func__, arg);

	if (!pd->num_pwr_gpios)
		goto pwr_up_end;

	if (arg >= MAX_CDI_GPIOS)
		arg = MAX_CDI_GPIOS - 1;

	arg = array_index_nospec(arg, MAX_CDI_GPIOS);
	pwr_gpio = pd->pwr_mapping[arg];

	if (pwr_gpio < pd->num_pwr_gpios) {
		pwr_gpio = array_index_nospec(pwr_gpio, pd->num_pwr_gpios);
		gpio_set_value(pd->pwr_gpios[pwr_gpio],
			PW_ON(pd->pwr_flags[pwr_gpio]));
		cdi_mgr->pwr_state |= BIT(pwr_gpio);
		return 0;
	}

	for (i = 0; i < pd->num_pwr_gpios; i++) {
		dev_dbg(cdi_mgr->pdev, "  - %d, %d\n",
			pd->pwr_gpios[i], PW_ON(pd->pwr_flags[i]));
		gpio_set_value(pd->pwr_gpios[i], PW_ON(pd->pwr_flags[i]));
		cdi_mgr->pwr_state |= BIT(i);
	}

pwr_up_end:
	return 0;
}

int cdi_mgr_power_down(struct cdi_mgr_priv *cdi_mgr, unsigned long arg)
{
	struct cdi_mgr_platform_data *pd = cdi_mgr->pdata;
	int i;
	u32 pwr_gpio;

	dev_dbg(cdi_mgr->pdev, "%s - %lx\n", __func__, arg);

	if (!pd->num_pwr_gpios)
		goto pwr_dn_end;

	if (arg >= MAX_CDI_GPIOS)
		arg = MAX_CDI_GPIOS - 1;

	arg = array_index_nospec(arg, MAX_CDI_GPIOS);

	pwr_gpio = pd->pwr_mapping[arg];

	if (pwr_gpio < pd->num_pwr_gpios) {
		pwr_gpio = array_index_nospec(pwr_gpio, pd->num_pwr_gpios);
		gpio_set_value(pd->pwr_gpios[pwr_gpio],
				PW_OFF(pd->pwr_flags[pwr_gpio]));
		cdi_mgr->pwr_state &= ~BIT(pwr_gpio);
		return 0;
	}

	for (i = 0; i < pd->num_pwr_gpios; i++) {
		dev_dbg(cdi_mgr->pdev, "  - %d, %d\n",
			pd->pwr_gpios[i], PW_OFF(pd->pwr_flags[i]));
		gpio_set_value(pd->pwr_gpios[i], PW_OFF(pd->pwr_flags[i]));
		cdi_mgr->pwr_state &= ~BIT(i);
	}
	mdelay(7);

pwr_dn_end:
	return 0;
}

static int cdi_mgr_mcdi_ctrl(struct cdi_mgr_priv *cdi_mgr, bool mcdi_on)
{
	struct cdi_mgr_platform_data *pd = cdi_mgr->pdata;
	int err, i;

	dev_dbg(cdi_mgr->pdev, "%s - %s\n", __func__, mcdi_on ? "ON" : "OFF");

	if (!pd->num_mcdi_gpios)
		return 0;

	for (i = 0; i < pd->num_mcdi_gpios; i++) {
		if (mcdi_on) {
			if (devm_gpio_request(cdi_mgr->pdev,
					      pd->mcdi_gpios[i],
					      "mcdi-gpio")) {
				dev_err(cdi_mgr->pdev, "failed req GPIO: %d\n",
					pd->mcdi_gpios[i]);
				goto mcdi_ctrl_err;
			}

			err = gpio_direction_output(
				pd->mcdi_gpios[i], PW_ON(pd->mcdi_flags[i]));
		} else {
			err = gpio_direction_output(
				pd->mcdi_gpios[i], PW_OFF(pd->mcdi_flags[i]));
			devm_gpio_free(cdi_mgr->pdev, pd->mcdi_gpios[i]);
		}
	}
	return 0;

mcdi_ctrl_err:
	for (; i >= 0; i--)
		devm_gpio_free(cdi_mgr->pdev, pd->mcdi_gpios[i]);
	return -EBUSY;
}

static int cdi_mgr_pwm_enable(
	struct cdi_mgr_priv *cdi_mgr, unsigned long arg)
{
	int err = 0;

	if (!cdi_mgr || !cdi_mgr->pwm)
		return -EINVAL;

	switch (arg) {
	case CDI_MGR_PWM_ENABLE:
		err = pwm_enable(cdi_mgr->pwm);
		break;
	case CDI_MGR_PWM_DISABLE:
		pwm_disable(cdi_mgr->pwm);
		break;
	default:
		dev_err(cdi_mgr->pdev, "%s unrecognized command: %lx\n",
			__func__, arg);
	}

	return err;
}

static int cdi_mgr_pwm_config(
	struct cdi_mgr_priv *cdi_mgr, const void __user *arg)
{
	struct cdi_mgr_pwm_info pwm_cfg;
	int err = 0;

	if (!cdi_mgr || !cdi_mgr->pwm)
		return -EINVAL;

	if (copy_from_user(&pwm_cfg, arg, sizeof(pwm_cfg))) {
		dev_err(cdi_mgr->pdev,
			"%s: failed to copy from user\n", __func__);
		return -EFAULT;
	}

	err = pwm_config(cdi_mgr->pwm, pwm_cfg.duty_ns, pwm_cfg.period_ns);

	return err;
}

static long cdi_mgr_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	struct cdi_mgr_priv *cdi_mgr = file->private_data;
	struct cdi_mgr_platform_data *pd = cdi_mgr->pdata;
	int err = 0;
	unsigned long flags;

	/* command distributor */
	switch (cmd) {
	case CDI_MGR_IOCTL_DEV_ADD:
		err = cdi_create_dev(cdi_mgr, (const void __user *)arg);
		break;
	case CDI_MGR_IOCTL_DEV_DEL:
		cdi_remove_dev(cdi_mgr, arg);
		break;
	case CDI_MGR_IOCTL_PWR_DN:
		err = cdi_mgr_power_down(cdi_mgr, arg);
		break;
	case CDI_MGR_IOCTL_PWR_UP:
		err = cdi_mgr_power_up(cdi_mgr, arg);
		break;
	case CDI_MGR_IOCTL_SET_PID:
		/* first enable irq to clear pending interrupt
		 * and then register PID
		 */
		if (cdi_mgr->err_irq && !atomic_xchg(&cdi_mgr->irq_in_use, 1))
			enable_irq(cdi_mgr->err_irq);

		err = cdi_mgr_write_pid(file, (const void __user *)arg);
		break;
	case CDI_MGR_IOCTL_SIGNAL:
		switch (arg) {
		case CDI_MGR_SIGNAL_RESUME:
			if (!cdi_mgr->sig_no) {
				dev_err(cdi_mgr->pdev,
					"invalid sig_no, setup pid first\n");
				return -EINVAL;
			}
			spin_lock_irqsave(&cdi_mgr->spinlock, flags);
			cdi_mgr->sinfo.si_signo = cdi_mgr->sig_no;
			spin_unlock_irqrestore(&cdi_mgr->spinlock, flags);
			break;
		case CDI_MGR_SIGNAL_SUSPEND:
			spin_lock_irqsave(&cdi_mgr->spinlock, flags);
			cdi_mgr->sinfo.si_signo = 0;
			spin_unlock_irqrestore(&cdi_mgr->spinlock, flags);
			break;
		default:
			dev_err(cdi_mgr->pdev, "%s unrecognized signal: %lx\n",
				__func__, arg);
		}
		break;
	case CDI_MGR_IOCTL_PWR_INFO:
		err = cdi_mgr_get_pwr_info(cdi_mgr, (void __user *)arg);
		break;
	case CDI_MGR_IOCTL_PWM_ENABLE:
		err = cdi_mgr_pwm_enable(cdi_mgr, arg);
		break;
	case CDI_MGR_IOCTL_PWM_CONFIG:
		err = cdi_mgr_pwm_config(cdi_mgr, (const void __user *)arg);
		break;
	case CDI_MGR_IOCTL_WAIT_ERR:
		if (cdi_mgr->err_irq && !atomic_xchg(&cdi_mgr->irq_in_use, 1)) {
			enable_irq(cdi_mgr->err_irq);
			cdi_mgr->err_irq_recvd = false;
		}

		err = wait_event_interruptible(cdi_mgr->err_queue,
			cdi_mgr->err_irq_recvd);
		cdi_mgr->err_irq_recvd = false;
		break;
	case CDI_MGR_IOCTL_ABORT_WAIT_ERR:
		cdi_mgr->err_irq_recvd = true;
		wake_up_interruptible(&cdi_mgr->err_queue);
		break;
	case CDI_MGR_IOCTL_GET_EXT_PWR_CTRL:
		if (copy_to_user((void __user *)arg,
				&pd->ext_pwr_ctrl,
				sizeof(u8))) {
			dev_err(cdi_mgr->pdev, "%s: failed to copy to user\n",
				__func__);
			return -EFAULT;
		}
		break;
	default:
		dev_err(cdi_mgr->pdev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
		err = -EINVAL;
	}

	if (err)
		dev_dbg(cdi_mgr->pdev, "err = %d\n", err);

	return err;
}

static int cdi_mgr_open(struct inode *inode, struct file *file)
{
	struct cdi_mgr_priv *cdi_mgr = container_of(inode->i_cdev,
					struct cdi_mgr_priv, cdev);

	/* only one application can open one cdi_mgr device */
	if (atomic_xchg(&cdi_mgr->in_use, 1))
		return -EBUSY;

	dev_dbg(cdi_mgr->pdev, "%s\n", __func__);
	file->private_data = cdi_mgr;

	/* if runtime_pwrctrl_off is not true, power on all here */
	if (!cdi_mgr->pdata->runtime_pwrctrl_off)
		cdi_mgr_power_up(cdi_mgr, 0xffffffff);

	cdi_mgr_mcdi_ctrl(cdi_mgr, true);
	return 0;
}

static int cdi_mgr_release(struct inode *inode, struct file *file)
{
	struct cdi_mgr_priv *cdi_mgr = file->private_data;

	if (cdi_mgr->pwm)
		if (pwm_is_enabled(cdi_mgr->pwm))
			pwm_disable(cdi_mgr->pwm);

	cdi_mgr_mcdi_ctrl(cdi_mgr, false);

	/* disable irq if irq is in use, when device is closed */
	if (atomic_xchg(&cdi_mgr->irq_in_use, 0)) {
		disable_irq(cdi_mgr->err_irq);
		cdi_mgr->err_irq_recvd = true;
		wake_up_interruptible(&cdi_mgr->err_queue);
	}

	/* if runtime_pwrctrl_off is not true, power off all here */
	if (!cdi_mgr->pdata->runtime_pwrctrl_off)
		cdi_mgr_power_down(cdi_mgr, 0xffffffff);

	/* clear sinfo to prevent report error after handler is closed */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	memset(&cdi_mgr->sinfo, 0, sizeof(struct siginfo));
#else
	memset(&cdi_mgr->sinfo, 0, sizeof(struct kernel_siginfo));
#endif
	cdi_mgr->t = NULL;
	WARN_ON(!atomic_xchg(&cdi_mgr->in_use, 0));

	return 0;
}

static const struct file_operations cdi_mgr_fileops = {
	.owner = THIS_MODULE,
	.open = cdi_mgr_open,
	.unlocked_ioctl = cdi_mgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cdi_mgr_ioctl,
#endif
	.release = cdi_mgr_release,
};

static void cdi_mgr_del(struct cdi_mgr_priv *cdi_mgr)
{
	struct cdi_mgr_platform_data *pd = cdi_mgr->pdata;
	struct cdi_mgr_client *cdi_dev = NULL;
	int i;

	mutex_lock(&cdi_mgr->mutex);
	list_for_each_entry(cdi_dev, &cdi_mgr->dev_list, list) {
		/* remove i2c_clients that cdi-mgr created */
		if (cdi_dev->client != NULL) {
			i2c_unregister_device(cdi_dev->client);
			cdi_dev->client = NULL;
		}
	}
	mutex_unlock(&cdi_mgr->mutex);

	for (i = 0; i < pd->num_pwr_gpios; i++)
		if (pd->pwr_gpios[i])
			gpio_direction_output(
				pd->pwr_gpios[i], PW_OFF(pd->pwr_flags[i]));

	i2c_put_adapter(cdi_mgr->adap);
}

static void cdi_mgr_dev_ins(struct work_struct *work)
{
	struct cdi_mgr_priv *cdi_mgr =
		container_of(work, struct cdi_mgr_priv, ins_work);
	struct device_node *np = cdi_mgr->pdev->of_node;
	struct device_node *subdev;
	struct cdi_mgr_new_dev d_cfg = {.drv_name = "cdi-dev"};
	const char *sname;
	u32 val;
	int err = 0;

	if (np == NULL)
		return;

	dev_dbg(cdi_mgr->dev, "%s - %s\n", __func__, np->full_name);
	sname = of_get_property(np, "cdi-dev", NULL);
	if (sname)
		strncpy(d_cfg.drv_name, sname, sizeof(d_cfg.drv_name) - 8);

	for_each_child_of_node(np, subdev) {
		err = of_property_read_u32(subdev, "addr", &val);
		if (err || !val)
			continue;

		d_cfg.addr = val;
		err = of_property_read_u32(subdev, "reg_len", &val);
		if (err || !val)
			continue;

		d_cfg.reg_bits = val;
		err = of_property_read_u32(subdev, "dat_len", &val);
		if (err || !val)
			continue;

		d_cfg.val_bits = val;

		__cdi_create_dev(cdi_mgr, &d_cfg);
	}
}

static int cdi_mgr_of_get_grp_gpio(
	struct device *dev, struct device_node *np,
	const char *name, int size, u32 *grp, u32 *flags)
{
	int num, i;

	num = of_gpio_named_count(np, name);
	dev_dbg(dev, "    num gpios of %s: %d\n", name, num);
	if (num < 0)
		return 0;

	for (i = 0; (i < num) && (i < size); i++) {
		grp[i] = of_get_named_gpio_flags(np, name, i, &flags[i]);
		if ((int)grp[i] < 0) {
			dev_err(dev, "%s: gpio[%d] invalid\n", __func__, i);
			return -EINVAL;
		}
		dev_dbg(dev, "        [%d] - %d %x\n", i, grp[i], flags[i]);
	}

	return num;
}

static int cdi_mgr_get_pwr_map(
	struct device *dev, struct device_node *np,
	struct cdi_mgr_platform_data *pd)
{
	int num_map_items = 0;
	u32 pwr_map_val;
	unsigned int i;

	for (i = 0; i < MAX_CDI_GPIOS; i++)
		pd->pwr_mapping[i] = i;

	if (!of_get_property(np, "pwr-items", NULL))
		return 0;

	num_map_items = of_property_count_elems_of_size(np,
				"pwr-items", sizeof(u32));
	if (num_map_items < 0) {
		dev_err(dev, "%s: error processing pwr items\n",
			__func__);
		return -1;
	}

	if (num_map_items < pd->num_pwr_gpios) {
		dev_err(dev, "%s: invalid number of pwr items\n",
			__func__);
		return -1;
	}

	for (i = 0; i < num_map_items; i++) {
		if (of_property_read_u32_index(np, "pwr-items",
			i, &pwr_map_val)) {
			dev_err(dev, "%s: failed to get pwr item\n",
				__func__);
			goto pwr_map_err;
		}

		if (pwr_map_val >= pd->num_pwr_gpios) {
			dev_err(dev, "%s: invalid power item index provided\n",
				__func__);
			goto pwr_map_err;
		}
		pd->pwr_mapping[i] = pwr_map_val;
	}

	pd->num_pwr_map = num_map_items;
	return 0;

pwr_map_err:
	for (i = 0; i < MAX_CDI_GPIOS; i++)
		pd->pwr_mapping[i] = i;

	pd->num_pwr_map = pd->num_pwr_gpios;

	return -1;
}

static struct cdi_mgr_platform_data *of_cdi_mgr_pdata(struct platform_device
	*pdev)
{
	struct device_node *np = pdev->dev.of_node, *child_np = NULL;
	struct cdi_mgr_platform_data *pd = NULL;
	int err;
	bool ext_pwr_ctrl_des = false, ext_pwr_ctrl_sensor = false;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		dev_err(&pdev->dev, "%s: allocate memory error\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	child_np = of_get_child_by_name(np, "tegra");
	if (child_np == NULL) {
		dev_err(&pdev->dev, "%s: missing tegra node # DT %s\n",
			__func__, np->full_name);
		return ERR_PTR(-EEXIST);
	}

	pd->drv_name = (void *)of_get_property(np, "drv_name", NULL);
	if (pd->drv_name)
		dev_dbg(&pdev->dev, "    drvname: %s\n", pd->drv_name);

	err = of_property_read_u32(child_np, "i2c-bus", &pd->bus);
	if (err) {
		dev_err(&pdev->dev, "%s: missing i2c bus # DT %s\n",
			__func__, child_np->full_name);
		return ERR_PTR(-EEXIST);
	}
	dev_dbg(&pdev->dev, "    i2c-bus: %d\n", pd->bus);

	err = of_property_read_u32(child_np, "csi-port", &pd->csi_port);
	if (err) {
		dev_err(&pdev->dev, "%s: missing csi port # DT %s\n",
			__func__, child_np->full_name);
		return ERR_PTR(-EEXIST);
	}
	dev_dbg(&pdev->dev, "    csiport: %d\n", pd->csi_port);

	pd->num_pwr_gpios = cdi_mgr_of_get_grp_gpio(
		&pdev->dev, child_np, "pwdn-gpios",
		ARRAY_SIZE(pd->pwr_gpios), pd->pwr_gpios, pd->pwr_flags);
	if (pd->num_pwr_gpios < 0)
		return ERR_PTR(pd->num_pwr_gpios);

	pd->num_mcdi_gpios = cdi_mgr_of_get_grp_gpio(
		&pdev->dev, child_np, "mcdi-gpios",
		ARRAY_SIZE(pd->mcdi_gpios), pd->mcdi_gpios, pd->mcdi_flags);
	if (pd->num_mcdi_gpios < 0)
		return ERR_PTR(pd->num_mcdi_gpios);

	child_np = of_get_child_by_name(np, "pwr_ctrl");
	if (child_np == NULL) {
		dev_err(&pdev->dev, "%s: missing pwr_ctrl node # DT %s\n",
			__func__, np->full_name);
		return ERR_PTR(-EEXIST);
	}

	pd->default_pwr_on = of_property_read_bool(child_np,
		"default-power-on");
	pd->runtime_pwrctrl_off =
		of_property_read_bool(child_np, "runtime-pwrctrl-off");

	pd->ext_pwr_ctrl = 0;
	ext_pwr_ctrl_des =
		of_property_read_bool(child_np, "ext-pwr-ctrl-deserializer");
	if (ext_pwr_ctrl_des == true)
		pd->ext_pwr_ctrl |= 1 << 0;
	ext_pwr_ctrl_sensor = of_property_read_bool(child_np,
		"ext-pwr-ctrl-sensor");
	if (ext_pwr_ctrl_sensor == true)
		pd->ext_pwr_ctrl |= 1 << 1;

	err = cdi_mgr_get_pwr_map(&pdev->dev, child_np, pd);
	if (err)
		dev_err(&pdev->dev,
			"%s: failed to map pwr items. Using default values\n",
			__func__);

	return pd;
}

static char *cdi_mgr_devnode(struct device *dev, umode_t *mode)
{
	if (!mode)
		return NULL;

	/* set alway user to access this device */
	*mode = 0666;

	return NULL;
}

static int cdi_mgr_suspend(struct device *dev)
{
	/* Nothing required for cdi-mgr suspend*/
	return 0;
}

static int cdi_mgr_resume(struct device *dev)
{
	struct pwm_device *pwm;
	/* Reconfigure PWM as done during boot time */
	if (of_property_read_bool(dev->of_node, "pwms")) {
		pwm = devm_pwm_get(dev, NULL);
		if (!IS_ERR(pwm))
			dev_info(dev, "%s Resume successful\n", __func__);
	}
	return 0;
}

static const struct dev_pm_ops cdi_mgr_pm_ops = {
	.suspend = cdi_mgr_suspend,
	.resume = cdi_mgr_resume,
	.runtime_suspend = cdi_mgr_suspend,
	.runtime_resume = cdi_mgr_resume,
};

static int cdi_mgr_probe(struct platform_device *pdev)
{
	int err = 0;
	struct cdi_mgr_priv *cdi_mgr;
	struct cdi_mgr_platform_data *pd;
	unsigned int i;

	dev_info(&pdev->dev, "%sing...\n", __func__);

	cdi_mgr = devm_kzalloc(&pdev->dev,
			sizeof(struct cdi_mgr_priv),
			GFP_KERNEL);
	if (!cdi_mgr) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}

	spin_lock_init(&cdi_mgr->spinlock);
	atomic_set(&cdi_mgr->in_use, 0);
	INIT_LIST_HEAD(&cdi_mgr->dev_list);
	mutex_init(&cdi_mgr->mutex);
	init_waitqueue_head(&cdi_mgr->err_queue);
	cdi_mgr->err_irq_recvd = false;
	cdi_mgr->pwm = NULL;

	if (pdev->dev.of_node) {
		pd = of_cdi_mgr_pdata(pdev);
		if (IS_ERR(pd))
			return PTR_ERR(pd);
		cdi_mgr->pdata = pd;
	} else if (pdev->dev.platform_data) {
		cdi_mgr->pdata = pdev->dev.platform_data;
		pd = cdi_mgr->pdata;
	} else {
		dev_err(&pdev->dev, "%s No platform data.\n", __func__);
		return -EFAULT;
	}

	if (of_property_read_bool(pdev->dev.of_node, "pwms")) {
		cdi_mgr->pwm = devm_pwm_get(&pdev->dev, NULL);
		if (!IS_ERR(cdi_mgr->pwm)) {
			dev_info(&pdev->dev,
				"%s: success to get PWM\n", __func__);
			pwm_disable(cdi_mgr->pwm);
		} else {
			err = PTR_ERR(cdi_mgr->pwm);
			if (err != -EPROBE_DEFER)
				dev_err(&pdev->dev,
					"%s: fail to get PWM\n", __func__);
			return err;
		}
	}

	cdi_mgr->adap = i2c_get_adapter(pd->bus);
	if (!cdi_mgr->adap) {
		dev_err(&pdev->dev, "%s no such i2c bus %d\n",
			__func__, pd->bus);
		return -ENODEV;
	}

	if (pd->num_pwr_gpios > 0) {
		for (i = 0; i < pd->num_pwr_gpios; i++) {
			if (!gpio_is_valid(pd->pwr_gpios[i]))
				goto err_probe;

			if (devm_gpio_request(
				&pdev->dev, pd->pwr_gpios[i], "pwdn-gpios")) {
				dev_err(&pdev->dev, "failed to req GPIO: %d\n",
					pd->pwr_gpios[i]);
				goto err_probe;
			}

			err = gpio_direction_output(pd->pwr_gpios[i],
				pd->default_pwr_on ?
				PW_ON(pd->pwr_flags[i]) :
				PW_OFF(pd->pwr_flags[i]));
			if (err < 0) {
				dev_err(&pdev->dev, "failed to setup GPIO: %d\n",
					pd->pwr_gpios[i]);
				i++;
				goto err_probe;
			}
			if (pd->default_pwr_on)
				cdi_mgr->pwr_state |= BIT(i);
		}
	}

	cdi_mgr->err_irq = platform_get_irq(pdev, 0);
	if (cdi_mgr->err_irq > 0) {
		err = devm_request_irq(&pdev->dev,
				cdi_mgr->err_irq,
				cdi_mgr_isr, 0, pdev->name, cdi_mgr);
		if (err) {
			dev_err(&pdev->dev,
				"request_irq failed with err %d\n", err);
			cdi_mgr->err_irq = 0;
			goto err_probe;
		}
		disable_irq(cdi_mgr->err_irq);
		atomic_set(&cdi_mgr->irq_in_use, 0);
	}

	cdi_mgr->pdev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, cdi_mgr);

	if (pd->drv_name)
		snprintf(cdi_mgr->devname, sizeof(cdi_mgr->devname),
			"%s.%x.%c", pd->drv_name, pd->bus, 'a' + pd->csi_port);
	else
		snprintf(cdi_mgr->devname, sizeof(cdi_mgr->devname),
			"cdi-mgr.%x.%c", pd->bus, 'a' + pd->csi_port);

	/* Request dynamic allocation of a device major number */
	err = alloc_chrdev_region(&cdi_mgr->devt,
				0, CDI_DEV_MAX, cdi_mgr->devname);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to allocate char dev region %d\n",
			err);
		goto err_probe;
	}

	/* poluate sysfs entries */
	cdi_mgr->cdi_class = class_create(THIS_MODULE, cdi_mgr->devname);
	if (IS_ERR(cdi_mgr->cdi_class)) {
		err = PTR_ERR(cdi_mgr->cdi_class);
		cdi_mgr->cdi_class = NULL;
		dev_err(&pdev->dev, "failed to create class %d\n",
			err);
		goto err_probe;
	}

	cdi_mgr->cdi_class->devnode = cdi_mgr_devnode;

	/* connect the file operations with the cdev */
	cdev_init(&cdi_mgr->cdev, &cdi_mgr_fileops);
	cdi_mgr->cdev.owner = THIS_MODULE;

	/* connect the major/minor number to this dev */
	err = cdev_add(&cdi_mgr->cdev, MKDEV(MAJOR(cdi_mgr->devt), 0), 1);
	if (err) {
		dev_err(&pdev->dev, "Unable to add cdev %d\n", err);
		goto err_probe;
	}

	/* send uevents to udev, it will create /dev node for cdi-mgr */
	cdi_mgr->dev = device_create(cdi_mgr->cdi_class, &pdev->dev,
				     cdi_mgr->cdev.dev,
				     cdi_mgr,
				     cdi_mgr->devname);
	if (IS_ERR(cdi_mgr->dev)) {
		err = PTR_ERR(cdi_mgr->dev);
		cdi_mgr->dev = NULL;
		dev_err(&pdev->dev, "failed to create device %d\n", err);
		goto err_probe;
	}

	cdi_mgr_debugfs_init(cdi_mgr);
	INIT_WORK(&cdi_mgr->ins_work, cdi_mgr_dev_ins);
	schedule_work(&cdi_mgr->ins_work);
	return 0;

err_probe:
	cdi_mgr_del(cdi_mgr);
	return err;
}

static int cdi_mgr_remove(struct platform_device *pdev)
{
	struct cdi_mgr_priv *cdi_mgr = dev_get_drvdata(&pdev->dev);

	if (cdi_mgr) {
		cdi_mgr_debugfs_remove(cdi_mgr);
		cdi_mgr_del(cdi_mgr);

		if (cdi_mgr->dev)
			device_destroy(cdi_mgr->cdi_class,
				       cdi_mgr->cdev.dev);
		if (cdi_mgr->cdev.dev)
			cdev_del(&cdi_mgr->cdev);

		if (cdi_mgr->cdi_class)
			class_destroy(cdi_mgr->cdi_class);

		if (cdi_mgr->devt)
			unregister_chrdev_region(cdi_mgr->devt, CDI_DEV_MAX);
	}

	return 0;
}

static const struct of_device_id cdi_mgr_of_match[] = {
	{ .compatible = "nvidia,cdi-mgr", },
	{ }
};
MODULE_DEVICE_TABLE(of, cdi_mgr_of_match);

static struct platform_driver cdi_mgr_driver = {
	.driver = {
		.name = "cdi-mgr",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cdi_mgr_of_match),
		.pm = &cdi_mgr_pm_ops,
	},
	.probe = cdi_mgr_probe,
	.remove = cdi_mgr_remove,
};

module_platform_driver(cdi_mgr_driver);

MODULE_DESCRIPTION("tegra auto cdi manager driver");
MODULE_AUTHOR("Songhee Baek <sbeak@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cdi_mgr");
MODULE_SOFTDEP("pre: cdi_pwm");
