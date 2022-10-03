/* $Id: vpox_mode.c $ */
/** @file
 * VirtualPox Additions Linux kernel video driver
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 * This file is based on ast_mode.c
 * Copyright 2012 Red Hat Inc.
 * Parts based on xf86-video-ast
 * Copyright (c) 2005 ASPEED Technology Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 */
/*
 * Authors: Dave Airlie <airlied@redhat.com>
 *          Michael Thayer <michael.thayer@oracle.com,
 *          Hans de Goede <hdegoede@redhat.com>
 */
#include "vpox_drv.h"
#include <linux/export.h>
#include <drm/drm_crtc_helper.h>
#if RTLNX_VER_MIN(3,18,0) || RTLNX_RHEL_MAJ_PREREQ(7,2)
# include <drm/drm_plane_helper.h>
#endif
#if RTLNX_VER_MIN(5,1,0) || RTLNX_RHEL_MAJ_PREREQ(8,1)
# include <drm/drm_probe_helper.h>
#endif

#if RTLNX_VER_MIN(6,0,0)
# include <drm/drm_edid.h>
#endif

#include "VPoxVideo.h"

static int vpox_cursor_set2(struct drm_crtc *crtc, struct drm_file *file_priv,
			    u32 handle, u32 width, u32 height,
			    s32 hot_x, s32 hot_y);
static int vpox_cursor_move(struct drm_crtc *crtc, int x, int y);

/**
 * Set a graphics mode.  Poke any required values into registers, do an HGSMI
 * mode set and tell the host we support advanced graphics functions.
 */
static void vpox_do_modeset(struct drm_crtc *crtc,
			    const struct drm_display_mode *mode)
{
	struct vpox_crtc *vpox_crtc = to_vpox_crtc(crtc);
	struct vpox_private *vpox;
	int width, height, bpp, pitch;
	u16 flags;
	s32 x_offset, y_offset;

	vpox = crtc->dev->dev_private;
	width = mode->hdisplay ? mode->hdisplay : 640;
	height = mode->vdisplay ? mode->vdisplay : 480;
#if RTLNX_VER_MIN(4,11,0) || RTLNX_RHEL_MAJ_PREREQ(7,5)
	bpp = crtc->enabled ? CRTC_FB(crtc)->format->cpp[0] * 8 : 32;
	pitch = crtc->enabled ? CRTC_FB(crtc)->pitches[0] : width * bpp / 8;
#elif RTLNX_VER_MIN(3,3,0)
	bpp = crtc->enabled ? CRTC_FB(crtc)->bits_per_pixel : 32;
	pitch = crtc->enabled ? CRTC_FB(crtc)->pitches[0] : width * bpp / 8;
#else
	bpp = crtc->enabled ? CRTC_FB(crtc)->bits_per_pixel : 32;
	pitch = crtc->enabled ? CRTC_FB(crtc)->pitch : width * bpp / 8;
#endif
	x_offset = vpox->single_framebuffer ? crtc->x : vpox_crtc->x_hint;
	y_offset = vpox->single_framebuffer ? crtc->y : vpox_crtc->y_hint;

	/*
	 * This is the old way of setting graphics modes.  It assumed one screen
	 * and a frame-buffer at the start of video RAM.  On older versions of
	 * VirtualPox, certain parts of the code still assume that the first
	 * screen is programmed this way, so try to fake it.
	 */
	if (vpox_crtc->crtc_id == 0 && crtc->enabled &&
	    vpox_crtc->fb_offset / pitch < 0xffff - crtc->y &&
	    vpox_crtc->fb_offset % (bpp / 8) == 0)
		VPoxVideoSetModeRegisters(
			width, height, pitch * 8 / bpp,
#if RTLNX_VER_MIN(4,11,0) || RTLNX_RHEL_MAJ_PREREQ(7,5)
			CRTC_FB(crtc)->format->cpp[0] * 8,
#else
			CRTC_FB(crtc)->bits_per_pixel,
#endif
			0,
			vpox_crtc->fb_offset % pitch / bpp * 8 + crtc->x,
			vpox_crtc->fb_offset / pitch + crtc->y);

	flags = VBVA_SCREEN_F_ACTIVE;
	flags |= (crtc->enabled && !vpox_crtc->blanked) ?
		 0 : VBVA_SCREEN_F_BLANK;
	flags |= vpox_crtc->disconnected ? VBVA_SCREEN_F_DISABLED : 0;
	VPoxHGSMIProcessDisplayInfo(vpox->guest_pool, vpox_crtc->crtc_id,
				   x_offset, y_offset, vpox_crtc->fb_offset +
				   crtc->x * bpp / 8 + crtc->y * pitch,
				   pitch, width, height,
				   vpox_crtc->blanked ? 0 : bpp, flags);
}

