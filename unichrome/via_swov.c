/*
 * Copyright 2004-2005 The Unichrome Project  [unichrome.sf.net]
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
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
 
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"

#include "via.h"
#include "xf86drm.h"

#include "via_driver.h"
#include "via_priv.h"
#include "via_swov.h"
#include "via_drm.h"
#include "via_vgahw.h"
#include "via_id.h"

#include <math.h>

/*
 * Warning: this file contains revision checks which are CLE266 specific.
 * There seems to be no checking present for KM400 or more recent devices.
 *
 * TODO:
 *   - pVia->Chipset checking of course.
 *   - move content of pVia->HWDiff into pVia->swov
 *   - merge with CLEXF40040
 */

#define   XV_DEBUG      1     /* write log msg to /var/log/XFree86.0.log */

#ifdef XV_DEBUG
# define DBG_DD(x) (x)
#else
# define DBG_DD(x)
#endif

/*
 * Old via_regrec code.
 *
 *
 */
#define VIDREG_BUFFER_SIZE  100  /* Number of entries in the VidRegBuffer. */
#define IN_VIDEO_DISPLAY     (*((unsigned long volatile *)(pVia->VidMapBase+V_FLAGS))&VBI_STATUS)
#if 0

#define IN_HQV_FIRE     (*((unsigned long volatile *)(pVia->VidMapBase+HQV_CONTROL))&HQV_IDLE)
#ifdef UNUSED

#define IN_VIDEO_FIRE   (*((unsigned long volatile *)(pVia->VidMapBase+V_COMPOSE_MODE))&V1_COMMAND_FIRE)
#define IN_HQV_FLIP     (*((unsigned long volatile *)(pVia->VidMapBase+HQV_CONTROL))&HQV_FLIP_STATUS)
#define IN_DISPLAY      (VIDInD(V_FLAGS) & 0x200)
#define IN_VBLANK       (!IN_DISPLAY)

static void 
viaWaitHQVIdle(VIAPtr pVia)
{
    while (!IN_HQV_FIRE);
}
#endif /* UNUSED */

#endif

static void 
viaWaitVideoCommandFire(VIAPtr pVia)
{
    /*while (IN_VIDEO_FIRE);*/
    CARD32 volatile *pdwState = (CARD32 volatile *) (pVia->VidMapBase+V_COMPOSE_MODE);
    /*pdwState = (CARD32 volatile *) (pVia->VidMapBase+V_COMPOSE_MODE);*/
    while ((*pdwState & V1_COMMAND_FIRE)||(*pdwState & V3_COMMAND_FIRE));
}

