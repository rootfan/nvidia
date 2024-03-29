/*
 * drivers/video/tegra/nvmap/nvmap_sci_ipc.c
 *
 * mapping between nvmap_hnadle and sci_ipc entery
 *
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/nvmap.h>
#include <linux/rbtree.h>
#include <linux/list.h>

#include <linux/nvscierror.h>
#include <linux/nvsciipc_interface.h>

#include "nvmap_priv.h"

struct nvmap_sci_ipc {
	struct rb_root entries;
	struct mutex mlock;
	struct list_head free_sid_list;
};

struct free_sid_node {
	struct list_head list;
	u32 sid;
};

/* An rb-tree root node for holding sci_ipc_id of clients */
struct nvmap_sci_ipc_entry {
	struct rb_node entry;
	struct nvmap_client *client;
	struct nvmap_handle *handle;
	u32 sci_ipc_id;
	u64 peer_vuid;
	u32 flags;
	u32 refcount;
};

static struct nvmap_sci_ipc *nvmapsciipc;

int nvmap_validate_sci_ipc_params(struct nvmap_client *client,
			NvSciIpcEndpointAuthToken auth_token,
			NvSciIpcEndpointVuid *pr_vuid,
			NvSciIpcEndpointVuid *lu_vuid)
{
	NvSciError err = NvSciError_Success;
	NvSciIpcTopoId pr_topoid;
	int ret = 0;

	err = NvSciIpcEndpointValidateAuthTokenLinuxCurrent(auth_token,
			lu_vuid);
	if (err != NvSciError_Success) {
		ret = -EINVAL;
		goto out;
	}

	err = NvSciIpcEndpointMapVuid(*lu_vuid, &pr_topoid, pr_vuid);
	if (err != NvSciError_Success) {
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static u32 nvmap_unique_sci_ipc_id(void)
{
	static atomic_t unq_id = { 0 };
	u32 id;

	if (!list_empty(&nvmapsciipc->free_sid_list)) {
		struct free_sid_node *fnode = list_first_entry(
			&nvmapsciipc->free_sid_list,
			typeof(*fnode),
			list);

		id = fnode->sid;
		list_del(&fnode->list);
		kfree(fnode);
		goto ret_id;
	}

	id = atomic_add_return(2, &unq_id);

ret_id:
	WARN_ON(id == 0);
	return id;
}

static struct nvmap_sci_ipc_entry *nvmap_search_sci_ipc_entry(
	struct rb_root *root,
	struct nvmap_handle *h,
	u32 flags,
	NvSciIpcEndpointVuid peer_vuid)
{
	struct rb_node *node;  /* top of the tree */
	struct nvmap_sci_ipc_entry *entry;

	for (node = rb_first(root); node; node = rb_next(node)) {
		entry = rb_entry(node, struct nvmap_sci_ipc_entry, entry);

		if (entry && entry->handle == h
			&& entry->flags == flags
			&& entry->peer_vuid == peer_vuid)
			return entry;
	}
	return NULL;
}

static void nvmap_insert_sci_ipc_entry(struct rb_root *root,
		struct nvmap_sci_ipc_entry *new)
{
	struct nvmap_sci_ipc_entry *entry;
	struct rb_node *parent = NULL;
	u32 sid = new->sci_ipc_id;
	struct rb_node **link;

	link = &root->rb_node;
	/* Go to the bottom of the tree */
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct nvmap_sci_ipc_entry, entry);

		if (entry->sci_ipc_id > sid)
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}

	/* Put the new node there */
	rb_link_node(&new->entry, parent, link);
	rb_insert_color(&new->entry, root);
}

int nvmap_create_sci_ipc_id(struct nvmap_client *client,
				struct nvmap_handle *h,
				u32 flags,
				u32 *sci_ipc_id,
				NvSciIpcEndpointVuid peer_vuid)
{
	struct nvmap_sci_ipc_entry *new_entry;
	struct nvmap_sci_ipc_entry *entry;
	int ret = -EINVAL;
	u32 id;

	mutex_lock(&nvmapsciipc->mlock);

	entry = nvmap_search_sci_ipc_entry(&nvmapsciipc->entries,
			h, flags, peer_vuid);
	if (entry) {
		entry->refcount++;
		*sci_ipc_id = entry->sci_ipc_id;
		pr_debug("%d: matched Sci_Ipc_Id:%u\n", __LINE__, *sci_ipc_id);
		ret = 0;
		goto unlock;
	} else {
		new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
		if (!new_entry) {
			ret = -ENOMEM;
			goto unlock;
		}
		id = nvmap_unique_sci_ipc_id();
		*sci_ipc_id = id;
		new_entry->sci_ipc_id = id;
		new_entry->client = client;
		new_entry->handle = h;
		new_entry->peer_vuid = peer_vuid;
		new_entry->flags = flags;
		new_entry->refcount = 1;

		pr_debug("%d: New Sci_ipc_id %d entry->pr_vuid: %llu entry->flags: %u new_entry->handle:0x%p\n",
			__LINE__, new_entry->sci_ipc_id, new_entry->peer_vuid,
			new_entry->flags, new_entry->handle);

		nvmap_insert_sci_ipc_entry(&nvmapsciipc->entries, new_entry);
		ret = 0;
	}
unlock:
	mutex_unlock(&nvmapsciipc->mlock);
	if (!ret)
		(void)nvmap_handle_get(h);
	return ret;
}