static void vpox_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct vpox_crtc *vpox_crtc = to_vpox_crtc(crtc);
	struct vpox_private *vpox = crtc->dev->dev_private;

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		vpox_crtc->blanked = false;
		/* Restart the refresh timer if necessary. */
		schedule_delayed_work(&vpox->refresh_work, VPOX_REFRESH_PERIOD);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		vpox_crtc->blanked = true;
		break;
	}

	mutex_lock(&vpox->hw_mutex);
	vpox_do_modeset(crtc, &crtc->hwmode);
	mutex_unlock(&vpox->hw_mutex);
}

static bool vpox_crtc_mode_fixup(struct drm_crtc *crtc,
				 const struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	return true;
}

/*
 * Try to map the layout of virtual screens to the range of the input device.
 * Return true if we need to re-set the crtc modes due to screen offset
 * changes.
 */
static bool vpox_set_up_input_mapping(struct vpox_private *vpox)
{
	struct drm_crtc *crtci;
	struct drm_connector *connectori;
	struct drm_framebuffer *fb1 = NULL;
	bool single_framebuffer = true;
	bool old_single_framebuffer = vpox->single_framebuffer;
	u16 width = 0, height = 0;

	/*
	 * Are we using an X.Org-style single large frame-buffer for all crtcs?
	 * If so then screen layout can be deduced from the crtc offsets.
	 * Same fall-back if this is the fbdev frame-buffer.
	 */
	list_for_each_entry(crtci, &vpox->dev->mode_config.crtc_list, head) {
		if (!fb1) {
			fb1 = CRTC_FB(crtci);
			if (to_vpox_framebuffer(fb1) == &vpox->fbdev->afb)
				break;
		} else if (CRTC_FB(crtci) && fb1 != CRTC_FB(crtci)) {
			single_framebuffer = false;
		}
	}
	if (single_framebuffer) {
		list_for_each_entry(crtci, &vpox->dev->mode_config.crtc_list,
				    head) {
			if (to_vpox_crtc(crtci)->crtc_id != 0)
				continue;

			if (!CRTC_FB(crtci))
				break;
			vpox->single_framebuffer = true;
			vpox->input_mapping_width = CRTC_FB(crtci)->width;
			vpox->input_mapping_height = CRTC_FB(crtci)->height;
			return old_single_framebuffer !=
			       vpox->single_framebuffer;
		}
	}
	/* Otherwise calculate the total span of all screens. */
	list_for_each_entry(connectori, &vpox->dev->mode_config.connector_list,
			    head) {
		struct vpox_connector *vpox_connector =
		    to_vpox_connector(connectori);
		struct vpox_crtc *vpox_crtc = vpox_connector->vpox_crtc;

		width = max_t(u16, width, vpox_crtc->x_hint +
					  vpox_connector->mode_hint.width);
		height = max_t(u16, height, vpox_crtc->y_hint +
					    vpox_connector->mode_hint.height);
	}

	vpox->single_framebuffer = false;
	vpox->input_mapping_width = width;
	vpox->input_mapping_height = height;

	return old_single_framebuffer != vpox->single_framebuffer;
}