static void 
viaWaitHQVFlip(VIAPtr pVia)
{
    unsigned long proReg=0;
    CARD32 volatile *pdwState;

    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(pVia->swov.gdwVideoFlagSW & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;

    pdwState = (CARD32 volatile *) (pVia->VidMapBase+(HQV_CONTROL|proReg));
    while (!(*pdwState & HQV_FLIP_STATUS) );
/*
    while (!((*pdwState & 0xc0)== 0xc0) );
    while (!((*pdwState & 0xc0)!= 0xc0) );
*/    
}

static void
viaWaitHQVFlipClear(VIAPtr pVia, unsigned long dwData)
{
    CARD32 volatile *pdwState = (CARD32 volatile *) (pVia->VidMapBase+(HQV_CONTROL));
    *pdwState =dwData;

    while ((*pdwState & HQV_FLIP_STATUS) )
    {
        VIDOutD(HQV_CONTROL, *pdwState|HQV_FLIP_STATUS);
    }
}

static void
viaWaitVBI(VIAPtr pVia)
{
    while (IN_VIDEO_DISPLAY);
}

static void
viaWaitHQVDone(VIAPtr pVia)
{
    CARD32 volatile *pdwState;
    unsigned long proReg=0;
    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(pVia->swov.gdwVideoFlagSW & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;


    pdwState = (CARD32 volatile *) (pVia->VidMapBase+(HQV_CONTROL|proReg));
    /*pdwState = (CARD32 volatile *) (GEAddr+HQV_CONTROL);*/

    /*if (*pdwState & HQV_ENABLE)*/
    if (pVia->swov.MPEG_ON)
    {
        while ((*pdwState & HQV_SW_FLIP) );
    }
}
/*
 * Send all data in VidRegBuffer to the hardware.
 */
static void
FlushVidRegBuffer(VIAPtr pVia)
{
    unsigned int i;
    
    viaWaitVideoCommandFire(pVia);

    for (i = 0; i < pVia->VidRegCursor; i += 2)
    {
        VIDOutD(pVia->VidRegBuffer[i], pVia->VidRegBuffer[i+1]);
        DBG_DD(ErrorF("FlushVideoRegs: [%i] %08lx %08lx\n",
            i>>1, pVia->VidRegBuffer[i]+0x200, pVia->VidRegBuffer[i+1]));
    }

    /* BUG: (?) VIA never resets the cursor. 
     * My fix is commented out for now, in case they had a reason for that. /A
     */
    /* pVia->VidRegCursor = 0; */
}

/*
 * Initialize and clear VidRegBuffer.
 */
static void
ResetVidRegBuffer(VIAPtr pVia)
{
    /* BUG: (Memory leak) This allocation may need have a corresponding free somewhere... /A */
    if (!pVia->VidRegBuffer)
        pVia->VidRegBuffer = xnfcalloc(VIDREG_BUFFER_SIZE, sizeof(CARD32) * 2);
    pVia->VidRegCursor = 0;
}

/*
 * Save a video register and data in VidRegBuffer.
 */
static void
SaveVideoRegister(VIAPtr pVia, CARD32 index, CARD32 data)
{
    pVia->VidRegBuffer[pVia->VidRegCursor++] = index;
    pVia->VidRegBuffer[pVia->VidRegCursor++] = data;

    if (pVia->VidRegCursor > VIDREG_BUFFER_SIZE) {
        DBG_DD(ErrorF("SaveVideoRegister: Out of video register space"));
    }
}


/*
 * HW Difference Flag
 * Moved here from via_hwdiff.c
 *
 * These are the entries of HWDiff used in our code (currently):
 *                     CLE266Ax   CLE266Cx   KM400     K8M800    PM800
 * ThreeHQVBuffer      FALSE      TRUE       TRUE      TRUE      TRUE
 * HQVFetchByteUnit    FALSE      TRUE       TRUE      TRUE      TRUE
 * SupportTwoColorKey  FALSE      TRUE       FALSE     FALSE     TRUE
 * HQVInitPatch        TRUE       FALSE      FALSE     FALSE     FALSE
 * HQVDisablePatch     FALSE      TRUE       TRUE      TRUE      FALSE
 *
 * This is now up to date with CLEXF40040. All unused entries were removed.
 * The functions depending on this struct are untouched. 
 *
 */
void 
VIAVidHWDiffInit(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIAHWDiff *HWDiff = &pVia->HWDiff;

    switch(pVia->Chipset)
    {
    case VIA_CLE266:
        if (CLE266_REV_IS_AX(pVia->ChipRev)) {
            HWDiff->dwThreeHQVBuffer = VID_HWDIFF_FALSE;
            HWDiff->dwHQVFetchByteUnit = VID_HWDIFF_FALSE;
            HWDiff->dwSupportTwoColorKey = VID_HWDIFF_FALSE;
            HWDiff->dwHQVInitPatch = VID_HWDIFF_TRUE;
            HWDiff->dwHQVDisablePatch = VID_HWDIFF_FALSE;
        } else {
            HWDiff->dwThreeHQVBuffer = VID_HWDIFF_TRUE;
            HWDiff->dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
            HWDiff->dwSupportTwoColorKey = VID_HWDIFF_TRUE;
            HWDiff->dwHQVInitPatch = VID_HWDIFF_FALSE;
            HWDiff->dwHQVDisablePatch = VID_HWDIFF_TRUE;
        }
        break;
    case VIA_KM400:
        HWDiff->dwThreeHQVBuffer = VID_HWDIFF_TRUE;
        HWDiff->dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
        HWDiff->dwSupportTwoColorKey = VID_HWDIFF_FALSE;
        HWDiff->dwHQVInitPatch = VID_HWDIFF_FALSE;
        HWDiff->dwHQVDisablePatch = VID_HWDIFF_TRUE;
        break;
#ifdef HAVE_K8M800
    case VIA_K8M800:
        HWDiff->dwThreeHQVBuffer = VID_HWDIFF_TRUE;
        HWDiff->dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
        HWDiff->dwSupportTwoColorKey = VID_HWDIFF_FALSE;
        HWDiff->dwHQVInitPatch = VID_HWDIFF_FALSE;
        HWDiff->dwHQVDisablePatch = VID_HWDIFF_TRUE;
        break;
#endif /* HAVE_K8M800 */
#ifdef HAVE_PM800
    case VIA_PM800:
        HWDiff->dwThreeHQVBuffer = VID_HWDIFF_TRUE;
        HWDiff->dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
        HWDiff->dwSupportTwoColorKey = VID_HWDIFF_TRUE;
        HWDiff->dwHQVInitPatch = VID_HWDIFF_FALSE;
        HWDiff->dwHQVDisablePatch = VID_HWDIFF_FALSE;
        break;
#endif /* HAVE_PM800 */
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "VIAVidHWDiffInit: Unhandled ChipSet.\n");
    }
}     
/*
 * Old via_overlay code.
 *
 */
typedef struct _YCBCRREC  {
  CARD32  dwY ;
  CARD32  dwCB;
  CARD32  dwCR;
} YCBCRREC;

/* Verify that using V1 bit definitions on V3
 * is not broken in OverlayGetV1V3Format()
 */

#if V1_COLORSPACE_SIGN != V3_COLORSPACE_SIGN
#error "V1_COLORSPACE_SIGN != V3_COLORSPACE_SIGN"
#endif
#if V1_YUV422 != V3_YUV422
#error "V1_YUV422 != V3_YUV422"
#endif
#if V1_SWAP_HW_HQV != V3_SWAP_HW_HQV
#error "V1_SWAP_HW_HQV != V3_SWAP_HW_HQV"
#endif
#if V1_RGB15 != V3_RGB15
#error "V1_RGB15 != V3_RGB15"
#endif
#if V1_RGB16 != V3_RGB16
#error "V1_RGB16 != V3_RGB16"
#endif
#if V1_RGB32 != V3_RGB32
#error "V1_RGB32 != V3_RGB32"
#endif

static BOOL
viaOverlayGetV1V3Format(VIAPtr pVia, int vport, /* 1 or 3, as in V1 or V3 */
			unsigned long videoFlag,
			unsigned long * pVidCtl, unsigned long * pHQVCtl)
{
    BOOL is_ok = TRUE;

    *pVidCtl |= V1_COLORSPACE_SIGN;
    switch (pVia->swov.SrcFourCC) {
    case FOURCC_YV12:
    case FOURCC_XVMC:
	if (videoFlag & VIDEO_HQV_INUSE) {
	    *pVidCtl |= V1_YUV422 | V1_SWAP_HW_HQV;
	    *pHQVCtl |= HQV_SRC_SW | HQV_YUV420 | HQV_ENABLE | HQV_SW_FLIP;
	} else {
	    if (vport == 1)
		*pVidCtl |= V1_YCbCr420;
	    else {
		DBG_DD(ErrorF("viaOverlayGetV1V3Format: V3 does not support planar YUV.\n"));
		is_ok = FALSE;
	    }
	}
	break;
	
    case FOURCC_YUY2:
	if (videoFlag & VIDEO_HQV_INUSE) {
	    *pVidCtl |= V1_YUV422 | V1_SWAP_HW_HQV;
	    *pHQVCtl |= HQV_SRC_SW | HQV_YUV422 | HQV_ENABLE | HQV_SW_FLIP;
	} else
	    *pVidCtl |= V1_YUV422;
	break;
	
    default:
	DBG_DD(ErrorF("viaOverlayGetV1V3Format: Invalid FOURCC format (0x%lx).\n",
		      pVia->swov.SrcFourCC));
	is_ok = FALSE;
	
	*pVidCtl |= V1_YUV422;
	break;
    }
    return is_ok;
}

static unsigned long
viaOverlayGetSrcStartAddress(VIAPtr pVia, unsigned long videoFlag,
			     LPDDUPDATEOVERLAY pUpdate, unsigned long srcPitch,
			     unsigned long * pHQVoffset)
{
    unsigned long srcWidth  = (unsigned long)(pUpdate->SrcRight - pUpdate->SrcLeft);
    unsigned long dstWidth  = (unsigned long)(pUpdate->DstRight - pUpdate->DstLeft);
    unsigned long srcHeight = (unsigned long)(pUpdate->SrcBottom - pUpdate->SrcTop);
    unsigned long dstHeight = (unsigned long)(pUpdate->DstBottom - pUpdate->DstTop);

    unsigned long offset = 0;
    unsigned long srcTopOffset = 0;
    unsigned long srcLeftOffset = 0;

    if ((pUpdate->SrcLeft != 0) || (pUpdate->SrcTop != 0)) {
	switch (pVia->swov.SrcFourCC) {
	case FOURCC_YUY2:
	case FOURCC_UYVY:
	    
	    if (videoFlag & VIDEO_HQV_INUSE) {
		offset = (((pUpdate->SrcTop & ~3) * srcPitch) + ((pUpdate->SrcLeft << 1) & ~31));
		
		if (srcHeight > dstHeight)
		    srcTopOffset = ((pUpdate->SrcTop & ~3) * dstHeight / srcHeight) * srcPitch;
		else
		    srcTopOffset = (pUpdate->SrcTop & ~3) * srcPitch;
		
		if (srcWidth > dstWidth)
		    srcLeftOffset = ((pUpdate->SrcLeft << 1) & ~31) * dstWidth / srcWidth;
		else
		    srcLeftOffset = (pUpdate->SrcLeft << 1) & ~31;
		*pHQVoffset = srcTopOffset + srcLeftOffset;
	    } else
		offset = ((pUpdate->SrcTop * srcPitch) + ((pUpdate->SrcLeft << 1) & ~15));
	    break;
	    
	case FOURCC_YV12:
	case FOURCC_XVMC:
	    
	    if (videoFlag & VIDEO_HQV_INUSE)
		offset = (((pUpdate->SrcTop & ~3) * (srcPitch << 1)) + ((pUpdate->SrcLeft << 1) & ~31));
	    else {
		offset = ((((pUpdate->SrcTop & ~3) * srcPitch) + pUpdate->SrcLeft) & ~31);
		if (pUpdate->SrcTop > 0)
		    pVia->swov.overlayRecordV1.dwUVoffset = (((((pUpdate->SrcTop & ~3)>>1) * srcPitch) + pUpdate->SrcLeft) & ~31) >> 1;
		else
		    pVia->swov.overlayRecordV1.dwUVoffset = offset >> 1;
	    }
	    break;
	    
	default:
	    DBG_DD(ErrorF("viaGetSrcStartAddress: Invalid FOURCC format (0x%lx).\n",
			  pVia->swov.SrcFourCC));
	    break;
	}
    }
    else {
        pVia->swov.overlayRecordV1.dwUVoffset = offset = 0;
    }

    return offset;
}

static YCBCRREC
viaOverlayGetYCbCrStartAddress(unsigned long videoFlag, unsigned long startAddr,
			       unsigned long offset, unsigned long UVoffset,
			       unsigned long srcPitch, unsigned long srcHeight)
{
    YCBCRREC YCbCr;

    if (videoFlag & VIDEO_HQV_INUSE) {
        YCbCr.dwY  = startAddr;
        YCbCr.dwCB = startAddr + srcPitch * srcHeight;
        YCbCr.dwCR = startAddr + srcPitch * srcHeight
            + srcPitch * (srcHeight >> 2);
    }
    else {
        YCbCr.dwY  = startAddr + offset;
        YCbCr.dwCB = startAddr + srcPitch * srcHeight + UVoffset;
        YCbCr.dwCR = startAddr + srcPitch * srcHeight + UVoffset
            + srcPitch * (srcHeight >> 2);
    }
    return YCbCr;
}

static YCBCRREC
viaOverlayGetYCbCrStartAddressProA(unsigned long videoFlag, unsigned long startAddr,
			       unsigned long offset, unsigned long UVoffset,
			       unsigned long srcPitch, unsigned long srcHeight)
{
    YCBCRREC YCbCr;
    
    if (videoFlag & VIDEO_HQV_INUSE) {
        YCbCr.dwY  = startAddr;
        YCbCr.dwCB = startAddr + srcPitch * srcHeight;
	YCbCr.dwCR = startAddr + srcPitch * srcHeight;
    }
    else {
        YCbCr.dwY  = startAddr + offset;
        YCbCr.dwCB = startAddr + srcPitch * srcHeight + UVoffset;
        YCbCr.dwCR = startAddr + srcPitch * srcHeight + UVoffset;
    }
    return YCbCr;
}

static unsigned long
viaOverlayHQVCalcZoomWidth(VIAPtr pVia, unsigned long videoFlag,
			   unsigned long srcWidth, unsigned long dstWidth,
			   unsigned long * pZoomCtl, unsigned long * pMiniCtl,
			   unsigned long * pHQVfilterCtl, unsigned long * pHQVminiCtl,
			   unsigned long * pHQVzoomflag)
{
    unsigned long tmp, sw1, d, falign, mdiv;
    Bool zoom_ok = TRUE;

    CARD32 HQVfilter[5] = {
        HQV_H_FILTER_DEFAULT, HQV_H_TAP4_121, HQV_H_TAP4_121,
        HQV_H_TAP8_12221, HQV_H_TAP8_12221
    };
    /* CARD HQVmini[5] = { 0, 0xc00, 0xa00, 0x900, 0x8800 }; */

    falign = 0;
    mdiv = 1;

    if (srcWidth == dstWidth) { /* No zoom */
        *pHQVfilterCtl |= HQV_H_FILTER_DEFAULT;
    }
    else if (srcWidth < dstWidth) { /* Zoom in */

        tmp = srcWidth * 0x800 / dstWidth;
        *pZoomCtl = ((tmp & 0x7ff) << 16) | V1_X_ZOOM_ENABLE;
        *pMiniCtl |= V1_X_INTERPOLY;
        zoom_ok = !(tmp > 0x7ff);

        *pHQVzoomflag = 1;
        *pHQVfilterCtl |= HQV_H_FILTER_DEFAULT;

    } else { /* srcWidth > dstWidth - Zoom out */

        /*HQV rounding patch
        //tmp = dstWidth*0x0800 / srcWidth;*/
        tmp = dstWidth * 0x800 * 0x400 / srcWidth;
        tmp = tmp / 0x400 + ((tmp & 0x3ff) ? 1 : 0);

        *pHQVminiCtl = (tmp & 0x7ff) | HQV_H_MINIFY_ENABLE | HQV_H_MINIFY_DOWN;

        /* Scale down the picture by a factor mdiv = (1 << d) = {2, 4, 8 or 16} */

        sw1 = srcWidth;
        for (d = 1; d < 5; d++) {
            sw1 >>= 1;
            if (sw1 <= dstWidth) break;
        }
        if (d == 5) { /* Too small. */
            d = 4;
            zoom_ok = FALSE;
        }
        mdiv = 1 << d;                  /* <= {2,4,8,16} */
        falign = ((mdiv<<1)-1) & 0xf;   /* <= {3,7,15,15} */
        *pMiniCtl |= V1_X_INTERPOLY;
        *pMiniCtl |= ((d<<1)-1) << 24;  /* <= {1,3,5,7} << 24 */

        *pHQVfilterCtl |= HQVfilter[d];
        /* *pHQVminiCtl = HQVmini[d]; */
        *pHQVminiCtl |= HQV_HDEBLOCK_FILTER;

        /* Scale to arbitrary size, on top of previous scaling by (1 << d). */

        if (sw1 < dstWidth) {
            /* CLE bug
            *pZoomCtl = sw1 * 0x0800 / dstWidth;*/
            *pZoomCtl = (sw1 - 2) * 0x0800 / dstWidth;
            *pZoomCtl = ((*pZoomCtl & 0x7ff) << 16) | V1_X_ZOOM_ENABLE;
        }
    }

    if (videoFlag & VIDEO_1_INUSE) {
        pVia->swov.overlayRecordV1.dwFetchAlignment = falign;
        pVia->swov.overlayRecordV1.dwminifyH = mdiv;
    }
    else {
        pVia->swov.overlayRecordV3.dwFetchAlignment = falign;
        pVia->swov.overlayRecordV3.dwminifyH = mdiv;
    }

    return zoom_ok;
}

static unsigned long 
viaOverlayHQVCalcZoomHeight(VIAPtr pVia, unsigned long srcHeight,
			    unsigned long dstHeight, unsigned long * pZoomCtl,
			    unsigned long * pMiniCtl, unsigned long * pHQVfilterCtl,
			    unsigned long * pHQVminiCtl, unsigned long * pHQVzoomflag)
{
    unsigned long tmp, sh1, d;
    Bool zoom_ok = TRUE;

    CARD32 HQVfilter[5] = {
        HQV_V_TAP4_121, HQV_V_TAP4_121, HQV_V_TAP4_121,
        HQV_V_TAP8_12221, HQV_V_TAP8_12221
    };
    /* CARD32 HQVmini[5] = { 0, 0x0c000000, 0x0a000000, 0x09000000, 0x08800000 }; */

    /*if (pVia->pBIOSInfo->scaleY)
    {
        dstHeight = dstHeight + 1;
    }*/

    if (srcHeight == dstHeight) { /* No zoom */
        *pHQVfilterCtl |= HQV_V_TAP4_121;
    }
    else if (srcHeight < dstHeight) { /* Zoom in */

        tmp = srcHeight * 0x0400 / dstHeight;
        *pZoomCtl |= ((tmp & 0x3ff) | V1_Y_ZOOM_ENABLE);
        *pMiniCtl |= (V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY);

        *pHQVzoomflag = 1;
        *pHQVfilterCtl |= HQV_V_TAP4_121;
    } 
    else { /* srcHeight > dstHeight - Zoom out */

        /*HQV rounding patch
        //tmp = dstHeight*0x0800 / srcHeight;*/
        tmp = dstHeight * 0x0800 * 0x400 / srcHeight;
        tmp = tmp / 0x400 + ((tmp & 0x3ff) ? 1 : 0);
        *pHQVminiCtl |= ((tmp& 0x7ff) << 16) | HQV_V_MINIFY_ENABLE | HQV_V_MINIFY_DOWN;

        /* Scale down the picture by a factor (1 << d) = {2, 4, 8 or 16} */

        sh1 = srcHeight;
        for (d = 1; d < 5; d++) {
            sh1 >>= 1;
            if (sh1 <= dstHeight) break;
        }
        if (d == 5) { /* Too small. */
            d = 4;
            zoom_ok = FALSE;
        }

        *pMiniCtl |= ((d<<1)-1) << 16;  /* <= {1,3,5,7} << 16 */

        *pHQVfilterCtl |= HQVfilter[d];
        /* *pHQVminiCtl |= HQVmini[d]; */
        *pHQVminiCtl |= HQV_VDEBLOCK_FILTER;

        /* Scale to arbitrary size, on top of previous scaling by (1 << d). */

        if (sh1 < dstHeight) {
            tmp = sh1 * 0x0400 / dstHeight;
            *pZoomCtl |= ((tmp & 0x3ff) | V1_Y_ZOOM_ENABLE);
            *pMiniCtl |= V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY;
        }
    }

    return zoom_ok;
}

static unsigned long 
viaOverlayGetFetch(VIAPtr pVia, unsigned long videoFlag,
		   unsigned long srcWidth, unsigned long dstWidth,
		   unsigned long oriSrcWidth, unsigned long * pHQVsrcFetch)
{
    unsigned long fetch = 0;
    int n = 2; /* 2^n bytes per pixel. */

    switch (pVia->swov.SrcFourCC) {
    case FOURCC_YV12:
    case FOURCC_XVMC:
	n = 0; /* 2^n = 1 byte per pixel (Y channel in planar YUV) */
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
	n = 1; /* 2^n = 2 bytes per pixel (packed YUV) */
	break;
    default:
	DBG_DD(ErrorF("viaOverlayGetFetch: Invalid FOURCC format (0x%lx).\n",
		      pVia->swov.SrcFourCC));
	break;
    }

    if (videoFlag & VIDEO_HQV_INUSE) {
        *pHQVsrcFetch = oriSrcWidth << n;
        if (n == 0) {
            /* Assume n == 0 <=> Planar YUV.
             * The V1/V3 pixelformat is always packed YUV when we use HQV,
             * so we switch from 8-bit to 16-bit pixels here.
             */
            n = 1;
        }
        if (dstWidth >= srcWidth)
            fetch = (ALIGN_TO(srcWidth << n, 16) >> 4) + 1;
        else
            fetch = (ALIGN_TO(dstWidth << n, 16) >> 4) + 1;
    } else {
        if (n == 0)
            fetch = (ALIGN_TO(srcWidth, 32) >> 4);
        else
            fetch = (ALIGN_TO(srcWidth << n, 16) >> 4) + 1;
    }

    /* Fix planar mode problem. */
    if (fetch < 4) fetch = 4;

    return fetch;
}

/*
 * This function uses quadratic mapping to adjust the midpoint of the scaling.
 */
static float rangeEqualize(float inLow,float inHigh,float outLow,float outHigh,float outMid,
                           float inValue)
{
    float
        inRange = inHigh - inLow,
        outRange = outHigh - outLow,
        normIn = ((inValue - inLow) / inRange)*2.-1.,
        delta = outMid - outRange*0.5 - outLow;
    return (inValue - inLow) * outRange / inRange + outLow + (1. - normIn*normIn)*delta;
}

static unsigned vPackFloat(float val, float hiLimit, float loLimit, float mult, int shift, 
                           Bool doSign)
{
    unsigned packed,mask,sign;
    val = (val > hiLimit) ? hiLimit : val;
    val = (val < loLimit) ? loLimit : val;
    sign = (val < 0) ? 1:0;
    val = (sign) ? -val : val;
    packed = ((unsigned)(val*mult + 1.)) >> 1;
    mask = (1 << shift) - 1;
    return (((packed >= mask) ? mask : packed) | ((doSign) ? (sign << shift) : 0));

}


typedef float colorCoeff[5];
static colorCoeff colorCTable[] = {{1.1875,1.625,0.875,0.375,2.0},
                                   {1.164,1.596,0.54,0.45,2.2}};

/*
 * This function is a partial rewrite of the overlay.c file of the original VIA drivers,
 * which was extremely nasty and difficult to follow. Coefficient for new chipset models should
 * be added in the table above and, if needed, implemented in the model switch below.
 */

static void
viaCalculateVideoColor(VIAPtr pVia, int hue, int saturation, int brightness, 
		       int contrast,Bool reset,CARD32 *col1,CARD32 *col2)
{
    float fA,fB1,fC1,fD,fB2,fC2,fB3,fC3;
    float fPI,fContrast,fSaturation,fHue,fBrightness;
    const float *mCoeff; 
    unsigned long dwA,dwB1,dwC1,dwD,dwB2,dwC2,dwB3,dwC3,dwS;
    unsigned long dwD_Int,dwD_Dec;
    int intD;
    int model;
    fPI = (float)(M_PI/180.);    

    if ( reset ) {
        saturation = 10000;
        brightness = 5000;
        contrast = 10000;
    }

    switch ( pVia->ChipId )
    {
    case PCI_CHIP_VT3205:
    case PCI_CHIP_VT3204:
    case PCI_CHIP_VT3259:
        model = 0;
        break;
    case PCI_CHIP_CLE3122:
        model = (CLE266_REV_IS_CX(pVia->ChipRev) ? 0 : 1); 
        break;
    default:
        ErrorF("Unknown Chip ID\n");
        model = 0;
    }

    switch( model )
    {
    case 0:
        fBrightness = rangeEqualize(0.,10000.,-128.,128.,-16.,(float) brightness);
        fContrast = rangeEqualize(0.,20000.,0.,1.6645,1.0,(float) contrast);
        fSaturation = rangeEqualize(0.,20000,0.,2.,1.,(float) saturation);
        break;
    default:
        fBrightness = rangeEqualize(0.,10000.,-128.,128.,-12.,(float) brightness);
        fContrast = rangeEqualize(0.,20000.,0.,1.6645,1.1,(float) contrast);
        fSaturation = rangeEqualize(0.,20000,0.,2.,1.15,(float) saturation);
        break;
    }
    fHue = (float)hue;

    mCoeff = colorCTable[model];

    fA  = (float)(mCoeff[0]*fContrast);
    fB1 = (float)(-mCoeff[1]*fContrast*fSaturation*sin(fHue*fPI));
    fC1 = (float)(mCoeff[1]*fContrast*fSaturation*cos(fHue*fPI));
    fD  = (float)(mCoeff[0]*(fBrightness));
    fB2 = (float)((mCoeff[2]*sin(fHue*fPI)-
        mCoeff[3]*cos(fHue*fPI))*fContrast*fSaturation);
    fC2 = (float)(-(mCoeff[2]*cos(fHue*fPI)+
        mCoeff[3]*sin(fHue*fPI))*fContrast*fSaturation);
    fB3 = (float)(mCoeff[4]*fContrast*fSaturation*cos(fHue*fPI));
    fC3 = (float)(mCoeff[4]*fContrast*fSaturation*sin(fHue*fPI));

    switch(model)
    {
    case 0:
        dwA = vPackFloat(fA,1.9375,0.,32.,5,0);
        dwB1 = vPackFloat(fB1,2.125,-2.125,16.,5,1);
        dwC1 = vPackFloat(fC1,2.125,-2.125,16.,5,1);

        if (fD>=0) {
            intD=(int)fD;
            if (intD>127)
                intD = 127;
            dwD_Int = ((unsigned long)intD)&0xff;
            dwD = ((unsigned long)(fD*16+1))>>1;
            dwD_Dec= dwD&0x7;
        } else {
            intD=(int)fD;
            if (intD< -128)
                intD = -128;
            intD = intD+256; 
            dwD_Int = ((unsigned long)intD)&0xff;
            fD = -fD;
            dwD = ((unsigned long)(fD*16+1))>>1;
            dwD_Dec= dwD&0x7;
        }

        dwB2 = vPackFloat(fB2,1.875,-1.875,16,4,1);
        dwC2 = vPackFloat(fC2,1.875,-1.875,16,4,1);
        dwB3 = vPackFloat(fB3,3.875,-3.875,16,5,1);
        dwC3 = vPackFloat(fC3,3.875,-3.875,16,5,1);
        *col1 = (dwA<<24)|(dwB1<<16)|(dwC1<<8)|dwD_Int;
        *col2 = (dwD_Dec<<29|dwB2<<24)|(dwC2<<16)|(dwB3<<8)|(dwC3);
        break;

    default:
        dwA = vPackFloat(fA,1.9375,-0.,32,5,0);
        dwB1 = vPackFloat(fB1,0.75,-0.75,8.,2,1);
        dwC1 = vPackFloat(fC1,2.875,1.,16.,5,0);

        if (fD>=127)
            fD=127;

        if (fD<=-128)
            fD=-128;

        if (fD>=0) {
            dwS = 0;
        }
        else {
            dwS = 1;
            fD = fD+128;
        }

        dwD = ((unsigned long)(fD*2+1))>>1;
        if (dwD>=0x7f) {
            dwD = 0x7f|(dwS<<7);
        } else {
            dwD = (dwD&0x7f)|(dwS<<7);
        }

        dwB2 = vPackFloat(fB2,0.,-0.875,16.,3,0); 
        dwC2 = vPackFloat(fC2,0.,-1.875,16.,4,0); 
        dwB3 = vPackFloat(fB3,3.75,0.,8.,4,0);
        dwC3 = vPackFloat(fC3,1.25,-1.25,8.,3,1);
        *col1 = (dwA<<24)|(dwB1<<18)|(dwC1<<9)|dwD;
        *col2 = (dwB2<<25)|(dwC2<<17)|(dwB3<<10)|(dwC3<<2);
        break;
    }
}

/*
 *
 *
 */
void viaSetColorSpace(VIAPtr pVia, int hue, int saturation, int brightness, int contrast,
                      Bool reset)
{
    CARD32 col1,col2;

    viaCalculateVideoColor(pVia,hue,saturation,brightness, contrast,reset,&col1,&col2);
    switch ( pVia->ChipId ) {
    case PCI_CHIP_VT3205:
    case PCI_CHIP_VT3204:
        VIDOutD(V3_ColorSpaceReg_1, col1);
        VIDOutD(V3_ColorSpaceReg_2, col2);
        DBG_DD(ErrorF("000002C4 %08lx\n",col1));
        DBG_DD(ErrorF("000002C8 %08lx\n",col2));
        break;
    case PCI_CHIP_CLE3122:
    case PCI_CHIP_VT3259:
        VIDOutD(V1_ColorSpaceReg_2, col2);
        VIDOutD(V1_ColorSpaceReg_1, col1);
        VIDOutD(V3_ColorSpaceReg_2, col2);
        VIDOutD(V3_ColorSpaceReg_1, col1);

        DBG_DD(ErrorF("00000288 %08lx\n",col2));
        DBG_DD(ErrorF("00000284 %08lx\n",col1));
        break;
    default:
/*IVOR*/
        DBG_DD(ErrorF("000002C4 %08lx\n",col1));
        DBG_DD(ErrorF("000002C8 %08lx\n",col2));

        break;
    }
}

static unsigned long ViaInitVideoStatusFlag(VIAPtr pVia) 
{
    switch ( pVia->ChipId ) {
    case PCI_CHIP_VT3205:
    case PCI_CHIP_VT3204:
    case PCI_CHIP_VT3259:
        return VIDEO_HQV_INUSE | SW_USE_HQV | VIDEO_3_INUSE;
    case PCI_CHIP_CLE3122:
        return VIDEO_HQV_INUSE | SW_USE_HQV | VIDEO_1_INUSE;
    default:
        DBG_DD(ErrorF("Unknown DeviceID\n"));
        break;
    }
    return 0UL;
}

static unsigned long ViaSetVidCtl(VIAPtr pVia, unsigned int videoFlag)
{
    if (videoFlag & VIDEO_1_INUSE) {
        /*=* Modify for C1 FIFO *=*/
        /* WARNING: not checking Chipset! */
        if (CLE266_REV_IS_CX(pVia->ChipRev))
            return V1_ENABLE | V1_EXPIRE_NUM_F;
        else {
            /* Overlay source format for V1 */
            if (pVia->swov.gdwUseExtendedFIFO)
                return V1_ENABLE | V1_EXPIRE_NUM_A | V1_FIFO_EXTENDED;
            else
                return V1_ENABLE | V1_EXPIRE_NUM;
        }
    }
    else {
        switch (pVia->ChipId)
        {
        case PCI_CHIP_VT3205:
        case PCI_CHIP_VT3204:	
            return V3_ENABLE | V3_EXPIRE_NUM_3205;

        case PCI_CHIP_CLE3122:
            if (CLE266_REV_IS_CX(pVia->ChipRev)) 
                return V3_ENABLE | V3_EXPIRE_NUM_F;
            else 
                return V3_ENABLE | V3_EXPIRE_NUM;
            break;

        case PCI_CHIP_VT3259:
                return V3_ENABLE | V3_EXPIRE_NUM_3205;
                break;

        default:
            DBG_DD(ErrorF("Unknown DeviceID\n"));
            break;
        }
    }
    return 0;
}/*End of DeviceID*/

/*
 * Fill the buffer with 0x8000 (YUV2 black)
 */
static void
ViaYUVFillBlack(VIAPtr pVia, int offset, int num)
{
    CARD16 *ptr = (CARD16 *)(pVia->FBBase + offset);

    while(num-- > 0)
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
        *ptr++ = 0x0080;
#else
        *ptr++ = 0x8000;
#endif
}

/*
 * Add an HQV surface to an existing FOURCC surface.
 * numbuf: number of buffers, 1, 2 or 3
 * fourcc: FOURCC code of the current (already existing) surface
 */
static long AddHQVSurface(ScrnInfoPtr pScrn, unsigned int numbuf, CARD32 fourcc)
{
    unsigned int i, width, height, pitch, fbsize, addr;
    unsigned long retCode;
    BOOL isplanar;

    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 AddrReg[3] = {HQV_DST_STARTADDR0, HQV_DST_STARTADDR1, HQV_DST_STARTADDR2};

    unsigned long proReg=0;
    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(pVia->swov.gdwVideoFlagSW & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;

    isplanar = ((fourcc == FOURCC_YV12) || (fourcc == FOURCC_XVMC));

    width  = pVia->swov.SWDevice.gdwSWSrcWidth;
    height = pVia->swov.SWDevice.gdwSWSrcHeight;
    pitch  = pVia->swov.SWDevice.dwPitch; 
    fbsize = pitch * height * (isplanar ? 2 : 1);

    VIAFreeLinear(&pVia->swov.HQVMem);
    retCode = VIAAllocLinear(&pVia->swov.HQVMem, pScrn, fbsize * numbuf);
    if(retCode != Success) return retCode;
    addr = pVia->swov.HQVMem.base;

    ViaYUVFillBlack(pVia, addr, fbsize);

    for (i = 0; i < numbuf; i++) {
        pVia->swov.overlayRecordV1.dwHQVAddr[i] = addr;
        VIDOutD(AddrReg[i]|proReg, addr);
        addr += fbsize;
    }

    return Success;
}

/*
 * Create a FOURCC surface (Supported: YUY2, YV12 or VIA)
  * doalloc: set true to actually allocate memory for the framebuffers
 */
static long 
CreateSurface(ScrnInfoPtr pScrn, CARD32 FourCC, CARD16 Width,
	      CARD16 Height, BOOL doalloc)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    unsigned long pitch, fbsize, addr;
    unsigned long retCode;
    BOOL isplanar;

    pVia->swov.SrcFourCC = FourCC;
    pVia->swov.gdwVideoFlagSW = ViaInitVideoStatusFlag(pVia);

    isplanar = ((FourCC == FOURCC_YV12) || (FourCC == FOURCC_XVMC));

    pitch  = ALIGN_TO(Width, 32) * (isplanar ? 1 : 2);
    fbsize = pitch * Height * (isplanar ? 1.5 : 1.0);

    VIAFreeLinear(&pVia->swov.SWfbMem);

    if (doalloc) {
        retCode = VIAAllocLinear(&pVia->swov.SWfbMem, pScrn, fbsize * 2);
        if(retCode != Success)
	    return retCode;
        addr = pVia->swov.SWfbMem.base;

        ViaYUVFillBlack(pVia, addr, fbsize);

        pVia->swov.SWDevice.dwSWPhysicalAddr[0]   = addr;
        pVia->swov.SWDevice.dwSWPhysicalAddr[1]   = addr + fbsize;
        pVia->swov.SWDevice.lpSWOverlaySurface[0] = pVia->FBBase + addr;
        pVia->swov.SWDevice.lpSWOverlaySurface[1] =
            pVia->swov.SWDevice.lpSWOverlaySurface[0] + fbsize;

        if (isplanar) {
            pVia->swov.SWDevice.dwSWCrPhysicalAddr[0] =
                pVia->swov.SWDevice.dwSWPhysicalAddr[0] + (pitch*Height);
            pVia->swov.SWDevice.dwSWCrPhysicalAddr[1] =
                pVia->swov.SWDevice.dwSWPhysicalAddr[1] + (pitch*Height);
            pVia->swov.SWDevice.dwSWCbPhysicalAddr[0] =
                pVia->swov.SWDevice.dwSWCrPhysicalAddr[0] + ((pitch>>1)*(Height>>1));
            pVia->swov.SWDevice.dwSWCbPhysicalAddr[1] =
                pVia->swov.SWDevice.dwSWCrPhysicalAddr[1] + ((pitch>>1)*(Height>>1));
        }
    }

    pVia->swov.SWDevice.gdwSWSrcWidth = Width;
    pVia->swov.SWDevice.gdwSWSrcHeight = Height;
    pVia->swov.SWDevice.dwPitch = pitch;

    pVia->swov.overlayRecordV1.dwV1OriWidth = Width;
    pVia->swov.overlayRecordV1.dwV1OriHeight = Height;
    pVia->swov.overlayRecordV1.dwV1OriPitch = pitch;

    return Success;
}

/*
 *
 */
int
ViaSwovSurfaceCreate(ScrnInfoPtr pScrn, viaPortPrivPtr pPriv, CARD32 FourCC,
		     CARD16 Width, CARD16 Height)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    unsigned long retCode = Success;
    int numbuf = pVia->HWDiff.dwThreeHQVBuffer ? 3 : 2;

    DBG_DD(ErrorF("ViaSwovSurfaceCreate: FourCC =0x%08lx\n", FourCC));

    if (pVia->VideoStatus & VIDEO_SWOV_SURFACE_CREATED)
        return Success;

    switch (FourCC) {
    case FOURCC_YUY2:
	retCode = CreateSurface(pScrn, FourCC, Width, Height, TRUE);
        if (retCode != Success)
	    break;
        if ((pVia->swov.gdwVideoFlagSW & SW_USE_HQV))
	    retCode = AddHQVSurface(pScrn, numbuf, FOURCC_YUY2);
        break;

    case FOURCC_HQVSW:
        retCode = AddHQVSurface(pScrn, numbuf, FOURCC_YUY2);
        break;

    case FOURCC_YV12:
        retCode = CreateSurface(pScrn, FourCC, Width, Height, TRUE);
        if (retCode == Success)
            retCode = AddHQVSurface(pScrn, numbuf, FOURCC_YV12);
        break;

    case FOURCC_XVMC:
        retCode = CreateSurface(pScrn, FourCC, Width, Height, FALSE);
        if (retCode == Success)
            retCode = AddHQVSurface(pScrn, numbuf, FOURCC_XVMC);
        break;

    default:
        break;
    }

    if (retCode == Success) {
	pVia->swov.SWDevice.lpSWOverlaySurface[0] = pVia->FBBase + pVia->swov.SWDevice.dwSWPhysicalAddr[0];
	pVia->swov.SWDevice.lpSWOverlaySurface[1] = pVia->FBBase + pVia->swov.SWDevice.dwSWPhysicalAddr[1];
	
	DBG_DD(ErrorF(" lpSWOverlaySurface[0]: %p\n", pVia->swov.SWDevice.lpSWOverlaySurface[0]));
	DBG_DD(ErrorF(" lpSWOverlaySurface[1]: %p\n", pVia->swov.SWDevice.lpSWOverlaySurface[1]));
	
	pVia->VideoStatus |= VIDEO_SWOV_SURFACE_CREATED | VIDEO_SWOV_ON;
    }
    return retCode;
}

