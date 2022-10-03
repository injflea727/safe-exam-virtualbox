/* $Id: vpox_irq.c $ */
/** @file
 * VirtualPox Additions Linux kernel video driver
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 * This file is based on qxl_irq.c
 * Copyright 2013 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alon Levy
 *          Michael Thayer <michael.thayer@oracle.com,
 *          Hans de Goede <hdegoede@redhat.com>
 */
#include "vpox_drv.h"

#if RTLNX_VER_MAX(5,1,0)
# include <drm/drm_crtc_helper.h>
# if RTLNX_RHEL_MAJ_PREREQ(8,1)
#  include <drm/drm_probe_helper.h>
# endif
#else
# include <drm/drm_probe_helper.h>
#endif
#include <VPoxVideo.h>

static void vpox_clear_irq(void)
{
	outl((u32)~0, VGA_PORT_HGSMI_HOST);
}

static u32 vpox_get_flags(struct vpox_private *vpox)
{
	return readl(vpox->guest_heap + HOST_FLAGS_OFFSET);
}

void vpox_report_hotplug(struct vpox_private *vpox)
{
	schedule_work(&vpox->hotplug_work);
}

irqreturn_t vpox_irq_handler(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *)arg;
	struct vpox_private *vpox = (struct vpox_private *)dev->dev_private;
	u32 host_flags = vpox_get_flags(vpox);

	if (!(host_flags & HGSMIHOSTFLAGS_IRQ))
		return IRQ_NONE;

	/*
	 * Due to a bug in the initial host implementation of hot-plug irqs,
	 * the hot-plug and cursor capability flags were never cleared.
	 * Fortunately we can tell when they would have been set by checking
	 * that the VSYNC flag is not set.
	 */
	if (host_flags &
	    (HGSMIHOSTFLAGS_HOTPLUG | HGSMIHOSTFLAGS_CURSOR_CAPABILITIES) &&
	    !(host_flags & HGSMIHOSTFLAGS_VSYNC))
		vpox_report_hotplug(vpox);

	vpox_clear_irq();

	return IRQ_HANDLED;
}

/**
 * Check that the position hints provided by the host are suitable for GNOME
 * shell (i.e. all screens disjoint and hints for all enabled screens) and if
 * not replace them with default ones.  Providing valid hints improves the
 * chances that we will get a known screen layout for pointer mapping.
 */
static void validate_or_set_position_hints(struct vpox_private *vpox)
{
	struct VBVAMODEHINT *hintsi, *hintsj;
	bool valid = true;
	u16 currentx = 0;
	int i, j;

	for (i = 0; i < vpox->num_crtcs; ++i) {
		for (j = 0; j < i; ++j) {
			hintsi = &vpox->last_mode_hints[i];
			hintsj = &vpox->last_mode_hints[j];

			if (hintsi->fEnabled && hintsj->fEnabled) {
				if (hintsi->dx >= 0xffff ||
				    hintsi->dy >= 0xffff ||
				    hintsj->dx >= 0xffff ||
				    hintsj->dy >= 0xffff ||
				    (hintsi->dx <
					hintsj->dx + (hintsj->cx & 0x8fff) &&
				     hintsi->dx + (hintsi->cx & 0x8fff) >
					hintsj->dx) ||
				    (hintsi->dy <
					hintsj->dy + (hintsj->cy & 0x8fff) &&
				     hintsi->dy + (hintsi->cy & 0x8fff) >
					hintsj->dy))
					valid = false;
			}
		}
	}
	if (!valid)
		for (i = 0; i < vpox->num_crtcs; ++i) {
			if (vpox->last_mode_hints[i].fEnabled) {
				vpox->last_mode_hints[i].dx = currentx;
				vpox->last_mode_hints[i].dy = 0;
				currentx +=
				    vpox->last_mode_hints[i].cx & 0x8fff;
			}
		}
}