static int vpox_crtc_set_base(struct drm_crtc *crtc,
				 struct drm_framebuffer *old_fb,
				 struct drm_framebuffer *new_fb,
				 int x, int y)
{
	struct vpox_private *vpox = crtc->dev->dev_private;
	struct vpox_crtc *vpox_crtc = to_vpox_crtc(crtc);
	struct drm_gem_object *obj;
	struct vpox_framebuffer *vpox_fb;
	struct vpox_bo *bo;
	int ret;
	u64 gpu_addr;

	vpox_fb = to_vpox_framebuffer(new_fb);
	obj = vpox_fb->obj;
	bo = gem_to_vpox_bo(obj);

	ret = vpox_bo_reserve(bo, false);
	if (ret)
		return ret;

	ret = vpox_bo_pin(bo, VPOX_MEM_TYPE_VRAM, &gpu_addr);
	vpox_bo_unreserve(bo);
	if (ret)
		return ret;

	/* Unpin the previous fb.  Do this after the new one has been pinned rather
	 * than before and re-pinning it on failure in case that fails too. */
	if (old_fb) {
		vpox_fb = to_vpox_framebuffer(old_fb);
		obj = vpox_fb->obj;
		bo = gem_to_vpox_bo(obj);
		ret = vpox_bo_reserve(bo, false);
		/* This should never fail, as no one else should be accessing it and we
		 * should be running under the modeset locks. */
		if (!ret) {
			vpox_bo_unpin(bo);
			vpox_bo_unreserve(bo);
		}
		else
		{
			DRM_ERROR("unable to lock buffer object: error %d\n", ret);
		}
	}

	if (&vpox->fbdev->afb == vpox_fb)
		vpox_fbdev_set_base(vpox, gpu_addr);

	vpox_crtc->fb_offset = gpu_addr;
	if (vpox_set_up_input_mapping(vpox)) {
		struct drm_crtc *crtci;

		list_for_each_entry(crtci, &vpox->dev->mode_config.crtc_list,
				    head) {
			vpox_do_modeset(crtci, &crtci->mode);
		}
	}

	return 0;
}

static int vpox_crtc_mode_set(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode,
			      int x, int y, struct drm_framebuffer *old_fb)
{
	struct vpox_private *vpox = crtc->dev->dev_private;
	int ret = vpox_crtc_set_base(crtc, old_fb, CRTC_FB(crtc), x, y);
	if (ret)
		return ret;
	mutex_lock(&vpox->hw_mutex);
	vpox_do_modeset(crtc, mode);
	VPoxHGSMIUpdateInputMapping(vpox->guest_pool, 0, 0,
				   vpox->input_mapping_width,
				   vpox->input_mapping_height);
	mutex_unlock(&vpox->hw_mutex);

	return ret;
}

static int vpox_crtc_page_flip(struct drm_crtc *crtc,
			       struct drm_framebuffer *fb,
#if RTLNX_VER_MIN(4,12,0) || RTLNX_RHEL_MAJ_PREREQ(7,5)
			       struct drm_pending_vblank_event *event,
			       uint32_t page_flip_flags,
			       struct drm_modeset_acquire_ctx *ctx)
#elif RTLNX_VER_MIN(3,12,0) || RTLNX_RHEL_MAJ_PREREQ(7,0)
			       struct drm_pending_vblank_event *event,
			       uint32_t page_flip_flags)
#else
			       struct drm_pending_vblank_event *event)
#endif
{
	struct vpox_private *vpox = crtc->dev->dev_private;
	struct drm_device *drm = vpox->dev;
	unsigned long flags;
	int rc;

	rc = vpox_crtc_set_base(crtc, CRTC_FB(crtc), fb, 0, 0);
	if (rc)
		return rc;

	vpox_do_modeset(crtc, &crtc->mode);
	mutex_unlock(&vpox->hw_mutex);

	spin_lock_irqsave(&drm->event_lock, flags);

	if (event)
#if RTLNX_VER_MIN(3,19,0) || RTLNX_RHEL_MAJ_PREREQ(7,2)
		drm_crtc_send_vblank_event(crtc, event);
#else
		drm_send_vblank_event(drm, -1, event);
#endif

	spin_unlock_irqrestore(&drm->event_lock, flags);

	return 0;
}

static void vpox_crtc_disable(struct drm_crtc *crtc)
{
}

static void vpox_crtc_prepare(struct drm_crtc *crtc)
{
}

static void vpox_crtc_commit(struct drm_crtc *crtc)
{
}

static const struct drm_crtc_helper_funcs vpox_crtc_helper_funcs = {
	.dpms = vpox_crtc_dpms,
	.mode_fixup = vpox_crtc_mode_fixup,
	.mode_set = vpox_crtc_mode_set,
	.disable = vpox_crtc_disable,
	.prepare = vpox_crtc_prepare,
	.commit = vpox_crtc_commit,
};

static void vpox_crtc_reset(struct drm_crtc *crtc)
{
}

static void vpox_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
	kfree(crtc);
}

static const struct drm_crtc_funcs vpox_crtc_funcs = {
	.cursor_move = vpox_cursor_move,
	.cursor_set2 = vpox_cursor_set2,
	.reset = vpox_crtc_reset,
	.set_config = drm_crtc_helper_set_config,
	/* .gamma_set = vpox_crtc_gamma_set, */
	.page_flip = vpox_crtc_page_flip,
	.destroy = vpox_crtc_destroy,
};

