/*
 * HID driver for Nintendo Wiimote extension devices
 * Copyright (c) 2011 David Herrmann
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include "hid-wiimote.h"

struct wiimote_ext {
	struct wiimote_data *wdata;
	struct work_struct worker;

	atomic_t opened;
	atomic_t mp_opened;
	bool plugged;
	bool motionp;
	__u8 ext_type;
};

enum wiiext_type {
	WIIEXT_NONE,		/* placeholder */
	WIIEXT_CLASSIC,		/* Nintendo classic controller */
	WIIEXT_NUNCHUCK,	/* Nintendo nunchuck controller */
};

/* diable all extensions */
static void ext_disable(struct wiimote_ext *ext)
{
	unsigned long flags;
	__u8 wmem = 0x55;

	if (!wiimote_cmd_acquire(ext->wdata)) {
		wiimote_cmd_write(ext->wdata, 0xa400f0, &wmem, sizeof(wmem));
		wiimote_cmd_release(ext->wdata);
	}

	spin_lock_irqsave(&ext->wdata->state.lock, flags);
	ext->motionp = false;
	ext->ext_type = WIIEXT_NONE;
	wiiproto_req_drm(ext->wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&ext->wdata->state.lock, flags);
}

static bool motionp_read(struct wiimote_ext *ext)
{
	__u8 rmem[2], wmem;
	ssize_t ret;
	bool avail = false;

	if (wiimote_cmd_acquire(ext->wdata))
		return false;

	/* initialize motion plus */
	wmem = 0x55;
	ret = wiimote_cmd_write(ext->wdata, 0xa600f0, &wmem, sizeof(wmem));
	if (ret)
		goto error;

	/* read motion plus ID */
	ret = wiimote_cmd_read(ext->wdata, 0xa600fe, rmem, 2);
	if (ret == 2 || rmem[1] == 0x5)
		avail = true;

error:
	wiimote_cmd_release(ext->wdata);
	return avail;
}

static __u8 ext_read(struct wiimote_ext *ext)
{
	ssize_t ret;
	__u8 rmem[2], wmem;
	__u8 type = WIIEXT_NONE;

	if (!ext->plugged)
		return WIIEXT_NONE;

	if (wiimote_cmd_acquire(ext->wdata))
		return WIIEXT_NONE;

	/* initialize extension */
	wmem = 0x55;
	ret = wiimote_cmd_write(ext->wdata, 0xa400f0, &wmem, sizeof(wmem));
	if (!ret) {
		/* disable encryption */
		wmem = 0x0;
		wiimote_cmd_write(ext->wdata, 0xa400fb, &wmem, sizeof(wmem));
	}

	/* read extension ID */
	ret = wiimote_cmd_read(ext->wdata, 0xa400fe, rmem, 2);
	if (ret == 2) {
		if (rmem[0] == 0 && rmem[1] == 0)
			type = WIIEXT_NUNCHUCK;
		else if (rmem[0] == 0x01 && rmem[1] == 0x01)
			type = WIIEXT_CLASSIC;
	}

	wiimote_cmd_release(ext->wdata);

	return type;
}

static void ext_enable(struct wiimote_ext *ext, bool motionp, __u8 ext_type)
{
	unsigned long flags;
	__u8 wmem;
	int ret;

	if (motionp) {
		if (wiimote_cmd_acquire(ext->wdata))
			return;

		if (ext_type == WIIEXT_CLASSIC)
			wmem = 0x07;
		else if (ext_type == WIIEXT_NUNCHUCK)
			wmem = 0x05;
		else
			wmem = 0x04;

		ret = wiimote_cmd_write(ext->wdata, 0xa600fe, &wmem, sizeof(wmem));
		wiimote_cmd_release(ext->wdata);
		if (ret)
			return;
	}

	spin_lock_irqsave(&ext->wdata->state.lock, flags);
	ext->motionp = motionp;
	ext->ext_type = ext_type;
	wiiproto_req_drm(ext->wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&ext->wdata->state.lock, flags);
}

static void wiiext_worker(struct work_struct *work)
{
	struct wiimote_ext *ext = container_of(work, struct wiimote_ext,
									worker);
	bool motionp;
	__u8 ext_type;

	ext_disable(ext);
	motionp = motionp_read(ext);
	ext_type = ext_read(ext);
	ext_enable(ext, motionp, ext_type);
}

/* schedule work only once, otherwise mark for reschedule */
static void wiiext_schedule(struct wiimote_ext *ext)
{
	queue_work(system_nrt_wq, &ext->worker);
}

/*
 * Reacts on extension port events
 * Whenever the driver gets an event from the wiimote that an extension has been
 * plugged or unplugged, this funtion shall be called. It checks what extensions
 * are connected and initializes and activates them.
 * This can be called in atomic context. The initialization is done in a
 * separate worker thread. The state.lock spinlock must be held by the caller.
 */
void wiiext_event(struct wiimote_data *wdata, bool plugged)
{
	if (!wdata->ext)
		return;

	if (wdata->ext->plugged == plugged)
		return;

	wdata->ext->plugged = plugged;
	/*
	 * We need to call wiiext_schedule(wdata->ext) here, however, the
	 * extension initialization logic is not fully understood and so
	 * automatic initialization is not supported, yet.
	 */
}

/*
 * Returns true if the current DRM mode should contain extension data and false
 * if there is no interest in extension data.
 * All supported extensions send 6 byte extension data so any DRM that contains
 * extension bytes is fine.
 * The caller must hold the state.lock spinlock.
 */
bool wiiext_active(struct wiimote_data *wdata)
{
	if (!wdata->ext)
		return false;

	return wdata->ext->motionp || wdata->ext->ext_type;
}

/* Initializes the extension driver of a wiimote */
int wiiext_init(struct wiimote_data *wdata)
{
	struct wiimote_ext *ext;
	unsigned long flags;

	ext = kzalloc(sizeof(*ext), GFP_KERNEL);
	if (!ext)
		return -ENOMEM;

	ext->wdata = wdata;
	INIT_WORK(&ext->worker, wiiext_worker);

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->ext = ext;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

/* Deinitializes the extension driver of a wiimote */
void wiiext_deinit(struct wiimote_data *wdata)
{
	struct wiimote_ext *ext = wdata->ext;
	unsigned long flags;

	if (!ext)
		return;

	/*
	 * We first unset wdata->ext to avoid further input from the wiimote
	 * core. The worker thread does not access this pointer so it is not
	 * affected by this.
	 * We kill the worker after this so it does not get respawned during
	 * deinitialization.
	 */

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->ext = NULL;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	cancel_work_sync(&ext->worker);
	kfree(ext);
}
