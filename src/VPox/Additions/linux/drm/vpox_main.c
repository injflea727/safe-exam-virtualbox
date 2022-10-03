/* $Id: vpox_main.c $ */
/** @file
 * VirtualPox Additions Linux kernel video driver
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 * This file is based on ast_main.c
 * Copyright 2012 Red Hat Inc.
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
 * Authors: Dave Airlie <airlied@redhat.com>,
 *          Michael Thayer <michael.thayer@oracle.com,
 *          Hans de Goede <hdegoede@redhat.com>
 */
#include "vpox_drv.h"
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>

#include <VPoxVideoGuest.h>
#include <VPoxVideoVBE.h>

#include "hgsmi_channels.h"

static void vpox_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct vpox_framebuffer *vpox_fb = to_vpox_framebuffer(fb);

	if (vpox_fb->obj)
#if RTLNX_VER_MIN(5,9,0) || RTLNX_RHEL_MIN(8,4) || RTLNX_SUSE_MAJ_PREREQ(15,3)
		drm_gem_object_put(vpox_fb->obj);
#else
		drm_gem_object_put_unlocked(vpox_fb->obj);
#endif

	drm_framebuffer_cleanup(fb);
	kfree(fb);
}

void vpox_enable_accel(struct vpox_private *vpox)
{
	unsigned int i;
	struct VBVABUFFER *vbva;

	if (!vpox->vbva_info || !vpox->vbva_buffers) {
		/* Should never happen... */
		DRM_ERROR("vpoxvideo: failed to set up VBVA.\n");
		return;
	}

	for (i = 0; i < vpox->num_crtcs; ++i) {
		if (vpox->vbva_info[i].pVBVA)
			continue;

		vbva = (void __force *)vpox->vbva_buffers +
			i * VBVA_MIN_BUFFER_SIZE;
		if (!VPoxVBVAEnable(&vpox->vbva_info[i],
				 vpox->guest_pool, vbva, i)) {
			/* very old host or driver error. */
			DRM_ERROR("vpoxvideo: vbva_enable failed\n");
			return;
		}
	}
}

void vpox_disable_accel(struct vpox_private *vpox)
{
	unsigned int i;

	for (i = 0; i < vpox->num_crtcs; ++i)
		VPoxVBVADisable(&vpox->vbva_info[i], vpox->guest_pool, i);
}

void vpox_report_caps(struct vpox_private *vpox)
{
	u32 caps = VBVACAPS_DISABLE_CURSOR_INTEGRATION |
		   VBVACAPS_IRQ | VBVACAPS_USE_VBVA_ONLY;

	if (vpox->initial_mode_queried)
		caps |= VBVACAPS_VIDEO_MODE_HINTS;

	VPoxHGSMISendCapsInfo(vpox->guest_pool, caps);
}

/**
 * Send information about dirty rectangles to VBVA.  If necessary we enable
 * VBVA first, as this is normally disabled after a change of master in case
 * the new master does not send dirty rectangle information (is this even
 * allowed?)
 */
void vpox_framebuffer_dirty_rectangles(struct drm_framebuffer *fb,
				       struct drm_clip_rect *rects,
				       unsigned int num_rects)
{
	struct vpox_private *vpox = fb->dev->dev_private;
	struct drm_crtc *crtc;
	unsigned int i;

	/* The user can send rectangles, we do not need the timer. */
	vpox->need_refresh_timer = false;
	mutex_lock(&vpox->hw_mutex);
	list_for_each_entry(crtc, &fb->dev->mode_config.crtc_list, head) {
		if (CRTC_FB(crtc) != fb)
			continue;

		for (i = 0; i < num_rects; ++i) {
			VBVACMDHDR cmd_hdr;
			unsigned int crtc_id = to_vpox_crtc(crtc)->crtc_id;

			if ((rects[i].x1 > crtc->x + crtc->hwmode.hdisplay) ||
			    (rects[i].y1 > crtc->y + crtc->hwmode.vdisplay) ||
			    (rects[i].x2 < crtc->x) ||
			    (rects[i].y2 < crtc->y))
				continue;

			cmd_hdr.x = (s16)rects[i].x1;
			cmd_hdr.y = (s16)rects[i].y1;
			cmd_hdr.w = (u16)rects[i].x2 - rects[i].x1;
			cmd_hdr.h = (u16)rects[i].y2 - rects[i].y1;

			if (!VPoxVBVABufferBeginUpdate(&vpox->vbva_info[crtc_id],
						      vpox->guest_pool))
				continue;

			VPoxVBVAWrite(&vpox->vbva_info[crtc_id], vpox->guest_pool,
				   &cmd_hdr, sizeof(cmd_hdr));
			VPoxVBVABufferEndUpdate(&vpox->vbva_info[crtc_id]);
		}
	}
	mutex_unlock(&vpox->hw_mutex);
}