/**
 * Query the host for the most recent video mode hints.
 */
static void vpox_update_mode_hints(struct vpox_private *vpox)
{
	struct drm_device *dev = vpox->dev;
	struct drm_connector *connector;
	struct vpox_connector *vpox_conn;
	struct VBVAMODEHINT *hints;
	u16 flags;
	bool disconnected;
	unsigned int crtc_id;
	int ret;

	ret = VPoxHGSMIGetModeHints(vpox->guest_pool, vpox->num_crtcs,
				   vpox->last_mode_hints);
	if (ret) {
		DRM_ERROR("vpoxvideo: hgsmi_get_mode_hints failed: %d\n", ret);
		return;
	}

	validate_or_set_position_hints(vpox);
#if RTLNX_VER_MIN(3,9,0)
	drm_modeset_lock_all(dev);
#else
	mutex_lock(&dev->mode_config.mutex);
#endif
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		vpox_conn = to_vpox_connector(connector);

		hints = &vpox->last_mode_hints[vpox_conn->vpox_crtc->crtc_id];
		if (hints->magic != VBVAMODEHINT_MAGIC)
			continue;

		disconnected = !(hints->fEnabled);
		crtc_id = vpox_conn->vpox_crtc->crtc_id;
		vpox_conn->mode_hint.width = hints->cx;
		vpox_conn->mode_hint.height = hints->cy;
		vpox_conn->vpox_crtc->x_hint = hints->dx;
		vpox_conn->vpox_crtc->y_hint = hints->dy;
		vpox_conn->mode_hint.disconnected = disconnected;

		if (vpox_conn->vpox_crtc->disconnected == disconnected)
			continue;

		if (disconnected)
			flags = VBVA_SCREEN_F_ACTIVE | VBVA_SCREEN_F_DISABLED;
		else
			flags = VBVA_SCREEN_F_ACTIVE | VBVA_SCREEN_F_BLANK;

		VPoxHGSMIProcessDisplayInfo(vpox->guest_pool, crtc_id, 0, 0, 0,
					   hints->cx * 4, hints->cx,
					   hints->cy, 0, flags);

		vpox_conn->vpox_crtc->disconnected = disconnected;
	}
#if RTLNX_VER_MIN(3,9,0)
	drm_modeset_unlock_all(dev);
#else
	mutex_unlock(&dev->mode_config.mutex);
#endif
}

static void vpox_hotplug_worker(struct work_struct *work)
{
	struct vpox_private *vpox = container_of(work, struct vpox_private,
						 hotplug_work);

	vpox_update_mode_hints(vpox);
	drm_kms_helper_hotplug_event(vpox->dev);
}

int vpox_irq_init(struct vpox_private *vpox)
{
	INIT_WORK(&vpox->hotplug_work, vpox_hotplug_worker);
	vpox_update_mode_hints(vpox);
#if RTLNX_VER_MIN(5,15,0) || RTLNX_RHEL_MAJ_PREREQ(9,1)
	return request_irq(VPOX_DRM_TO_PCI_DEV(vpox->dev)->irq, vpox_irq_handler, IRQF_SHARED, vpox->dev->driver->name, vpox->dev);
#elif RTLNX_VER_MIN(3,16,0) || RTLNX_RHEL_MAJ_PREREQ(7,1)
	return drm_irq_install(vpox->dev, VPOX_DRM_TO_PCI_DEV(vpox->dev)->irq);
#else
	return drm_irq_install(vpox->dev);
#endif
}

void vpox_irq_fini(struct vpox_private *vpox)
{
#if RTLNX_VER_MIN(5,15,0) || RTLNX_RHEL_MAJ_PREREQ(9,1)
	free_irq(VPOX_DRM_TO_PCI_DEV(vpox->dev)->irq, vpox->dev);
#else
	drm_irq_uninstall(vpox->dev);
#endif
	flush_work(&vpox->hotplug_work);
}
