/*
 * Error log support on PowerNV.
 *
 * Copyright 2013,2014 IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/fcntl.h>
#include <linux/kobject.h>
#include <asm/uaccess.h>
#include <asm/opal.h>

struct elog_obj {
	struct kobject kobj;
	struct bin_attribute raw_attr;
	uint64_t id;
	uint64_t type;
	size_t size;
	char *buffer;
};
#define to_elog_obj(x) container_of(x, struct elog_obj, kobj)

struct elog_attribute {
	struct attribute attr;
	ssize_t (*show)(struct elog_obj *elog, struct elog_attribute *attr,
			char *buf);
	ssize_t (*store)(struct elog_obj *elog, struct elog_attribute *attr,
			 const char *buf, size_t count);
};
#define to_elog_attr(x) container_of(x, struct elog_attribute, attr)

static ssize_t elog_id_show(struct elog_obj *elog_obj,
			    struct elog_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "0x%llx\n", elog_obj->id);
}

static const char *elog_type_to_string(uint64_t type)
{
	switch (type) {
	case 0: return "PEL";
	default: return "unknown";
	}
}

static ssize_t elog_type_show(struct elog_obj *elog_obj,
			      struct elog_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "0x%llx %s\n",
		       elog_obj->type,
		       elog_type_to_string(elog_obj->type));
}

static ssize_t elog_ack_show(struct elog_obj *elog_obj,
			     struct elog_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "ack - acknowledge log message\n");
}

static ssize_t elog_ack_store(struct elog_obj *elog_obj,
			      struct elog_attribute *attr,
			      const char *buf,
			      size_t count)
{
	opal_send_ack_elog(elog_obj->id);
	sysfs_remove_file_self(&elog_obj->kobj, &attr->attr);
	kobject_put(&elog_obj->kobj);
	return count;
}

static struct elog_attribute id_attribute =
	__ATTR(id, 0666, elog_id_show, NULL);
static struct elog_attribute type_attribute =
	__ATTR(type, 0666, elog_type_show, NULL);
static struct elog_attribute ack_attribute =
	__ATTR(acknowledge, 0660, elog_ack_show, elog_ack_store);

static struct kset *elog_kset;

static ssize_t elog_attr_show(struct kobject *kobj,
			      struct attribute *attr,
			      char *buf)
{
	struct elog_attribute *attribute;
	struct elog_obj *elog;

	attribute = to_elog_attr(attr);
	elog = to_elog_obj(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(elog, attribute, buf);
}

static ssize_t elog_attr_store(struct kobject *kobj,
			       struct attribute *attr,
			       const char *buf, size_t len)
{
	struct elog_attribute *attribute;
	struct elog_obj *elog;

	attribute = to_elog_attr(attr);
	elog = to_elog_obj(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(elog, attribute, buf, len);
}

static const struct sysfs_ops elog_sysfs_ops = {
	.show = elog_attr_show,
	.store = elog_attr_store,
};

static void elog_release(struct kobject *kobj)
{
	struct elog_obj *elog;

	elog = to_elog_obj(kobj);
	kfree(elog->buffer);
	kfree(elog);
}

static struct attribute *elog_default_attrs[] = {
	&id_attribute.attr,
	&type_attribute.attr,
	&ack_attribute.attr,
	NULL,
};

static struct kobj_type elog_ktype = {
	.sysfs_ops = &elog_sysfs_ops,
	.release = &elog_release,
	.default_attrs = elog_default_attrs,
};

/* Maximum size of a single log on FSP is 16KB */
#define OPAL_MAX_ERRLOG_SIZE	16384