static int vpox_user_framebuffer_dirty(struct drm_framebuffer *fb,
				       struct drm_file *file_priv,
				       unsigned int flags, unsigned int color,
				       struct drm_clip_rect *rects,
				       unsigned int num_rects)
{
	vpox_framebuffer_dirty_rectangles(fb, rects, num_rects);

	return 0;
}

static const struct drm_framebuffer_funcs vpox_fb_funcs = {
	.destroy = vpox_user_framebuffer_destroy,
	.dirty = vpox_user_framebuffer_dirty,
};

int vpox_framebuffer_init(struct drm_device *dev,
			  struct vpox_framebuffer *vpox_fb,
#if RTLNX_VER_MIN(4,5,0) || RTLNX_RHEL_MAJ_PREREQ(7,3)
			  const struct DRM_MODE_FB_CMD *mode_cmd,
#else
			  struct DRM_MODE_FB_CMD *mode_cmd,
#endif
			  struct drm_gem_object *obj)
{
	int ret;

#if RTLNX_VER_MIN(4,11,0) || RTLNX_RHEL_MAJ_PREREQ(7,5)
	drm_helper_mode_fill_fb_struct(dev, &vpox_fb->base, mode_cmd);
#else
	drm_helper_mode_fill_fb_struct(&vpox_fb->base, mode_cmd);
#endif
	vpox_fb->obj = obj;
	ret = drm_framebuffer_init(dev, &vpox_fb->base, &vpox_fb_funcs);
	if (ret) {
		DRM_ERROR("framebuffer init failed %d\n", ret);
		return ret;
	}

	return 0;
}

static struct drm_framebuffer *vpox_user_framebuffer_create(
		struct drm_device *dev,
		struct drm_file *filp,
#if RTLNX_VER_MIN(4,5,0) || RTLNX_RHEL_MAJ_PREREQ(7,3)
		const struct drm_mode_fb_cmd2 *mode_cmd)
#else
		struct drm_mode_fb_cmd2 *mode_cmd)
#endif
{
	struct drm_gem_object *obj;
	struct vpox_framebuffer *vpox_fb;
	int ret = -ENOMEM;

#if RTLNX_VER_MIN(4,7,0) || RTLNX_RHEL_MAJ_PREREQ(7,4)
	obj = drm_gem_object_lookup(filp, mode_cmd->handles[0]);
#else
	obj = drm_gem_object_lookup(dev, filp, mode_cmd->handles[0]);
#endif
	if (!obj)
		return ERR_PTR(-ENOENT);

	vpox_fb = kzalloc(sizeof(*vpox_fb), GFP_KERNEL);
	if (!vpox_fb)
		goto err_unref_obj;

	ret = vpox_framebuffer_init(dev, vpox_fb, mode_cmd, obj);
	if (ret)
		goto err_free_vpox_fb;

	return &vpox_fb->base;

err_free_vpox_fb:
	kfree(vpox_fb);
err_unref_obj:
#if RTLNX_VER_MIN(5,9,0) || RTLNX_RHEL_MIN(8,4) || RTLNX_SUSE_MAJ_PREREQ(15,3)
	drm_gem_object_put(obj);
#else
	drm_gem_object_put_unlocked(obj);
#endif
	return ERR_PTR(ret);
}

static const struct drm_mode_config_funcs vpox_mode_funcs = {
	.fb_create = vpox_user_framebuffer_create,
};