/*
 *  Destroy Surface
 */
void
ViaSwovSurfaceDestroy(ScrnInfoPtr pScrn, viaPortPrivPtr pPriv)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    DBG_DD(ErrorF("ViaSwovSurfaceDestroy: FourCC =0x%08lx\n",
		  pPriv->FourCC));

    if (pVia->VideoStatus & VIDEO_SWOV_SURFACE_CREATED) {
        DBG_DD(ErrorF("ViaSwovSurfaceDestroy: VideoStatus =0x%08lx\n",
		      pVia->VideoStatus));
	
	switch (pPriv->FourCC) {
	case FOURCC_YUY2:
	    pVia->swov.SrcFourCC = 0;
	    
	    VIAFreeLinear(&pVia->swov.SWfbMem);
	    if ((pVia->swov.gdwVideoFlagSW & SW_USE_HQV))
		VIAFreeLinear(&pVia->swov.HQVMem);
	    pVia->swov.gdwVideoFlagSW = 0;
	    break;
	    
	case FOURCC_HQVSW:
	    VIAFreeLinear(&pVia->swov.HQVMem);
	    pVia->swov.gdwVideoFlagSW = 0;
	    break;
	    
	case FOURCC_YV12:
	case FOURCC_XVMC:
	    pVia->swov.SrcFourCC = 0;
	    
	    VIAFreeLinear(&pVia->swov.SWfbMem);
	    VIAFreeLinear(&pVia->swov.HQVMem);
	    pVia->swov.gdwVideoFlagSW = 0;
	    break;
	}

	pVia->VideoStatus &= ~VIDEO_SWOV_SURFACE_CREATED;

    } else
        DBG_DD(ErrorF("ViaSwovSurfaceDestroy: No SW Overlay Surface Destroyed,"
		      " VideoStatus =0x%08lx\n", pVia->VideoStatus));
}