static struct vpox_crtc *vpox_crtc_init(struct drm_device *dev, unsigned int i)
{
	struct vpox_crtc *vpox_crtc;

	vpox_crtc = kzalloc(sizeof(*vpox_crtc), GFP_KERNEL);
	if (!vpox_crtc)
		return NULL;

	vpox_crtc->crtc_id = i;

	drm_crtc_init(dev, &vpox_crtc->base, &vpox_crtc_funcs);
	drm_mode_crtc_set_gamma_size(&vpox_crtc->base, 256);
	drm_crtc_helper_add(&vpox_crtc->base, &vpox_crtc_helper_funcs);

	return vpox_crtc;
}

static void vpox_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
	kfree(encoder);
}

#if RTLNX_VER_MAX(3,13,0) && !RTLNX_RHEL_MAJ_PREREQ(7,1)
static struct drm_encoder *drm_encoder_find(struct drm_device *dev, u32 id)
{
	struct drm_mode_object *mo;

	mo = drm_mode_object_find(dev, id, DRM_MODE_OBJECT_ENCODER);
	return mo ? obj_to_encoder(mo) : NULL;
}
#endif

static struct drm_encoder *vpox_best_single_encoder(struct drm_connector
						    *connector)
{
#if RTLNX_VER_MIN(5,5,0) || RTLNX_RHEL_MIN(8,3) || RTLNX_SUSE_MAJ_PREREQ(15,3)
        struct drm_encoder *encoder;

        /* There is only one encoder per connector */
        drm_connector_for_each_possible_encoder(connector, encoder)
            return encoder;
#else /* < 5.5 || RHEL < 8.3 */
	int enc_id = connector->encoder_ids[0];

	/* pick the encoder ids */
	if (enc_id)
# if RTLNX_VER_MIN(4,15,0) || RTLNX_RHEL_MAJ_PREREQ(7,6) || (defined(CONFIG_SUSE_VERSION) && RTLNX_VER_MIN(4,12,0))
		return drm_encoder_find(connector->dev, NULL, enc_id);
# else
		return drm_encoder_find(connector->dev, enc_id);
# endif
#endif /* < 5.5 || RHEL < 8.3 */
	return NULL;
}

static const struct drm_encoder_funcs vpox_enc_funcs = {
	.destroy = vpox_encoder_destroy,
};

static void vpox_encoder_dpms(struct drm_encoder *encoder, int mode)
{
}

static bool vpox_mode_fixup(struct drm_encoder *encoder,
			    const struct drm_display_mode *mode,
			    struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void vpox_encoder_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
}

static void vpox_encoder_prepare(struct drm_encoder *encoder)
{
}

static void vpox_encoder_commit(struct drm_encoder *encoder)
{
}

static const struct drm_encoder_helper_funcs vpox_enc_helper_funcs = {
	.dpms = vpox_encoder_dpms,
	.mode_fixup = vpox_mode_fixup,
	.prepare = vpox_encoder_prepare,
	.commit = vpox_encoder_commit,
	.mode_set = vpox_encoder_mode_set,
};

static struct drm_encoder *vpox_encoder_init(struct drm_device *dev,
					     unsigned int i)
{
	struct vpox_encoder *vpox_encoder;

	vpox_encoder = kzalloc(sizeof(*vpox_encoder), GFP_KERNEL);
	if (!vpox_encoder)
		return NULL;

	drm_encoder_init(dev, &vpox_encoder->base, &vpox_enc_funcs,
#if RTLNX_VER_MIN(4,5,0) || RTLNX_RHEL_MAJ_PREREQ(7,3)
			 DRM_MODE_ENCODER_DAC, NULL);
#else
			 DRM_MODE_ENCODER_DAC);
#endif
	drm_encoder_helper_add(&vpox_encoder->base, &vpox_enc_helper_funcs);

	vpox_encoder->base.possible_crtcs = 1 << i;
	return &vpox_encoder->base;
}

/**
 * Generate EDID data with a mode-unique serial number for the virtual
 *  monitor to try to persuade Unity that different modes correspond to
 *  different monitors and it should not try to force the same resolution on
 *  them.
 */