#if RTLNX_VER_MAX(4,0,0) && !RTLNX_RHEL_MAJ_PREREQ(7,3)
# define pci_iomap_range(dev, bar, offset, maxlen) \
	ioremap(pci_resource_start(dev, bar) + (offset), maxlen)
#endif

/**
 * Tell the host about the views.  This design originally targeted the
 * Windows XP driver architecture and assumed that each screen would
 * have a dedicated frame buffer with the command buffer following it,
 * the whole being a "view".  The host works out which screen a command
 * buffer belongs to by checking whether it is in the first view, then
 * whether it is in the second and so on.  The first match wins.  We
 * cheat around this by making the first view be the managed memory
 * plus the first command buffer, the second the same plus the second
 * buffer and so on.
 */
static int vpox_set_views(struct vpox_private *vpox)
{
	VBVAINFOVIEW *p;
	int i;

	p = VPoxHGSMIBufferAlloc(vpox->guest_pool, sizeof(*p),
			       HGSMI_CH_VBVA, VBVA_INFO_VIEW);
	if (!p)
		return -ENOMEM;

	for (i = 0; i < vpox->num_crtcs; ++i) {
		p->u32ViewIndex = i;
		p->u32ViewOffset = 0;
		p->u32ViewSize = vpox->available_vram_size +
			i * VBVA_MIN_BUFFER_SIZE;
		p->u32MaxScreenSize = vpox->available_vram_size;

		VPoxHGSMIBufferSubmit(vpox->guest_pool, p);
	}

	VPoxHGSMIBufferFree(vpox->guest_pool, p);

	return 0;
}

static int vpox_accel_init(struct vpox_private *vpox)
{
	unsigned int i, ret;

	vpox->vbva_info = devm_kcalloc(vpox->dev->dev, vpox->num_crtcs,
				       sizeof(*vpox->vbva_info), GFP_KERNEL);
	if (!vpox->vbva_info)
		return -ENOMEM;

	/* Take a command buffer for each screen from the end of usable VRAM. */
	vpox->available_vram_size -= vpox->num_crtcs * VBVA_MIN_BUFFER_SIZE;

	vpox->vbva_buffers = pci_iomap_range(VPOX_DRM_TO_PCI_DEV(vpox->dev), 0,
					     vpox->available_vram_size,
					     vpox->num_crtcs *
					     VBVA_MIN_BUFFER_SIZE);
	if (!vpox->vbva_buffers)
		return -ENOMEM;

	for (i = 0; i < vpox->num_crtcs; ++i)
		VPoxVBVASetupBufferContext(&vpox->vbva_info[i],
					  vpox->available_vram_size +
					  i * VBVA_MIN_BUFFER_SIZE,
					  VBVA_MIN_BUFFER_SIZE);

	vpox_enable_accel(vpox);
	ret = vpox_set_views(vpox);
	if (ret)
		goto err_pci_iounmap;

	return 0;

err_pci_iounmap:
	pci_iounmap(VPOX_DRM_TO_PCI_DEV(vpox->dev), vpox->vbva_buffers);
	return ret;
}

static void vpox_accel_fini(struct vpox_private *vpox)
{
	vpox_disable_accel(vpox);
	pci_iounmap(VPOX_DRM_TO_PCI_DEV(vpox->dev), vpox->vbva_buffers);
}

/** Do we support the 4.3 plus mode hint reporting interface? */
static bool have_hgsmi_mode_hints(struct vpox_private *vpox)
{
	u32 have_hints, have_cursor;
	int ret;

	ret = VPoxQueryConfHGSMI(vpox->guest_pool,
			       VPOX_VBVA_CONF32_MODE_HINT_REPORTING,
			       &have_hints);
	if (ret)
		return false;

	ret = VPoxQueryConfHGSMI(vpox->guest_pool,
			       VPOX_VBVA_CONF32_GUEST_CURSOR_REPORTING,
			       &have_cursor);
	if (ret)
		return false;

	return have_hints == VINF_SUCCESS && have_cursor == VINF_SUCCESS;
}

/**
 * Our refresh timer call-back.  Only used for guests without dirty rectangle
 * support.
 */