static void SetFIFO_V1(VIAPtr pVia, CARD8 depth, CARD8 prethreshold, CARD8 threshold) 
{
    SaveVideoRegister(pVia, V_FIFO_CONTROL, ((depth-1) & 0x7f) |
        ((prethreshold & 0x7f) << 24) | ((threshold & 0x7f) << 8));
}

static void SetFIFO_V3(VIAPtr pVia, CARD8 depth, CARD8 prethreshold, CARD8 threshold)
{
    SaveVideoRegister(pVia, ALPHA_V3_FIFO_CONTROL,
        (VIDInD(ALPHA_V3_FIFO_CONTROL) & ALPHA_FIFO_MASK) |
        ((depth - 1) & 0xff) | ((threshold & 0xff) << 8));
    SaveVideoRegister(pVia, ALPHA_V3_PREFIFO_CONTROL,
        (VIDInD(ALPHA_V3_PREFIFO_CONTROL) & ~V3_FIFO_MASK) |
        (prethreshold & 0x7f));
}

static void SetFIFO_64or32(VIAPtr pVia)
{
    /*=* Modify for C1 FIFO *=*/
    /* WARNING: not checking Chipset! */
    if (CLE266_REV_IS_CX(pVia->ChipRev))
        SetFIFO_V1(pVia, 64, 56, 56);
    else
        SetFIFO_V1(pVia, 32, 29, 16);
}