static struct nvmap_sci_ipc_entry *nvmap_find_entry_for_id(struct rb_root *es,
					 u32 id)
{
	struct nvmap_sci_ipc_entry *e = NULL;
	struct rb_node *n;

	for (n = rb_first(es); n; n = rb_next(n)) {
		e = rb_entry(n, struct nvmap_sci_ipc_entry, entry);
		if (e && e->sci_ipc_id == id)
			goto found;
	}
found:
	return e;
}

int nvmap_get_handle_from_sci_ipc_id(struct nvmap_client *client, u32 flags,
		u32 sci_ipc_id, NvSciIpcEndpointVuid localu_vuid, u32 *h)
{
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_sci_ipc_entry *entry;
	struct nvmap_handle *h_org;
	int ret = 0;
	int fd;

	mutex_lock(&nvmapsciipc->mlock);

	pr_debug("%d: Sci_Ipc_Id %d local_vuid: %llu flags: %u\n",
		__LINE__, sci_ipc_id, localu_vuid, flags);

	entry = nvmap_find_entry_for_id(&nvmapsciipc->entries, sci_ipc_id);
	if ((entry == NULL) || (entry->handle == NULL) ||
		(entry->peer_vuid != localu_vuid) || (entry->flags != flags)) {

		pr_err("%d: No matching Sci_Ipc_Id %d found\n",
		__LINE__, sci_ipc_id);

		ret = -EINVAL;
		goto unlock;
	}

	h_org = entry->handle;

	/* h->dmabuf can't be NULL anymore. Duplicate the handle. */
	ref = nvmap_duplicate_handle(client, h_org, true);
	if (!ref) {
		ret = -EINVAL;
		goto unlock;
	}
	nvmap_handle_put(h_org);

	fd = nvmap_get_dmabuf_fd(client, ref->handle);
	*h = fd;
	fd_install(fd, ref->handle->dmabuf->file);

	entry->refcount--;
	if (entry->refcount == 0U) {
		struct free_sid_node *free_node;

		rb_erase(&entry->entry, &nvmapsciipc->entries);
		free_node = kzalloc(sizeof(*free_node), GFP_KERNEL);
		if (free_node == NULL) {
			ret = -ENOMEM;
			kfree(entry);
			goto unlock;
		}
		free_node->sid = entry->sci_ipc_id;
		list_add_tail(&free_node->list, &nvmapsciipc->free_sid_list);
		kfree(entry);
	}
unlock:
	mutex_unlock(&nvmapsciipc->mlock);
	return ret;
}

int nvmap_sci_ipc_init(void)
{
	nvmapsciipc = kzalloc(sizeof(*nvmapsciipc), GFP_KERNEL);
	if (!nvmapsciipc)
		return -ENOMEM;
	nvmapsciipc->entries = RB_ROOT;
	INIT_LIST_HEAD(&nvmapsciipc->free_sid_list);
	mutex_init(&nvmapsciipc->mlock);

	return 0;
}

void nvmap_sci_ipc_exit(void)
{
	struct nvmap_sci_ipc_entry *e;
	struct free_sid_node *fnode;
	struct rb_node *n;

	mutex_lock(&nvmapsciipc->mlock);
	while ((n = rb_first(&nvmapsciipc->entries))) {
		e = rb_entry(n, struct nvmap_sci_ipc_entry, entry);
		rb_erase(&e->entry, &nvmapsciipc->entries);
		kfree(e);
	}

	list_for_each_entry(fnode, &nvmapsciipc->free_sid_list, list) {
		list_del(&fnode->list);
		kfree(fnode);
	}

	nvmapsciipc = NULL;
	mutex_unlock(&nvmapsciipc->mlock);
}