static void vpox_refresh_timer(struct work_struct *work)
{
	struct vpox_private *vpox = container_of(work, struct vpox_private,
												 refresh_work.work);
	bool have_unblanked = false;
	struct drm_crtc *crtci;

	if (!vpox->need_refresh_timer)
		return;
	list_for_each_entry(crtci, &vpox->dev->mode_config.crtc_list, head) {
		struct vpox_crtc *vpox_crtc = to_vpox_crtc(crtci);
		if (crtci->enabled && !vpox_crtc->blanked)
			have_unblanked = true;
	}
	if (!have_unblanked)
		return;
	/* This forces a full refresh. */
	vpox_enable_accel(vpox);
	/* Schedule the next timer iteration. */
	schedule_delayed_work(&vpox->refresh_work, VPOX_REFRESH_PERIOD);
}

static bool vpox_check_supported(u16 id)
{
	u16 dispi_id;

	vpox_write_ioport(VBE_DISPI_INDEX_ID, id);
	dispi_id = inw(VBE_DISPI_IOPORT_DATA);

	return dispi_id == id;
}

/**
 * Set up our heaps and data exchange buffers in VRAM before handing the rest
 * to the memory manager.
 */
static int vpox_hw_init(struct vpox_private *vpox)
{
	int ret = -ENOMEM;

	vpox->full_vram_size = inl(VBE_DISPI_IOPORT_DATA);
	vpox->any_pitch = vpox_check_supported(VBE_DISPI_ID_ANYX);

	DRM_INFO("VRAM %08x\n", vpox->full_vram_size);

	/* Map guest-heap at end of vram */
	vpox->guest_heap =
	    pci_iomap_range(VPOX_DRM_TO_PCI_DEV(vpox->dev), 0, GUEST_HEAP_OFFSET(vpox),
			    GUEST_HEAP_SIZE);
	if (!vpox->guest_heap)
		return -ENOMEM;

	/* Create guest-heap mem-pool use 2^4 = 16 byte chunks */
	vpox->guest_pool = gen_pool_create(4, -1);
	if (!vpox->guest_pool)
		goto err_unmap_guest_heap;

	ret = gen_pool_add_virt(vpox->guest_pool,
				(unsigned long)vpox->guest_heap,
				GUEST_HEAP_OFFSET(vpox),
				GUEST_HEAP_USABLE_SIZE, -1);
	if (ret)
		goto err_destroy_guest_pool;

	/* Reduce available VRAM size to reflect the guest heap. */
	vpox->available_vram_size = GUEST_HEAP_OFFSET(vpox);
	/* Linux drm represents monitors as a 32-bit array. */
	VPoxQueryConfHGSMI(vpox->guest_pool, VPOX_VBVA_CONF32_MONITOR_COUNT,
			 &vpox->num_crtcs);
	vpox->num_crtcs = clamp_t(u32, vpox->num_crtcs, 1, VPOX_MAX_SCREENS);

	if (!have_hgsmi_mode_hints(vpox)) {
		ret = -ENOTSUPP;
		goto err_destroy_guest_pool;
	}

	vpox->last_mode_hints = devm_kcalloc(vpox->dev->dev, vpox->num_crtcs,
					     sizeof(VBVAMODEHINT),
					     GFP_KERNEL);
	if (!vpox->last_mode_hints) {
		ret = -ENOMEM;
		goto err_destroy_guest_pool;
	}

	ret = vpox_accel_init(vpox);
	if (ret)
		goto err_destroy_guest_pool;

	/* Set up the refresh timer for users which do not send dirty rectangles. */
	INIT_DELAYED_WORK(&vpox->refresh_work, vpox_refresh_timer);

	return 0;

err_destroy_guest_pool:
	gen_pool_destroy(vpox->guest_pool);
err_unmap_guest_heap:
	pci_iounmap(VPOX_DRM_TO_PCI_DEV(vpox->dev), vpox->guest_heap);
	return ret;
}

static void vpox_hw_fini(struct vpox_private *vpox)
{
	vpox->need_refresh_timer = false;
	cancel_delayed_work(&vpox->refresh_work);
	vpox_accel_fini(vpox);
	gen_pool_destroy(vpox->guest_pool);
	pci_iounmap(VPOX_DRM_TO_PCI_DEV(vpox->dev), vpox->guest_heap);
}