static void vpox_set_edid(struct drm_connector *connector, int width,
			  int height)
{
	enum { EDID_SIZE = 128 };
	unsigned char edid[EDID_SIZE] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,	/* header */
		0x58, 0x58,	/* manufacturer (VBX) */
		0x00, 0x00,	/* product code */
		0x00, 0x00, 0x00, 0x00,	/* serial number goes here */
		0x01,		/* week of manufacture */
		0x00,		/* year of manufacture */
		0x01, 0x03,	/* EDID version */
		0x80,		/* capabilities - digital */
		0x00,		/* horiz. res in cm, zero for projectors */
		0x00,		/* vert. res in cm */
		0x78,		/* display gamma (120 == 2.2). */
		0xEE,		/* features (standby, suspend, off, RGB, std */
				/* colour space, preferred timing mode) */
		0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54,
		/* chromaticity for standard colour space. */
		0x00, 0x00, 0x00,	/* no default timings */
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		    0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,	/* no standard timings */
		0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x00, 0x02, 0x02,
		    0x02, 0x02,
		/* descriptor block 1 goes below */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* descriptor block 2, monitor ranges */
		0x00, 0x00, 0x00, 0xFD, 0x00,
		0x00, 0xC8, 0x00, 0xC8, 0x64, 0x00, 0x0A, 0x20, 0x20, 0x20,
		    0x20, 0x20,
		/* 0-200Hz vertical, 0-200KHz horizontal, 1000MHz pixel clock */
		0x20,
		/* descriptor block 3, monitor name */
		0x00, 0x00, 0x00, 0xFC, 0x00,
		'V', 'B', 'O', 'X', ' ', 'm', 'o', 'n', 'i', 't', 'o', 'r',
		'\n',
		/* descriptor block 4: dummy data */
		0x00, 0x00, 0x00, 0x10, 0x00,
		0x0A, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x20,
		0x00,		/* number of extensions */
		0x00		/* checksum goes here */
	};
	int clock = (width + 6) * (height + 6) * 60 / 10000;
	unsigned int i, sum = 0;

	edid[12] = width & 0xff;
	edid[13] = width >> 8;
	edid[14] = height & 0xff;
	edid[15] = height >> 8;
	edid[54] = clock & 0xff;
	edid[55] = clock >> 8;
	edid[56] = width & 0xff;
	edid[58] = (width >> 4) & 0xf0;
	edid[59] = height & 0xff;
	edid[61] = (height >> 4) & 0xf0;
	for (i = 0; i < EDID_SIZE - 1; ++i)
		sum += edid[i];
	edid[EDID_SIZE - 1] = (0x100 - (sum & 0xFF)) & 0xFF;
#if RTLNX_VER_MIN(4,19,0) || RTLNX_RHEL_MAJ_PREREQ(7,7) || RTLNX_RHEL_MAJ_PREREQ(8,1) || RTLNX_SUSE_MAJ_PREREQ(15,1) || RTLNX_SUSE_MAJ_PREREQ(12,5)
	drm_connector_update_edid_property(connector, (struct edid *)edid);
#else
	drm_mode_connector_update_edid_property(connector, (struct edid *)edid);
#endif
}