static ssize_t raw_attr_read(struct file *filep, struct kobject *kobj,
			     struct bin_attribute *bin_attr,
			     char *buffer, loff_t pos, size_t count)
{
	int opal_rc;

	struct elog_obj *elog = to_elog_obj(kobj);

	/* We may have had an error reading before, so let's retry */
	if (!elog->buffer) {
		elog->buffer = kzalloc(elog->size, GFP_KERNEL);
		if (!elog->buffer)
			return -EIO;

		opal_rc = opal_read_elog(__pa(elog->buffer),
					 elog->size, elog->id);
		if (opal_rc != OPAL_SUCCESS) {
			pr_err("ELOG: log read failed for log-id=%llx\n",
			       elog->id);
			kfree(elog->buffer);
			elog->buffer = NULL;
			return -EIO;
		}
	}

	memcpy(buffer, elog->buffer + pos, count);

	return count;
}

static struct elog_obj *create_elog_obj(uint64_t id, size_t size, uint64_t type)
{
	struct elog_obj *elog;
	int rc;

	elog = kzalloc(sizeof(*elog), GFP_KERNEL);
	if (!elog)
		return NULL;

	elog->kobj.kset = elog_kset;

	kobject_init(&elog->kobj, &elog_ktype);

	sysfs_bin_attr_init(&elog->raw_attr);

	elog->raw_attr.attr.name = "raw";
	elog->raw_attr.attr.mode = 0400;
	elog->raw_attr.size = size;
	elog->raw_attr.read = raw_attr_read;

	elog->id = id;
	elog->size = size;
	elog->type = type;

	elog->buffer = kzalloc(elog->size, GFP_KERNEL);

	if (elog->buffer) {
		rc = opal_read_elog(__pa(elog->buffer),
					 elog->size, elog->id);
		if (rc != OPAL_SUCCESS) {
			pr_err("ELOG: log read failed for log-id=%llx\n",
			       elog->id);
			kfree(elog->buffer);
			elog->buffer = NULL;
		}
	}

	rc = kobject_add(&elog->kobj, NULL, "0x%llx", id);
	if (rc) {
		kobject_put(&elog->kobj);
		return NULL;
	}

	rc = sysfs_create_bin_file(&elog->kobj, &elog->raw_attr);
	if (rc) {
		kobject_put(&elog->kobj);
		return NULL;
	}

	kobject_uevent(&elog->kobj, KOBJ_ADD);

	return elog;
}

static void elog_work_fn(struct work_struct *work)
{
	size_t elog_size;
	uint64_t log_id;
	uint64_t elog_type;
	int rc;
	char name[2+16+1];

	rc = opal_get_elog_size(&log_id, &elog_size, &elog_type);
	if (rc != OPAL_SUCCESS) {
		pr_err("ELOG: Opal log read failed\n");
		return;
	}

	BUG_ON(elog_size > OPAL_MAX_ERRLOG_SIZE);

	if (elog_size >= OPAL_MAX_ERRLOG_SIZE)
		elog_size  =  OPAL_MAX_ERRLOG_SIZE;

	sprintf(name, "0x%llx", log_id);

	/* we may get notified twice, let's handle
	 * that gracefully and not create two conflicting
	 * entries.
	 */
	if (kset_find_obj(elog_kset, name))
		return;

	create_elog_obj(log_id, elog_size, elog_type);
}

static DECLARE_WORK(elog_work, elog_work_fn);

static int elog_event(struct notifier_block *nb,
				unsigned long events, void *change)
{
	/* check for error log event */
	if (events & OPAL_EVENT_ERROR_LOG_AVAIL)
		schedule_work(&elog_work);
	return 0;
}

static struct notifier_block elog_nb = {
	.notifier_call  = elog_event,
	.next           = NULL,
	.priority       = 0
};

int __init opal_elog_init(void)
{
	int rc = 0;

	elog_kset = kset_create_and_add("elog", NULL, opal_kobj);
	if (!elog_kset) {
		pr_warn("%s: failed to create elog kset\n", __func__);
		return -1;
	}

	rc = opal_notifier_register(&elog_nb);
	if (rc) {
		pr_err("%s: Can't register OPAL event notifier (%d)\n",
		__func__, rc);
		return rc;
	}

	/* We are now ready to pull error logs from opal. */
	opal_resend_pending_logs();

	return 0;
}