#if RTLNX_VER_MIN(4,19,0) || RTLNX_RHEL_MIN(8,3)
int vpox_driver_load(struct drm_device *dev)
#else
int vpox_driver_load(struct drm_device *dev, unsigned long flags)
#endif
{
	struct vpox_private *vpox;
	int ret = 0;

	if (!vpox_check_supported(VBE_DISPI_ID_HGSMI))
		return -ENODEV;

	vpox = devm_kzalloc(dev->dev, sizeof(*vpox), GFP_KERNEL);
	if (!vpox)
		return -ENOMEM;

	dev->dev_private = vpox;
	vpox->dev = dev;

	mutex_init(&vpox->hw_mutex);

	ret = vpox_hw_init(vpox);
	if (ret)
		return ret;

	ret = vpox_mm_init(vpox);
	if (ret)
		goto err_hw_fini;

	drm_mode_config_init(dev);

	dev->mode_config.funcs = (void *)&vpox_mode_funcs;
	dev->mode_config.min_width = 64;
	dev->mode_config.min_height = 64;
	dev->mode_config.preferred_depth = 24;
	dev->mode_config.max_width = VBE_DISPI_MAX_XRES;
	dev->mode_config.max_height = VBE_DISPI_MAX_YRES;

	ret = vpox_mode_init(dev);
	if (ret)
		goto err_drm_mode_cleanup;

	ret = vpox_irq_init(vpox);
	if (ret)
		goto err_mode_fini;

	ret = vpox_fbdev_init(dev);
	if (ret)
		goto err_irq_fini;

	return 0;

err_irq_fini:
	vpox_irq_fini(vpox);
err_mode_fini:
	vpox_mode_fini(dev);
err_drm_mode_cleanup:
	drm_mode_config_cleanup(dev);
	vpox_mm_fini(vpox);
err_hw_fini:
	vpox_hw_fini(vpox);
	return ret;
}

#if RTLNX_VER_MIN(4,11,0) || RTLNX_RHEL_MAJ_PREREQ(7,5)
void vpox_driver_unload(struct drm_device *dev)
#else
int vpox_driver_unload(struct drm_device *dev)
#endif
{
	struct vpox_private *vpox = dev->dev_private;

	vpox_fbdev_fini(dev);
	vpox_irq_fini(vpox);
	vpox_mode_fini(dev);
	drm_mode_config_cleanup(dev);
	vpox_mm_fini(vpox);
	vpox_hw_fini(vpox);
#if RTLNX_VER_MAX(4,11,0) && !RTLNX_RHEL_MAJ_PREREQ(7,5)
	return 0;
#endif
}

/**
 * @note this is described in the DRM framework documentation.  AST does not
 * have it, but we get an oops on driver unload if it is not present.
 */
void vpox_driver_lastclose(struct drm_device *dev)
{
	struct vpox_private *vpox = dev->dev_private;

#if RTLNX_VER_MIN(3,16,0) || RTLNX_RHEL_MAJ_PREREQ(7,1)
	if (vpox->fbdev)
		drm_fb_helper_restore_fbdev_mode_unlocked(&vpox->fbdev->helper);
#else
	drm_modeset_lock_all(dev);
	if (vpox->fbdev)
		drm_fb_helper_restore_fbdev_mode(&vpox->fbdev->helper);
	drm_modeset_unlock_all(dev);
#endif
}

int vpox_gem_create(struct drm_device *dev,
		    u32 size, bool iskernel, struct drm_gem_object **obj)
{
	struct vpox_bo *vpoxbo;
	int ret;

	*obj = NULL;

	size = roundup(size, PAGE_SIZE);
	if (size == 0)
	{
		DRM_ERROR("bad size\n");
		return -EINVAL;
	}

	ret = vpox_bo_create(dev, size, 0, 0, &vpoxbo);
	if (ret) {
		if (ret != -ERESTARTSYS)
			DRM_ERROR("failed to allocate GEM object\n");
		DRM_ERROR("failed to allocate GEM (%d)\n", ret);
		return ret;
	}

	*obj = &vpoxbo->gem;

	return 0;
}