static int vpox_get_modes(struct drm_connector *connector)
{
	struct vpox_connector *vpox_connector = NULL;
	struct drm_display_mode *mode = NULL;
	struct vpox_private *vpox = NULL;
	unsigned int num_modes = 0;
	int preferred_width, preferred_height;

	vpox_connector = to_vpox_connector(connector);
	vpox = connector->dev->dev_private;
	/*
	 * Heuristic: we do not want to tell the host that we support dynamic
	 * resizing unless we feel confident that the user space client using
	 * the video driver can handle hot-plug events.  So the first time modes
	 * are queried after a "master" switch we tell the host that we do not,
	 * and immediately after we send the client a hot-plug notification as
	 * a test to see if they will respond and query again.
	 * That is also the reason why capabilities are reported to the host at
	 * this place in the code rather than elsewhere.
	 * We need to report the flags location before reporting the IRQ
	 * capability.
	 */
	VPoxHGSMIReportFlagsLocation(vpox->guest_pool, GUEST_HEAP_OFFSET(vpox) +
				    HOST_FLAGS_OFFSET);
	if (vpox_connector->vpox_crtc->crtc_id == 0)
		vpox_report_caps(vpox);
	if (!vpox->initial_mode_queried) {
		if (vpox_connector->vpox_crtc->crtc_id == 0) {
			vpox->initial_mode_queried = true;
			vpox_report_hotplug(vpox);
		}
		return drm_add_modes_noedid(connector, 800, 600);
	}
	/* Also assume that a client which supports hot-plugging also knows
	 * how to update the screen in a way we can use, the only known
	 * relevent client which cannot is Plymouth in Ubuntu 14.04. */
	vpox->need_refresh_timer = false;
	num_modes = drm_add_modes_noedid(connector, 2560, 1600);
	preferred_width = vpox_connector->mode_hint.width ?
			  vpox_connector->mode_hint.width : 1024;
	preferred_height = vpox_connector->mode_hint.height ?
			   vpox_connector->mode_hint.height : 768;
	mode = drm_cvt_mode(connector->dev, preferred_width, preferred_height,
			    60, false, false, false);
	if (mode) {
		mode->type |= DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
		++num_modes;
	}
	vpox_set_edid(connector, preferred_width, preferred_height);

#if RTLNX_VER_MIN(3,19,0) || RTLNX_RHEL_MAJ_PREREQ(7,2)
	if (vpox_connector->vpox_crtc->x_hint != -1)
		drm_object_property_set_value(&connector->base,
			vpox->dev->mode_config.suggested_x_property,
			vpox_connector->vpox_crtc->x_hint);
	else
		drm_object_property_set_value(&connector->base,
			vpox->dev->mode_config.suggested_x_property, 0);

	if (vpox_connector->vpox_crtc->y_hint != -1)
		drm_object_property_set_value(&connector->base,
			vpox->dev->mode_config.suggested_y_property,
			vpox_connector->vpox_crtc->y_hint);
	else
		drm_object_property_set_value(&connector->base,
			vpox->dev->mode_config.suggested_y_property, 0);
#endif

	return num_modes;
}

#if RTLNX_VER_MAX(3,14,0) && !RTLNX_RHEL_MAJ_PREREQ(7,1)
static int vpox_mode_valid(struct drm_connector *connector,
#else
static enum drm_mode_status vpox_mode_valid(struct drm_connector *connector,
#endif
			   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static void vpox_connector_destroy(struct drm_connector *connector)
{
#if RTLNX_VER_MAX(3,17,0) && !RTLNX_RHEL_MAJ_PREREQ(7,2)
	drm_sysfs_connector_remove(connector);
#else
	drm_connector_unregister(connector);
#endif
	drm_connector_cleanup(connector);
	kfree(connector);
}

static enum drm_connector_status
vpox_connector_detect(struct drm_connector *connector, bool force)
{
	struct vpox_connector *vpox_connector;

	vpox_connector = to_vpox_connector(connector);

	return vpox_connector->mode_hint.disconnected ?
	    connector_status_disconnected : connector_status_connected;
}

static int vpox_fill_modes(struct drm_connector *connector, u32 max_x,
			   u32 max_y)
{
	struct vpox_connector *vpox_connector;
	struct drm_device *dev;
	struct drm_display_mode *mode, *iterator;

	vpox_connector = to_vpox_connector(connector);
	dev = vpox_connector->base.dev;
	list_for_each_entry_safe(mode, iterator, &connector->modes, head) {
		list_del(&mode->head);
		drm_mode_destroy(dev, mode);
	}

	return drm_helper_probe_single_connector_modes(connector, max_x, max_y);
}

static const struct drm_connector_helper_funcs vpox_connector_helper_funcs = {
	.mode_valid = vpox_mode_valid,
	.get_modes = vpox_get_modes,
	.best_encoder = vpox_best_single_encoder,
};

static const struct drm_connector_funcs vpox_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = vpox_connector_detect,
	.fill_modes = vpox_fill_modes,
	.destroy = vpox_connector_destroy,
};

static int vpox_connector_init(struct drm_device *dev,
			       struct vpox_crtc *vpox_crtc,
			       struct drm_encoder *encoder)
{
	struct vpox_connector *vpox_connector;
	struct drm_connector *connector;

	vpox_connector = kzalloc(sizeof(*vpox_connector), GFP_KERNEL);
	if (!vpox_connector)
		return -ENOMEM;

	connector = &vpox_connector->base;
	vpox_connector->vpox_crtc = vpox_crtc;

	drm_connector_init(dev, connector, &vpox_connector_funcs,
			   DRM_MODE_CONNECTOR_VGA);
	drm_connector_helper_add(connector, &vpox_connector_helper_funcs);

	connector->interlace_allowed = 0;
	connector->doublescan_allowed = 0;

#if RTLNX_VER_MIN(3,19,0) || RTLNX_RHEL_MAJ_PREREQ(7,2)
	drm_mode_create_suggested_offset_properties(dev);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.suggested_x_property, 0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.suggested_y_property, 0);
#endif
#if RTLNX_VER_MAX(3,17,0) && !RTLNX_RHEL_MAJ_PREREQ(7,2)
	drm_sysfs_connector_add(connector);
#else
	drm_connector_register(connector);
#endif

#if RTLNX_VER_MIN(4,19,0) || RTLNX_RHEL_MAJ_PREREQ(7,7) || RTLNX_RHEL_MAJ_PREREQ(8,1) || RTLNX_SUSE_MAJ_PREREQ(15,1) || RTLNX_SUSE_MAJ_PREREQ(12,5)
	drm_connector_attach_encoder(connector, encoder);
#else
	drm_mode_connector_attach_encoder(connector, encoder);
#endif

	return 0;
}