static void SetFIFO_64or16(VIAPtr pVia)
{
    /*=* Modify for C1 FIFO *=*/
    /* WARNING: not checking Chipset! */
    if (CLE266_REV_IS_CX(pVia->ChipRev))
        SetFIFO_V1(pVia, 64, 56, 56);
    else
        SetFIFO_V1(pVia, 16, 12, 8);
}

static void SetFIFO_64or48or32(VIAPtr pVia)
{
    /*=* Modify for C1 FIFO *=*/
    /* WARNING: not checking Chipset! */
    if (CLE266_REV_IS_CX(pVia->ChipRev))
        SetFIFO_V1(pVia, 64, 56, 56);
    else {
        if (pVia->swov.gdwUseExtendedFIFO)
            SetFIFO_V1(pVia, 48, 40, 40);
        else
            SetFIFO_V1(pVia, 32, 29, 16);
    }                        
}

static void SetFIFO_V3_64or32or32(VIAPtr pVia)
{
    switch (pVia->ChipId)
    {
    case PCI_CHIP_VT3259:
    case PCI_CHIP_VT3205:
    case PCI_CHIP_VT3204:
        SetFIFO_V3(pVia, 32, 29, 29);
        break;

    case PCI_CHIP_CLE3122:
        if (CLE266_REV_IS_CX(pVia->ChipRev))
            SetFIFO_V3(pVia, 64, 56, 56);
        else
            SetFIFO_V3(pVia, 32, 16, 16);
        break;

    default:
        break;
    }
}

static void SetFIFO_V3_64or32or16(VIAPtr pVia)
{
    switch (pVia->ChipId)
    {
    case PCI_CHIP_VT3259:
    case PCI_CHIP_VT3205:
    case PCI_CHIP_VT3204:
        SetFIFO_V3(pVia, 32, 29, 29);
        break;

    case PCI_CHIP_CLE3122:
        if (CLE266_REV_IS_CX(pVia->ChipRev))
            SetFIFO_V3(pVia, 64, 56, 56);
        else
            SetFIFO_V3(pVia, 16, 16, 8);
        break;

    default:
        break;
    }
}

static void
SetupFIFOs(VIAPtr pVia, unsigned long videoFlag, unsigned long miniCtl,
	   unsigned long srcWidth)
{
    if (miniCtl & V1_Y_INTERPOLY)
    {
        if (pVia->swov.SrcFourCC == FOURCC_YV12 ||
            pVia->swov.SrcFourCC == FOURCC_XVMC)
        {
            if (videoFlag & VIDEO_HQV_INUSE)
            {
                if (videoFlag & VIDEO_1_INUSE)
                    SetFIFO_64or32(pVia);
                else
                    SetFIFO_V3_64or32or16(pVia);
            }
            else
            {
                /* Minified video will be skewed without this workaround. */
                if (srcWidth <= 80) /* Fetch count <= 5 */
                {
                    if (videoFlag & VIDEO_1_INUSE)
                        SetFIFO_V1(pVia, 16, 0, 0);
                    else
                        SetFIFO_V3(pVia, 16, 16, 0);
                }
                else
                {
                    if (videoFlag & VIDEO_1_INUSE)
                        SetFIFO_64or16(pVia);
                    else
                        SetFIFO_V3_64or32or16(pVia);
                }
            }
        }
        else
        {
            if (videoFlag & VIDEO_1_INUSE)
                SetFIFO_64or48or32(pVia);
            else
            {
                /* Fix V3 bug. */
                if (srcWidth <= 8)
                    SetFIFO_V3(pVia, 1, 0, 0);
                else
                    SetFIFO_V3_64or32or32(pVia);
            }
        }
    }
    else 
    {
        if (pVia->swov.SrcFourCC == FOURCC_YV12 ||
            pVia->swov.SrcFourCC == FOURCC_XVMC)
        {
            if (videoFlag & VIDEO_HQV_INUSE)
            {
                if (videoFlag & VIDEO_1_INUSE)
                    SetFIFO_64or32(pVia);
                else
                    SetFIFO_V3_64or32or16(pVia);
            }
            else
            {
                /* Minified video will be skewed without this workaround. */
                if (srcWidth <= 80) /* Fetch count <= 5 */
                {
                    if (videoFlag & VIDEO_1_INUSE)
                        SetFIFO_V1(pVia, 16, 0, 0);
                    else
                        SetFIFO_V3(pVia, 16, 16, 0);
                }
                else
                {
                    if (videoFlag & VIDEO_1_INUSE)
                        SetFIFO_64or16(pVia);
                    else
                        SetFIFO_V3_64or32or16(pVia);
                }
            }
        }
        else
        {
            if (videoFlag & VIDEO_1_INUSE)
                SetFIFO_64or48or32(pVia);
            else
            {
                /* Fix V3 bug. */
                if (srcWidth <= 8)
                    SetFIFO_V3(pVia, 1, 0, 0);
                else
                    SetFIFO_V3_64or32or32(pVia);
            }
        }
    }
}

static CARD32 SetColorKey(VIAPtr pVia, unsigned long videoFlag,
                          CARD32 keyLow, CARD32 keyHigh, CARD32 compose)
{
    keyLow &= 0x00FFFFFF;

    keyLow|=0x40000000;

    /*SaveVideoRegister(pVia, V_COLOR_KEY, keyLow);*/

    if (videoFlag & VIDEO_1_INUSE) {
        SaveVideoRegister(pVia, V_COLOR_KEY, keyLow);
    }
    else {
    /* IVOR   if (pVia->HWDiff.dwSupportTwoColorKey )*/    /*CLE_C0*/
            SaveVideoRegister(pVia, V3_COLOR_KEY, keyLow);
    }

    /*compose = (compose & ~0x0f) | SELECT_VIDEO_IF_COLOR_KEY;*/
    /*CLE_C0*/
    compose = (compose & ~0x0f) | SELECT_VIDEO_IF_COLOR_KEY | SELECT_VIDEO3_IF_COLOR_KEY;
    /*compose = (compose & ~0x0f)  ;*/

    return compose;
}


static CARD32 SetChromaKey(VIAPtr pVia, unsigned long videoFlag,
                           CARD32 chromaLow, CARD32 chromaHigh,
                           CARD32 miniCtl, CARD32 compose)
{
    chromaLow  &= CHROMA_KEY_LOW;
    chromaHigh &= CHROMA_KEY_HIGH;

    chromaLow  |= (VIDInD(V_CHROMAKEY_LOW) & ~CHROMA_KEY_LOW);
    chromaHigh |= (VIDInD(V_CHROMAKEY_HIGH)& ~CHROMA_KEY_HIGH);

    chromaLow |= 0x40000000;

    SaveVideoRegister(pVia, V_CHROMAKEY_HIGH, chromaHigh);
    if (videoFlag & VIDEO_1_INUSE)
    {
        SaveVideoRegister(pVia, V_CHROMAKEY_LOW, chromaLow&(~V_CHROMAKEY_V3));
        /*Temporarily solve the H/W Interpolation error when using Chroma Key*/
        SaveVideoRegister(pVia, V1_MINI_CONTROL, miniCtl & 0xFFFFFFF8);
    }
    else
    {
        SaveVideoRegister(pVia, V_CHROMAKEY_LOW, chromaLow | V_CHROMAKEY_V3);
        SaveVideoRegister(pVia, V3_MINI_CONTROL, miniCtl & 0xFFFFFFF8);
    }

    /*Modified by Scottie[2001.12.5] for select video if (color key & chroma key)*/
    if (compose == SELECT_VIDEO_IF_COLOR_KEY)
        compose = SELECT_VIDEO_IF_COLOR_KEY | SELECT_VIDEO_IF_CHROMA_KEY;
    else
        compose = (compose & ~0x0f) | SELECT_VIDEO_IF_CHROMA_KEY;

    return compose;
}

static void SetVideoStart(VIAPtr pVia, unsigned long videoFlag,
                          unsigned int numbufs, CARD32 a1, CARD32 a2, CARD32 a3)
{
    CARD32 V1Addr[3] = {V1_STARTADDR_0, V1_STARTADDR_1, V1_STARTADDR_2};
    CARD32 V3Addr[3] = {V3_STARTADDR_0, V3_STARTADDR_1, V3_STARTADDR_2};
    CARD32* VideoAddr = (videoFlag & VIDEO_1_INUSE) ? V1Addr : V3Addr;

    SaveVideoRegister(pVia, VideoAddr[0], a1);
    if (numbufs > 1) SaveVideoRegister(pVia, VideoAddr[1], a2);
    if (numbufs > 2) SaveVideoRegister(pVia, VideoAddr[2], a3);
}

