/*
 * Copyright 2003 Red Hat, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86fbman.h"

#ifdef XF86DRI
#include "xf86drm.h"
#endif

#include "via_driver.h"
#include "via_swov.h"
#ifdef XF86DRI
#include "via_drm.h"
#endif

/*
 *	Isolate the wonders of X memory allocation and DRI memory allocation
 *	and 4.3 or 4.4 differences in one abstraction.
 *
 *	The pool code indicates who provided the memory:
 *	0  -  nobody
 *	1  -  xf86 linear
 *	2  -  DRM
 */
int
viaOffScreenLinear(struct buffer_object *obj, ScrnInfoPtr pScrn,
                   unsigned long size)
{
    int depth = pScrn->bitsPerPixel >> 3;
    VIAPtr pVia = VIAPTR(pScrn);
    FBLinearPtr linear;

    linear = xf86AllocateOffscreenLinear(pScrn->pScreen,
                                        (size + depth - 1) / depth,
                                        32, NULL, NULL, NULL);
    if (!linear)
        return BadAlloc;
    obj->offset = linear->offset * depth;
    obj->handle = (unsigned long) linear;
    obj->domain = TTM_PL_SYSTEM;
    obj->size = size;
    return Success;
}

struct buffer_object *
drm_bo_alloc(ScrnInfoPtr pScrn, unsigned int size, int domain)
{
    struct buffer_object *obj = NULL;
    VIAPtr pVia = VIAPTR(pScrn);

    obj = xnfcalloc(1, sizeof(*obj));
    if (obj) {
        switch (domain) {
        case TTM_PL_VRAM:
        case TTM_PL_TT:
#ifdef XF86DRI
            if (pVia->directRenderingType == DRI_1) {
                drm_via_mem_t drm;
                int ret;

                drm.context = DRIGetContext(pScrn->pScreen);
                drm.size = size;
                drm.type = (domain == TTM_PL_TT ? VIA_MEM_AGP : VIA_MEM_VIDEO);
                ret = drmCommandWriteRead(pVia->drmFD, DRM_VIA_ALLOCMEM,
                                            &drm, sizeof(drm_via_mem_t));
                if (!ret && (size == drm.size)) {
                    obj->offset = drm.offset;
                    obj->handle = drm.index;
                    obj->domain = domain;
                    obj->size = drm.size;
                    DEBUG(ErrorF("%lu of DRI memory allocated at %lx, handle %lu\n",
                                obj->size, obj->offset, obj->handle));
                    break;
                } else
                    DEBUG(ErrorF("DRM memory allocation failed %d\n", ret));
            }
#endif
        case TTM_PL_SYSTEM:
        default:
            if (Success != viaOffScreenLinear(obj, pScrn, size)) {
                ErrorF("Linear memory allocation failed\n");
                free(obj);
                obj = NULL;
            }
            break;
        }
    }
    return obj;
}

void*
drm_bo_map(ScrnInfoPtr pScrn, struct buffer_object *obj)
{
    VIAPtr pVia = VIAPTR(pScrn);

    switch (obj->domain) {
    case TTM_PL_TT:
        obj->ptr = pVia->agpMappedAddr + obj->offset;
        break;
    case TTM_PL_VRAM:
    default:
        obj->ptr = pVia->FBBase + obj->offset;
        break;
    }
    return obj->ptr;
}

void
drm_bo_unmap(ScrnInfoPtr pScrn, struct buffer_object *obj)
{
    obj->ptr = NULL;
}

void
drm_bo_free(ScrnInfoPtr pScrn, struct buffer_object *obj)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if (obj) {
        DEBUG(ErrorF("Freed %lu (pool %d)\n", obj->offset, obj->domain));
        switch (obj->domain) {
        case TTM_PL_SYSTEM: {
                FBLinearPtr linear = (FBLinearPtr) obj->handle;

                xf86FreeOffscreenLinear(linear);
            }
            break;
        case TTM_PL_VRAM:
        case TTM_PL_TT:
#ifdef XF86DRI
            if (pVia->directRenderingType == DRI_1) {
                drm_via_mem_t drm;

                drm.index = obj->handle;
                if (drmCommandWrite(pVia->drmFD, DRM_VIA_FREEMEM,
                                    &drm, sizeof(drm_via_mem_t)) < 0)
                    ErrorF("DRM failed to free for handle %lld.\n", obj->handle);
            }
#endif
            break;
        }
        free(obj);
    }
}

Bool
drm_bo_manager_init(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    Bool ret = ums_create(pScrn);

#ifdef XF86DRI
    if (pVia->directRenderingType == DRI_1)
        ret = VIADRIKernelInit(pScrn);
#endif
    return ret;
}