int vpox_dumb_create(struct drm_file *file,
		     struct drm_device *dev, struct drm_mode_create_dumb *args)
{
	int ret;
	struct drm_gem_object *gobj;
	u32 handle;

	args->pitch = args->width * ((args->bpp + 7) / 8);
	args->size = args->pitch * args->height;

	ret = vpox_gem_create(dev, args->size, false, &gobj);
	if (ret)
		return ret;

	ret = drm_gem_handle_create(file, gobj, &handle);
#if RTLNX_VER_MIN(5,9,0) || RTLNX_RHEL_MIN(8,4) || RTLNX_SUSE_MAJ_PREREQ(15,3)
	drm_gem_object_put(gobj);
#else
	drm_gem_object_put_unlocked(gobj);
#endif
	if (ret)
		return ret;

	args->handle = handle;

	return 0;
}

#if RTLNX_VER_MAX(3,12,0) && !RTLNX_RHEL_MAJ_PREREQ(7,3)
int vpox_dumb_destroy(struct drm_file *file,
		      struct drm_device *dev, u32 handle)
{
	return drm_gem_handle_delete(file, handle);
}
#endif

#if RTLNX_VER_MAX(4,19,0) && !RTLNX_RHEL_MAJ_PREREQ(7,7) && !RTLNX_RHEL_MAJ_PREREQ(8,1) && !RTLNX_SUSE_MAJ_PREREQ(15,1) && !RTLNX_SUSE_MAJ_PREREQ(12,5)
static void ttm_bo_put(struct ttm_buffer_object *bo)
{
	ttm_bo_unref(&bo);
}
#endif

void vpox_gem_free_object(struct drm_gem_object *obj)
{
	struct vpox_bo *vpox_bo = gem_to_vpox_bo(obj);

#if RTLNX_VER_MIN(5,14,0) || RTLNX_RHEL_RANGE(8,6, 8,99)
	/* Starting from kernel 5.14, there is a warning appears in dmesg
	 * on attempt to desroy pinned buffer object. Make sure it is unpinned. */
	while (vpox_bo->bo.pin_count)
	{
		int ret;
		ret = vpox_bo_unpin(vpox_bo);
		if (ret)
		{
			DRM_ERROR("unable to unpin buffer object\n");
			break;
		}
	}
#endif

	ttm_bo_put(&vpox_bo->bo);
}

static inline u64 vpox_bo_mmap_offset(struct vpox_bo *bo)
{
#if RTLNX_VER_MIN(5,4,0) || RTLNX_RHEL_MIN(8,3) || RTLNX_SUSE_MAJ_PREREQ(15,3)
        return drm_vma_node_offset_addr(&bo->bo.base.vma_node);
#elif RTLNX_VER_MAX(3,12,0) && !RTLNX_RHEL_MAJ_PREREQ(7,0)
	return bo->bo.addr_space_offset;
#else
	return drm_vma_node_offset_addr(&bo->bo.vma_node);
#endif /* >= 5.4.0 */
}

int
vpox_dumb_mmap_offset(struct drm_file *file,
		      struct drm_device *dev,
		      u32 handle, u64 *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;
	struct vpox_bo *bo;

	mutex_lock(&dev->struct_mutex);
#if RTLNX_VER_MIN(4,7,0) || RTLNX_RHEL_MAJ_PREREQ(7,4)
	obj = drm_gem_object_lookup(file, handle);
#else
	obj = drm_gem_object_lookup(dev, file, handle);
#endif
	if (!obj) {
		ret = -ENOENT;
		goto out_unlock;
	}

	bo = gem_to_vpox_bo(obj);
	*offset = vpox_bo_mmap_offset(bo);

#if RTLNX_VER_MIN(5,14,0) || RTLNX_RHEL_RANGE(8,6, 8,99)
	ret = drm_vma_node_allow(&bo->bo.base.vma_node, file);
	if (ret)
	{
		DRM_ERROR("unable to grant previladges to user");
	}
#endif

	drm_gem_object_put(obj);

out_unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