static void SetHQVFetch(VIAPtr pVia, CARD32 srcFetch, unsigned long srcHeight)
{
unsigned long proReg=0;
    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(pVia->swov.gdwVideoFlagSW & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;

    if (!pVia->HWDiff.dwHQVFetchByteUnit) {   /* CLE_C0 */
        srcFetch >>= 3; /* fetch unit is 8-byte */
    }
    SaveVideoRegister(pVia, proReg|HQV_SRC_FETCH_LINE, ((srcFetch - 1) << 16) | (srcHeight - 1));
}

static void SetFetch(VIAPtr pVia, unsigned long videoFlag, CARD32 fetch)
{
    fetch <<= 20;
    if (videoFlag & VIDEO_1_INUSE) {
        SaveVideoRegister(pVia, V12_QWORD_PER_LINE, fetch);
    }
    else {
        fetch |= VIDInD(V3_ALPHA_QWORD_PER_LINE) & ~V3_FETCH_COUNT;
        SaveVideoRegister(pVia, V3_ALPHA_QWORD_PER_LINE, fetch);
    }
}

static void SetDisplayCount(VIAPtr pVia, unsigned long videoFlag,
                            unsigned long srcWidth, unsigned long srcHeight)
{
    unsigned long DisplayCount;

    /* Removed VIA's large pixelformat switch/case.
     * All formats (YV12, UYVY, YUY2, VIA, RGB16 and RGB32)
     * seem to use the same count. /A
     */

    if (videoFlag & VIDEO_HQV_INUSE)
        DisplayCount = srcWidth - 1;
    else
        DisplayCount = srcWidth - pVia->swov.overlayRecordV1.dwminifyH;

    if (videoFlag & VIDEO_1_INUSE)
        SaveVideoRegister(pVia, V1_SOURCE_HEIGHT, (srcHeight << 16) | DisplayCount);
    else
        SaveVideoRegister(pVia, V3_SOURCE_WIDTH, DisplayCount);
}

static void SetMiniAndZoom(VIAPtr pVia, unsigned long videoFlag,
                           CARD32 miniCtl, CARD32 zoomCtl)
{
    if (videoFlag & VIDEO_1_INUSE) {
        SaveVideoRegister(pVia, V1_MINI_CONTROL, miniCtl);
        SaveVideoRegister(pVia, V1_ZOOM_CONTROL, zoomCtl);
    }
    else {
        SaveVideoRegister(pVia, V3_MINI_CONTROL, miniCtl);
        SaveVideoRegister(pVia, V3_ZOOM_CONTROL, zoomCtl);
    }
}

static void SetVideoControl(VIAPtr pVia, unsigned long videoFlag, CARD32 vidCtl)
{
    if (videoFlag & VIDEO_1_INUSE)
        SaveVideoRegister(pVia, V1_CONTROL, vidCtl);
    else
        SaveVideoRegister(pVia, V3_CONTROL, vidCtl);
}

static void FireVideoCommand(VIAPtr pVia, unsigned long videoFlag, CARD32 compose)
{
    if (videoFlag & VIDEO_1_INUSE)
        SaveVideoRegister(pVia, V_COMPOSE_MODE, compose | V1_COMMAND_FIRE);
    else
        SaveVideoRegister(pVia, V_COMPOSE_MODE, compose | V3_COMMAND_FIRE);
}

static void 
SetVideoWindow(ScrnInfoPtr pScrn, unsigned long videoFlag, LPDDUPDATEOVERLAY pUpdate)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    CARD32 left = pUpdate->DstLeft;
    CARD32 top = pUpdate->DstTop;
    CARD32 right = pUpdate->DstRight - 1;
    CARD32 bottom = pUpdate->DstBottom - 1;

    DBG_DD(ErrorF("SetVideoWindow: X (%ld,%ld) Y (%ld,%ld)\n",
		  left, right, top, bottom));

    /* modify for HW DVI limitation,
     * When we enable the CRT and DVI both, then change resolution.
     * If the resolution small than the panel physical size,
     * the video display in Y direction will be cut.
     * So, we need to adjust the Y top and bottom position.
     */
    if (videoFlag & VIDEO_1_INUSE) {
	if  (pBIOSInfo->SetDVI && pBIOSInfo->scaleY) {
	    top = pUpdate->DstTop * pBIOSInfo->panelY / pScrn->currentMode->VDisplay;
	    bottom = pUpdate->DstBottom * pBIOSInfo->panelY / pScrn->currentMode->VDisplay;
	}
    }

    if (top < 0)
	top = 0;
    else if (top > 2047)
	top = 2047;

    if (bottom < 0)
	bottom = 0;
    else if (bottom > 2047)
	bottom = 2047;

    if (left < 0)
	left = 0;
    else if (left > 2047)
	left = 2047;

    if (right < 0)
	right = 0;
    else if (right > 2047)
	right = 2047;

    if (videoFlag & VIDEO_1_INUSE) {
        SaveVideoRegister(pVia, V1_WIN_END_Y,  (right << 16) | bottom);
        SaveVideoRegister(pVia, V1_WIN_START_Y, (left << 16) | top);
    } else {
        SaveVideoRegister(pVia, V3_WIN_END_Y,  (right << 16) | bottom);
        SaveVideoRegister(pVia, V3_WIN_START_Y, (left << 16) | top);
    }
}

/* IVOR - Guess what. this needs checking */

/****************************************************************************
 *
 * Upd_Video()
 *
 ***************************************************************************/
static Bool 
Upd_Video(ScrnInfoPtr pScrn, unsigned long videoFlag,
	  unsigned long startAddr, LPDDUPDATEOVERLAY pUpdate,
	  unsigned long srcPitch,
	  unsigned long oriSrcWidth,  unsigned long oriSrcHeight,
	  unsigned long deinterlaceMode,
	  unsigned long haveColorKey, unsigned long haveChromaKey,
	  unsigned long colorKeyLow,  unsigned long colorKeyHigh,
	  unsigned long chromaKeyLow, unsigned long chromaKeyHigh)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAHWDiff *hwDiff = &pVia->HWDiff;

    int i;
    unsigned long vidCtl = 0, compose;
    unsigned long srcWidth, srcHeight, dstWidth, dstHeight;
    unsigned long zoomCtl = 0, miniCtl = 0;
    unsigned long hqvCtl = 0;
    unsigned long hqvFilterCtl = 0, hqvMiniCtl = 0;
    unsigned long haveHQVzoomH = 0, haveHQVzoomV = 0;
    unsigned long hqvSrcWidth = 0, hqvDstWidth = 0;
    unsigned long hqvSrcFetch = 0, hqvOffset = 0;
    unsigned long dwOffset = 0,fetch = 0,tmp = 0;