int vpox_mode_init(struct drm_device *dev)
{
	struct vpox_private *vpox = dev->dev_private;
	struct drm_encoder *encoder;
	struct vpox_crtc *vpox_crtc;
	unsigned int i;
	int ret;

	/* vpox_cursor_init(dev); */
	for (i = 0; i < vpox->num_crtcs; ++i) {
		vpox_crtc = vpox_crtc_init(dev, i);
		if (!vpox_crtc)
			return -ENOMEM;
		encoder = vpox_encoder_init(dev, i);
		if (!encoder)
			return -ENOMEM;
		ret = vpox_connector_init(dev, vpox_crtc, encoder);
		if (ret)
			return ret;
	}

	return 0;
}

void vpox_mode_fini(struct drm_device *dev)
{
	/* vpox_cursor_fini(dev); */
}

/**
 * Copy the ARGB image and generate the mask, which is needed in case the host
 * does not support ARGB cursors.  The mask is a 1BPP bitmap with the bit set
 * if the corresponding alpha value in the ARGB image is greater than 0xF0.
 */
static void copy_cursor_image(u8 *src, u8 *dst, u32 width, u32 height,
			      size_t mask_size)
{
	size_t line_size = (width + 7) / 8;
	u32 i, j;

	memcpy(dst + mask_size, src, width * height * 4);
	for (i = 0; i < height; ++i)
		for (j = 0; j < width; ++j)
			if (((u32 *)src)[i * width + j] > 0xf0000000)
				dst[i * line_size + j / 8] |= (0x80 >> (j % 8));
}

static int vpox_cursor_set2(struct drm_crtc *crtc, struct drm_file *file_priv,
			    u32 handle, u32 width, u32 height,
			    s32 hot_x, s32 hot_y)
{
	struct vpox_private *vpox = crtc->dev->dev_private;
	struct vpox_crtc *vpox_crtc = to_vpox_crtc(crtc);
	struct ttm_bo_kmap_obj uobj_map;
	size_t data_size, mask_size;
	struct drm_gem_object *obj;
	u32 flags, caps = 0;
	struct vpox_bo *bo;
	bool src_isiomem;
	u8 *dst = NULL;
	u8 *src;
	int ret;

	if (!handle) {
		bool cursor_enabled = false;
		struct drm_crtc *crtci;

		/* Hide cursor. */
		vpox_crtc->cursor_enabled = false;
		list_for_each_entry(crtci, &vpox->dev->mode_config.crtc_list,
				    head) {
			if (to_vpox_crtc(crtci)->cursor_enabled)
				cursor_enabled = true;
			}

		if (!cursor_enabled)
			VPoxHGSMIUpdatePointerShape(vpox->guest_pool, 0, 0, 0,
						   0, 0, NULL, 0);
		return 0;
	}

	vpox_crtc->cursor_enabled = true;

	if (width > VPOX_MAX_CURSOR_WIDTH || height > VPOX_MAX_CURSOR_HEIGHT ||
	    width == 0 || height == 0)
		return -EINVAL;
	ret = VPoxQueryConfHGSMI(vpox->guest_pool,
				VPOX_VBVA_CONF32_CURSOR_CAPABILITIES, &caps);
	if (ret)
		return ret == VERR_NO_MEMORY ? -ENOMEM : -EINVAL;

	if (!(caps & VPOX_VBVA_CURSOR_CAPABILITY_HARDWARE)) {
		/*
		 * -EINVAL means cursor_set2() not supported, -EAGAIN means
		 * retry at once.
		 */
		return -EBUSY;
	}

#if RTLNX_VER_MIN(4,7,0) || RTLNX_RHEL_MAJ_PREREQ(7,4)
	obj = drm_gem_object_lookup(file_priv, handle);
#else
	obj = drm_gem_object_lookup(crtc->dev, file_priv, handle);
#endif
	if (!obj) {
		DRM_ERROR("Cannot find cursor object %x for crtc\n", handle);
		return -ENOENT;
	}

	bo = gem_to_vpox_bo(obj);
	ret = vpox_bo_reserve(bo, false);
	if (ret)
		goto out_unref_obj;

	/*
	 * The mask must be calculated based on the alpha
	 * channel, one bit per ARGB word, and must be 32-bit
	 * padded.
	 */
	mask_size = ((width + 7) / 8 * height + 3) & ~3;
	data_size = width * height * 4 + mask_size;
	vpox->cursor_hot_x = hot_x;
	vpox->cursor_hot_y = hot_y;
	vpox->cursor_width = width;
	vpox->cursor_height = height;
	vpox->cursor_data_size = data_size;
	dst = vpox->cursor_data;

#if RTLNX_VER_MIN(5,14,0) || RTLNX_RHEL_RANGE(8,6, 8,99)
	ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.resource->num_pages, &uobj_map);