unsigned long proReg=0;
    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(videoFlag & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;

    compose = (VIDInD(V_COMPOSE_MODE) & 
        ~(SELECT_VIDEO_IF_COLOR_KEY | V1_COMMAND_FIRE | V3_COMMAND_FIRE)) | V_COMMAND_LOAD_VBI;

    DBG_DD(ErrorF("// Upd_Video:\n"));
    DBG_DD(ErrorF("Modified rSrc  X (%ld,%ld) Y (%ld,%ld)\n",
		  pUpdate->SrcLeft, pUpdate->SrcRight,
		  pUpdate->SrcTop, pUpdate->SrcBottom));
    DBG_DD(ErrorF("Modified rDest  X (%ld,%ld) Y (%ld,%ld)\n",
		  pUpdate->DstLeft, pUpdate->DstRight,
		  pUpdate->DstTop, pUpdate->DstBottom));

    pVia->swov.overlayRecordV1.dwWidth  = dstWidth  = pUpdate->DstRight - pUpdate->DstLeft;
    pVia->swov.overlayRecordV1.dwHeight = dstHeight = pUpdate->DstBottom - pUpdate->DstTop;
    srcWidth  = (unsigned long) pUpdate->SrcRight - pUpdate->SrcLeft;
    srcHeight = (unsigned long) pUpdate->SrcBottom - pUpdate->SrcTop;
    DBG_DD(ErrorF("===srcWidth= %ld \n", srcWidth));
    DBG_DD(ErrorF("===srcHeight= %ld \n", srcHeight));
    
    vidCtl = ViaSetVidCtl(pVia, videoFlag);
    viaOverlayGetV1V3Format(pVia, (videoFlag & VIDEO_1_INUSE) ? 1 : 3,
			    videoFlag, &vidCtl, &hqvCtl);
    
#if 0
    if (hwDiff->dwThreeHQVBuffer) { /* CLE_C0: HQV supports triple-buffering */
	hqvCtl &= ~HQV_SW_FLIP;
	hqvCtl |= HQV_TRIPLE_BUFF | HQV_FLIP_STATUS; 
    }
#endif 
    
    /* Starting address of source and Source offset*/
    dwOffset = viaOverlayGetSrcStartAddress(pVia, videoFlag, pUpdate, srcPitch, &hqvOffset);
    DBG_DD(ErrorF("===dwOffset= 0x%lx \n", dwOffset));
    DBG_DD(ErrorF("===hqvOffset= 0x%lx \n", hqvOffset));
    
    pVia->swov.overlayRecordV1.dwOffset = dwOffset;
    
    if (pVia->swov.SrcFourCC == FOURCC_YV12 ||
	pVia->swov.SrcFourCC == FOURCC_XVMC) {
	YCBCRREC YCbCr;
	if (videoFlag & VIDEO_HQV_INUSE) {
	    SetVideoStart(pVia, videoFlag, hwDiff->dwThreeHQVBuffer ? 3 : 2,
			  pVia->swov.overlayRecordV1.dwHQVAddr[0]+dwOffset,
			  pVia->swov.overlayRecordV1.dwHQVAddr[1]+dwOffset,
			  pVia->swov.overlayRecordV1.dwHQVAddr[2]+dwOffset);
	    
	    if (pVia->swov.SrcFourCC != FOURCC_XVMC) {
            if (pVia->ChipId == PCI_CHIP_VT3259)
            {
        DBG_DD(ErrorF("viaOverlayGetYCbCrStartAddressProA\n"));
	    YCbCr = viaOverlayGetYCbCrStartAddressProA(videoFlag, startAddr,
						   pVia->swov.overlayRecordV1.dwOffset,
						   pVia->swov.overlayRecordV1.dwUVoffset,
						   srcPitch,oriSrcHeight);
            }
            else
            {
        DBG_DD(ErrorF("viaOverlayGetYCbCrStartAddress\n"));
	    YCbCr = viaOverlayGetYCbCrStartAddress(videoFlag, startAddr,
						   pVia->swov.overlayRecordV1.dwOffset,
						   pVia->swov.overlayRecordV1.dwUVoffset,
						   srcPitch,oriSrcHeight);
            }

            if (pVia->ChipId == PCI_CHIP_VT3259)
	        SaveVideoRegister(pVia, proReg|0x1cc, dwOffset);   
	    SaveVideoRegister(pVia, proReg|HQV_SRC_STARTADDR_Y, YCbCr.dwY);
	    SaveVideoRegister(pVia, proReg|HQV_SRC_STARTADDR_U, YCbCr.dwCR);

            if (pVia->ChipId != PCI_CHIP_VT3259)
        	    SaveVideoRegister(pVia, proReg|HQV_SRC_STARTADDR_V, YCbCr.dwCB);
            }
	} else {
        DBG_DD(ErrorF("viaOverlayGetYCbCrStartAddress2\n"));
	    YCbCr = viaOverlayGetYCbCrStartAddress(videoFlag, startAddr,
						   pVia->swov.overlayRecordV1.dwOffset,
						   pVia->swov.overlayRecordV1.dwUVoffset,
						   srcPitch, oriSrcHeight);
	    
	    if (videoFlag & VIDEO_1_INUSE) {
		SaveVideoRegister(pVia, V1_STARTADDR_0, YCbCr.dwY);
		SaveVideoRegister(pVia, V1_STARTADDR_CB0, YCbCr.dwCR);
		SaveVideoRegister(pVia, V1_STARTADDR_CR0, YCbCr.dwCB);
	    } else
		DBG_DD(ErrorF("Upd_Video() : We do not support YV12 with V3!\n"));
	}
    } else {
	if (videoFlag & VIDEO_HQV_INUSE) {
	    hqvSrcWidth = (unsigned long) pUpdate->SrcRight - pUpdate->SrcLeft;
	    hqvDstWidth = (unsigned long) pUpdate->DstRight - pUpdate->DstLeft;
	    
	    if (hqvSrcWidth > hqvDstWidth)
		dwOffset = dwOffset * hqvDstWidth / hqvSrcWidth;
	    
	    SetVideoStart(pVia, videoFlag, hwDiff->dwThreeHQVBuffer ? 3 : 2,
			  pVia->swov.overlayRecordV1.dwHQVAddr[0] + hqvOffset,
			  pVia->swov.overlayRecordV1.dwHQVAddr[1] + hqvOffset,
			  pVia->swov.overlayRecordV1.dwHQVAddr[2] + hqvOffset);
	    
            if (pVia->ChipId == PCI_CHIP_VT3259)
                SaveVideoRegister(pVia, proReg|0x1cc, dwOffset); 

	    SaveVideoRegister(pVia, proReg|HQV_SRC_STARTADDR_Y, startAddr);
	} else {
	    startAddr += dwOffset;
	    SetVideoStart(pVia, videoFlag, 1, startAddr, 0, 0);
	}
    }

    fetch = viaOverlayGetFetch(pVia, videoFlag,
			       srcWidth, dstWidth, oriSrcWidth, &hqvSrcFetch);
    DBG_DD(ErrorF("===fetch= 0x%lx \n", fetch));

#if 0
    /* For DCT450 test-BOB INTERLEAVE */
    if ((deinterlaceMode & DDOVER_INTERLEAVED) && (deinterlaceMode & DDOVER_BOB)) {
	if (videoFlag & VIDEO_HQV_INUSE)
	    hqvCtl |= HQV_FIELD_2_FRAME | HQV_FRAME_2_FIELD | HQV_DEINTERLACE;
	else
	    vidCtl |= V1_BOB_ENABLE | V1_FRAME_BASE;
    } else if (deinterlaceMode & DDOVER_BOB) {
	if (videoFlag & VIDEO_HQV_INUSE)
	    /* The HQV source data line count should be two times of the original line count */
	    hqvCtl |= HQV_FIELD_2_FRAME | HQV_DEINTERLACE;
	else
	    vidCtl |= V1_BOB_ENABLE;
    }
#endif
    
    if (videoFlag & VIDEO_HQV_INUSE) {
	if (!(deinterlaceMode & DDOVER_INTERLEAVED) && (deinterlaceMode & DDOVER_BOB))
	    SetHQVFetch(pVia, hqvSrcFetch, oriSrcHeight << 1);
	else
	    SetHQVFetch(pVia, hqvSrcFetch, oriSrcHeight);
	
	if (pVia->swov.SrcFourCC == FOURCC_YV12 ||
	    pVia->swov.SrcFourCC == FOURCC_XVMC) {
	    if (videoFlag & VIDEO_1_INUSE)
		SaveVideoRegister(pVia, V1_STRIDE, srcPitch << 1);
	    else
		SaveVideoRegister(pVia, V3_STRIDE, srcPitch << 1);
	    
	    SaveVideoRegister(pVia, proReg|HQV_SRC_STRIDE, ((srcPitch >> 1) << 16) | srcPitch);
	    SaveVideoRegister(pVia, proReg|HQV_DST_STRIDE, (srcPitch << 1));
	} else {
	    if (videoFlag & VIDEO_1_INUSE)
		SaveVideoRegister(pVia, V1_STRIDE, srcPitch);
	    else
		SaveVideoRegister(pVia, V3_STRIDE, srcPitch);
	    
	    SaveVideoRegister(pVia, proReg|HQV_SRC_STRIDE, srcPitch);
	    SaveVideoRegister(pVia, proReg|HQV_DST_STRIDE, srcPitch);
	}
	
    } else {
	if (videoFlag & VIDEO_1_INUSE)
	    SaveVideoRegister(pVia, V1_STRIDE, srcPitch | (srcPitch << 15));
	else
	    SaveVideoRegister(pVia, V3_STRIDE, srcPitch | (srcPitch << 15));
    }
    
    /* Set destination window */
    SetVideoWindow(pScrn, videoFlag, pUpdate);
    
    compose |= ALWAYS_SELECT_VIDEO;
    
    /* Setup X zoom factor*/
    
    pVia->swov.overlayRecordV1.dwFetchAlignment = 0;

    if (!viaOverlayHQVCalcZoomWidth(pVia, videoFlag, srcWidth, dstWidth, &zoomCtl, &miniCtl,
				    &hqvFilterCtl, &hqvMiniCtl, &haveHQVzoomH)) {
	/* Need to scale (minify) too much - can't handle it. */
	SetFetch(pVia, videoFlag, fetch);
	FireVideoCommand(pVia, videoFlag, compose);
	FlushVidRegBuffer(pVia);
	return FALSE;
    }
    
    SetFetch(pVia, videoFlag, fetch);
    
    /* Setup Y zoom factor */
    
    /* For DCT450 test-BOB INTERLEAVE */
    if ((deinterlaceMode & DDOVER_INTERLEAVED) && (deinterlaceMode & DDOVER_BOB)) {
	if (!(videoFlag & VIDEO_HQV_INUSE)) {
	    srcHeight /= 2;
	    if (videoFlag & VIDEO_1_INUSE)
		vidCtl |= V1_BOB_ENABLE | V1_FRAME_BASE;
	    else
		vidCtl |= V3_BOB_ENABLE | V3_FRAME_BASE;
	} else
	    hqvCtl |= HQV_FIELD_2_FRAME | HQV_FRAME_2_FIELD | HQV_DEINTERLACE;
    } else if (deinterlaceMode & DDOVER_BOB) {
	if (videoFlag & VIDEO_HQV_INUSE) {
	    srcHeight <<= 1;
	    hqvCtl |= HQV_FIELD_2_FRAME | HQV_DEINTERLACE;
	} else {
	    if (videoFlag & VIDEO_1_INUSE)
		vidCtl |= V1_BOB_ENABLE;
	    else
		vidCtl |= V3_BOB_ENABLE;
	}
    }

    SetDisplayCount(pVia, videoFlag, srcWidth, srcHeight);
    
    if (!viaOverlayHQVCalcZoomHeight(pVia, srcHeight, dstHeight, &zoomCtl, &miniCtl,
				     &hqvFilterCtl, &hqvMiniCtl ,&haveHQVzoomV)) {
	/* Need to scale (minify) too much - can't handle it. */
	FireVideoCommand(pVia, videoFlag, compose);
	FlushVidRegBuffer(pVia);
	return FALSE;
    }
    
    SetupFIFOs(pVia, videoFlag, miniCtl, srcWidth);
    
    if (videoFlag & VIDEO_HQV_INUSE) {
	miniCtl=0;

	if (haveHQVzoomH || haveHQVzoomV) {
	    tmp = 0;

	    if (haveHQVzoomH) {
		miniCtl = V1_X_INTERPOLY;
		tmp = zoomCtl & 0xffff0000;
	    }

	    if (haveHQVzoomV) {
		miniCtl |= V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY;
		tmp |= zoomCtl & 0x0000ffff;
		hqvFilterCtl &= 0xfffdffff;
	    }
	    
	    /* Temporary fix for 2D bandwidth problem. 2002/08/01*/
	    if (pVia->swov.gdwUseExtendedFIFO)
		miniCtl &= ~V1_Y_INTERPOLY;
	    
	    SetMiniAndZoom(pVia, videoFlag, miniCtl, tmp);
	} else {
	    if (srcHeight == dstHeight)
		hqvFilterCtl &= 0xfffdffff;

	    SetMiniAndZoom(pVia, videoFlag, 0, 0);
	}
	SaveVideoRegister(pVia, proReg|HQV_MINIFY_CONTROL, hqvMiniCtl);
	SaveVideoRegister(pVia, proReg|HQV_FILTER_CONTROL, hqvFilterCtl);
    } else
	SetMiniAndZoom(pVia, videoFlag, miniCtl, zoomCtl);
    
    if (haveColorKey)
	compose = SetColorKey(pVia, videoFlag, colorKeyLow, colorKeyHigh, compose);

    if (haveChromaKey)
	compose = SetChromaKey(pVia, videoFlag, chromaKeyLow, chromaKeyHigh,
			       miniCtl, compose);

/*wait hqvdone.*/
    viaWaitHQVDone(pVia);

DBG_DD(ErrorF("** 6 of %ld **", pVia->VidRegCursor));

    /* Setup video control*/
    if (videoFlag & VIDEO_HQV_INUSE)
    {
        if (!pVia->swov.SWVideo_ON)
        {
            DBG_DD(ErrorF("    First HQV\n"));
	    
            FlushVidRegBuffer(pVia);
	    
            DBG_DD(ErrorF(" Wait flips"));
            
            if (hwDiff->dwHQVInitPatch)
            {
                DBG_DD(ErrorF(" Initializing HQV twice ..."));
                for (i = 0; i < 2; i++) {
                    viaWaitHQVFlipClear(pVia,
                                        ((hqvCtl & ~HQV_SW_FLIP) | HQV_FLIP_STATUS) & ~HQV_ENABLE);
                    VIDOutD(proReg|HQV_CONTROL, hqvCtl);
                    viaWaitHQVFlip(pVia);
                }
                DBG_DD(ErrorF(" done.\n"));
            }
            else
            { /* CLE_C0 */
                CARD32 volatile *HQVCtrl =
                    (CARD32 volatile *) (pVia->VidMapBase + (proReg|HQV_CONTROL));
		
#if 1                /* check HQV is idle */
                DBG_DD(ErrorF("HQV control wf - %08lx\n", *HQVCtrl));
                while (!(*HQVCtrl & HQV_IDLE)) {
                    DBG_DD(ErrorF("HQV control busy - %08lx\n", *HQVCtrl));
                    usleep(1);
                }
#endif
                
                VIDOutD(proReg|HQV_CONTROL, hqvCtl | HQV_FLIP_STATUS);
                VIDOutD(proReg|HQV_CONTROL, hqvCtl | HQV_SW_FLIP);
                
                DBG_DD(ErrorF("HQV control wf5 - %08lx\n", *HQVCtrl));
                DBG_DD(ErrorF(" Wait flips5")); 
                
/*		for (i = 0; (i < 50) && !(*HQVCtrl & HQV_FLIP_STATUS); i++) {
        DBG_DD(ErrorF(" HQV wait %d %08x\n",i, *HQVCtrl));
        *HQVCtrl |= HQV_SW_FLIP | HQV_FLIP_STATUS;
        usleep(1);
		}*/
               
                viaWaitHQVFlip(pVia);
                
                DBG_DD(ErrorF(" Wait flips6"));
            }



            if (videoFlag & VIDEO_1_INUSE)
            {
                VIDOutD(V1_CONTROL, vidCtl);
                VIDOutD(V_COMPOSE_MODE, compose | V1_COMMAND_FIRE);
                if (pVia->swov.gdwUseExtendedFIFO)
                {
                    /*Set Display FIFO*/
                    DBG_DD(ErrorF(" Wait flips7"));
                    viaWaitVBI(pVia);
                    DBG_DD(ErrorF(" Wait flips 8"));
                    hwp->writeSeq(hwp, 0x17, 0x2F);
                    ViaSeqMask(hwp, 0x16, 0x14, 0x1F);
                    hwp->writeSeq(hwp, 0x18, 0x56);
                    DBG_DD(ErrorF(" Wait flips 9"));
                }
            }
            else
            {
                DBG_DD(ErrorF(" Wait flips 10 ctl: %08lx, comp %08lx\n", vidCtl,compose));
                VIDOutD(V3_CONTROL, vidCtl);
                VIDOutD(V_COMPOSE_MODE, compose | V3_COMMAND_FIRE);
            }
            
            DBG_DD(ErrorF(" Done flips"));
        }
        else
        {
            DBG_DD(ErrorF("    Normal called\n"));
            SaveVideoRegister(pVia, proReg|HQV_CONTROL, hqvCtl | HQV_FLIP_STATUS);

            SetVideoControl(pVia, videoFlag, vidCtl);
            FireVideoCommand(pVia, videoFlag, compose);
            /*viaWaitHQVDone(pVia);*/

            FlushVidRegBuffer(pVia);
        }
    }
    else
    {
        SetVideoControl(pVia, videoFlag, vidCtl);
        FireVideoCommand(pVia, videoFlag, compose);
/*       viaWaitHQVDone(pVia);*/
        FlushVidRegBuffer(pVia);
    }
    pVia->swov.SWVideo_ON = TRUE;
    
    DBG_DD(ErrorF(" Done Upd_Video"));
    
    return TRUE;

} /* Upd_Video */