#elif RTLNX_VER_MIN(5,12,0) || RTLNX_RHEL_MAJ_PREREQ(8,5)
	ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.mem.num_pages, &uobj_map);
#else
	ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &uobj_map);
#endif
	if (ret) {
		vpox->cursor_data_size = 0;
		goto out_unreserve_bo;
	}

	src = ttm_kmap_obj_virtual(&uobj_map, &src_isiomem);
	if (src_isiomem) {
		DRM_ERROR("src cursor bo not in main memory\n");
		ret = -EIO;
		goto out_unmap_bo;
	}

	copy_cursor_image(src, dst, width, height, mask_size);

	flags = VPOX_MOUSE_POINTER_VISIBLE | VPOX_MOUSE_POINTER_SHAPE |
		VPOX_MOUSE_POINTER_ALPHA;
	ret = VPoxHGSMIUpdatePointerShape(vpox->guest_pool, flags,
					 vpox->cursor_hot_x, vpox->cursor_hot_y,
					 width, height, dst, data_size);
	ret = ret == VINF_SUCCESS ? 0 : ret == VERR_NO_MEMORY ? -ENOMEM :
		ret == VERR_NOT_SUPPORTED ? -EBUSY : -EINVAL;

out_unmap_bo:
	ttm_bo_kunmap(&uobj_map);
out_unreserve_bo:
	vpox_bo_unreserve(bo);
out_unref_obj:
#if RTLNX_VER_MIN(5,9,0) || RTLNX_RHEL_MIN(8,4) || RTLNX_SUSE_MAJ_PREREQ(15,3)
	drm_gem_object_put(obj);
#else
	drm_gem_object_put_unlocked(obj);
#endif

	return ret;
}

static int vpox_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct vpox_private *vpox = crtc->dev->dev_private;
	s32 crtc_x =
	    vpox->single_framebuffer ? crtc->x : to_vpox_crtc(crtc)->x_hint;
	s32 crtc_y =
	    vpox->single_framebuffer ? crtc->y : to_vpox_crtc(crtc)->y_hint;
	int ret;

	x += vpox->cursor_hot_x;
	y += vpox->cursor_hot_y;
	if (x + crtc_x < 0 || y + crtc_y < 0 ||
		x + crtc_x >= vpox->input_mapping_width ||
		y + crtc_y >= vpox->input_mapping_width ||
		vpox->cursor_data_size == 0)
		return 0;
	ret = VPoxHGSMICursorPosition(vpox->guest_pool, true, x + crtc_x,
					 y + crtc_y, NULL, NULL);
	return ret == VINF_SUCCESS ? 0 : ret == VERR_NO_MEMORY ? -ENOMEM : ret ==
		VERR_NOT_SUPPORTED ? -EBUSY : -EINVAL;
}