/*
 *  VIAVidUpdateOverlay
 *  Parameters:   src rectangle, dst rectangle, colorkey...
 *  Return Value: unsigned long of state
 *  note: Update the overlay image param.
 */
Bool
VIAVidUpdateOverlay(ScrnInfoPtr pScrn, LPDDUPDATEOVERLAY pUpdate)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    OVERLAYRECORD* ovlV1 = &pVia->swov.overlayRecordV1;

    unsigned long flags = pUpdate->dwFlags;
    unsigned long videoFlag = 0;
    unsigned long startAddr = 0;
    unsigned long deinterlaceMode=0;

    unsigned long haveColorKey = 0, haveChromaKey = 0;
    unsigned long colorKeyLow = 0, colorKeyHigh = 0;
    unsigned long chromaKeyLow = 0, chromaKeyHigh = 0;


    unsigned long scrnWidth, scrnHeight;
    int dstTop, dstBottom, dstLeft, dstRight;
    int panDX,panDY; /* Panning delta */

    unsigned long proReg=0;

    panDX = pVia->swov.panning_x - pVia->swov.panning_old_x;
    panDY = pVia->swov.panning_y - pVia->swov.panning_old_y;

    /* Adjust to fix panning mode bug */

    pUpdate->DstLeft   -= panDX;
    pUpdate->DstTop    -= panDY;
    pUpdate->DstRight  -= panDX;
    pUpdate->DstBottom -= panDY;

    DBG_DD(ErrorF("Raw rSrc  X (%ld,%ld) Y (%ld,%ld)\n",
        pUpdate->SrcLeft, pUpdate->SrcRight,
        pUpdate->SrcTop, pUpdate->SrcBottom));
    DBG_DD(ErrorF("Raw rDest  X (%ld,%ld) Y (%ld,%ld)\n",
        pUpdate->DstLeft, pUpdate->DstRight,
        pUpdate->DstTop, pUpdate->DstBottom));

    if ((pVia->swov.SrcFourCC == FOURCC_YUY2) ||
        (pVia->swov.SrcFourCC == FOURCC_YV12) ||
        (pVia->swov.SrcFourCC == FOURCC_XVMC))
    {
        videoFlag = pVia->swov.gdwVideoFlagSW;
    }

    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(videoFlag & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;


    flags |= DDOVER_INTERLEAVED;

    /* Disable destination color keying if the alpha window is in use. */
    if (pVia->swov.gdwAlphaEnabled)
        flags &= ~DDOVER_KEYDEST;

    ResetVidRegBuffer(pVia);

    /*for SW decode HW overlay use*/
    startAddr = VIDInD(proReg|HQV_SRC_STARTADDR_Y);
    
    if (flags & DDOVER_KEYDEST) {
	haveColorKey = 1;
	colorKeyLow = pUpdate->dwColorSpaceLowValue;
    }

    if (flags & DDOVER_INTERLEAVED)
	deinterlaceMode |= DDOVER_INTERLEAVED;

    if (flags & DDOVER_BOB)
	deinterlaceMode |= DDOVER_BOB;
    
    if ((pVia->ChipId == PCI_CHIP_CLE3122) && (pScrn->currentMode->HDisplay > 1024)) {
	DBG_DD(ErrorF("UseExtendedFIFO\n"));
	pVia->swov.gdwUseExtendedFIFO = 1;
    } else
	pVia->swov.gdwUseExtendedFIFO = 0;
    
    /* Figure out actual rSrc rectangle */
    
    dstLeft = pUpdate->DstLeft;
    dstTop = pUpdate->DstTop;
    dstRight = pUpdate->DstRight;
    dstBottom = pUpdate->DstBottom;
    
    scrnWidth  = pScrn->currentMode->HDisplay;
    scrnHeight = pScrn->currentMode->VDisplay;
    
    if (dstLeft < 0) {
	pUpdate->SrcLeft = (((-dstLeft) * ovlV1->dwV1OriWidth) +
			    ((dstRight - dstLeft) >> 1)) / (dstRight - dstLeft);
    }
    if (dstRight > scrnWidth) {
	pUpdate->SrcRight = (((scrnWidth - dstLeft) * ovlV1->dwV1OriWidth) +
			     ((dstRight - dstLeft) >> 1)) / (dstRight - dstLeft);
    }
    if (dstTop < 0) {
	pUpdate->SrcTop = (((-dstTop) * ovlV1->dwV1OriHeight) +
			   ((dstBottom - dstTop) >> 1)) / (dstBottom - dstTop);
    }
    if (dstBottom > scrnHeight) {
	pUpdate->SrcBottom = (((scrnHeight - dstTop) * ovlV1->dwV1OriHeight) +
			      ((dstBottom - dstTop) >> 1)) / (dstBottom - dstTop);
    }
    
    /* Save modified src & original dest rectangle param. */
    
    if ((pVia->swov.SrcFourCC == FOURCC_YUY2) ||
	(pVia->swov.SrcFourCC == FOURCC_YV12) ||
	(pVia->swov.SrcFourCC == FOURCC_XVMC)) {   
	pVia->swov.SWDevice.gdwSWDstLeft   = pUpdate->DstLeft + panDX;
	pVia->swov.SWDevice.gdwSWDstTop    = pUpdate->DstTop + panDY;
	pVia->swov.SWDevice.gdwSWDstWidth  = pUpdate->DstRight - pUpdate->DstLeft;
	pVia->swov.SWDevice.gdwSWDstHeight = pUpdate->DstBottom - pUpdate->DstTop;
	
	pVia->swov.SWDevice.gdwSWSrcWidth  =
	    ovlV1->dwV1SrcWidth = pUpdate->SrcRight - pUpdate->SrcLeft;
	pVia->swov.SWDevice.gdwSWSrcHeight =
	    ovlV1->dwV1SrcHeight = pUpdate->SrcBottom - pUpdate->SrcTop;
    }
    
    ovlV1->dwV1SrcLeft  = pUpdate->SrcLeft;
    ovlV1->dwV1SrcRight = pUpdate->SrcRight;
    ovlV1->dwV1SrcTop   = pUpdate->SrcTop;
    ovlV1->dwV1SrcBot   = pUpdate->SrcBottom;
    
    /* Figure out actual rDest rectangle */
    
    pUpdate->DstLeft = (dstLeft < 0) ? 0 : dstLeft;
    pUpdate->DstTop = (dstTop < 0) ? 0 : dstTop;
    if (pUpdate->DstTop >= scrnHeight) pUpdate->DstTop = scrnHeight-1;
    pUpdate->DstRight = (dstRight > scrnWidth) ? scrnWidth : dstRight;
    pUpdate->DstBottom = (dstBottom > scrnHeight) ? scrnHeight : dstBottom;
    
    /* Update the overlay */
    
    if (!Upd_Video(pScrn, videoFlag, startAddr, pUpdate,
		  pVia->swov.SWDevice.dwPitch, ovlV1->dwV1OriWidth,
		  ovlV1->dwV1OriHeight, deinterlaceMode, haveColorKey, haveChromaKey,
		  colorKeyLow, colorKeyHigh, chromaKeyLow, chromaKeyHigh))
	return FALSE;

    pVia->swov.SWVideo_ON = FALSE;
    
    pVia->swov.panning_old_x = pVia->swov.panning_x;
    pVia->swov.panning_old_y = pVia->swov.panning_y;

    return TRUE;

} /*VIAVidUpdateOverlay*/

/*
 *
 */
void
ViaOverlayHide(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD32 videoFlag = 0;
    unsigned long proReg = 0;

    if ((pVia->swov.SrcFourCC == FOURCC_YUY2) ||
        (pVia->swov.SrcFourCC == FOURCC_YV12) ||
        (pVia->swov.SrcFourCC == FOURCC_XVMC))
        videoFlag = pVia->swov.gdwVideoFlagSW;

    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(videoFlag & VIDEO_1_INUSE))
        proReg = REG_HQV1_INDEX;

    
    ResetVidRegBuffer(pVia);
    
    if (pVia->HWDiff.dwHQVDisablePatch)
        ViaSeqMask(hwp, 0x2E, 0x00, 0x10);
    
    SaveVideoRegister(pVia, V_FIFO_CONTROL, V1_FIFO_PRETHRESHOLD12 |
		      V1_FIFO_THRESHOLD8 | V1_FIFO_DEPTH16);
    SaveVideoRegister(pVia, ALPHA_V3_FIFO_CONTROL, ALPHA_FIFO_THRESHOLD4
		      | ALPHA_FIFO_DEPTH8 | V3_FIFO_THRESHOLD24 | V3_FIFO_DEPTH32);
    
    if (videoFlag & VIDEO_HQV_INUSE)
        SaveVideoRegister(pVia, proReg|HQV_CONTROL, VIDInD(proReg|HQV_CONTROL) & ~HQV_ENABLE);
    
    if (videoFlag & VIDEO_1_INUSE)
        SaveVideoRegister(pVia, V1_CONTROL, VIDInD(V1_CONTROL) & ~V1_ENABLE);
    else
        SaveVideoRegister(pVia, V3_CONTROL, VIDInD(V3_CONTROL) & ~V3_ENABLE);
    
    FireVideoCommand(pVia, videoFlag, VIDInD(V_COMPOSE_MODE));
    FlushVidRegBuffer(pVia);
    
    if (pVia->HWDiff.dwHQVDisablePatch)
        ViaSeqMask(hwp, 0x2E, 0x10, 0x10);
    
    pVia->swov.SWVideo_ON = FALSE;
    
    if (pVia->swov.gdwUseExtendedFIFO)
        ViaSetPrimaryFIFO(pScrn, pScrn->currentMode);

    pVia->VideoStatus &= ~VIDEO_SWOV_ON;
}
