/*
 * Copyright 2005-2011 The Openchrome Project [openchrome.org]
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "via.h"
#include "via_driver.h"
#include "via_vgahw.h"
#include "via_id.h"

#include "cursorstr.h"

/*
 * Enables the second display channel.
 */
void
ViaSecondDisplayChannelEnable(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaSecondDisplayChannelEnable\n"));
    ViaCrtcMask(hwp, 0x6A, 0x00, 1 << 6);
    ViaCrtcMask(hwp, 0x6A, 1 << 7, 1 << 7);
    ViaCrtcMask(hwp, 0x6A, 1 << 6, 1 << 6);
}

/*
 * Disables the second display channel.
 */
void
ViaSecondDisplayChannelDisable(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaSecondDisplayChannelDisable\n"));

    ViaCrtcMask(hwp, 0x6A, 0x00, 1 << 6);
    ViaCrtcMask(hwp, 0x6A, 0x00, 1 << 7);
    ViaCrtcMask(hwp, 0x6A, 1 << 6, 1 << 6);
}

/*
 * Initial settings for displays.
 */
void
ViaDisplayInit(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplayInit\n"));

    ViaSecondDisplayChannelDisable(pScrn);
    ViaCrtcMask(hwp, 0x6A, 0x00, 0x3D);

    hwp->writeCrtc(hwp, 0x6B, 0x00);
    hwp->writeCrtc(hwp, 0x6C, 0x00);
    hwp->writeCrtc(hwp, 0x79, 0x00);

    /* (IGA1 Timing Plus 2, added in VT3259 A3 or later) */
    if (pVia->Chipset != VIA_CLE266 && pVia->Chipset != VIA_KM400)
        ViaCrtcMask(hwp, 0x47, 0x00, 0xC8);
}

/*
 * Enables simultaneous mode.
 */
void
ViaDisplayEnableSimultaneous(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaDisplayEnableSimultaneous\n"));
    ViaCrtcMask(hwp, 0x6B, 0x08, 0x08);
}

/*
 * Disables simultaneous mode.
 */
void
ViaDisplayDisableSimultaneous(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaDisplayDisableSimultaneous\n"));
    ViaCrtcMask(hwp, 0x6B, 0x00, 0x08);
}

/*
 * Enables CRT using DPMS registers.
 */
void
ViaDisplayEnableCRT(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplayEnableCRT\n"));
    ViaCrtcMask(hwp, 0x36, 0x00, 0x30);
}

/*
 * Disables CRT using DPMS registers.
 */
void
ViaDisplayDisableCRT(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplayDisableCRT\n"));
    ViaCrtcMask(hwp, 0x36, 0x30, 0x30);
}

void
ViaDisplayEnableDVO(ScrnInfoPtr pScrn, int port)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplayEnableDVO, port: %d\n", port));
    switch (port) {
        case VIA_DI_PORT_DVP0:
            ViaSeqMask(hwp, 0x1E, 0xC0, 0xC0);
            break;
        case VIA_DI_PORT_DVP1:
            ViaSeqMask(hwp, 0x1E, 0x30, 0x30);
            break;
    }
}

void
ViaDisplayDisableDVO(ScrnInfoPtr pScrn, int port)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplayDisableDVO, port: %d\n", port));
    switch (port) {
        case VIA_DI_PORT_DVP0:
            ViaSeqMask(hwp, 0x1E, 0x00, 0xC0);
            break;
        case VIA_DI_PORT_DVP1:
            ViaSeqMask(hwp, 0x1E, 0x00, 0x30);
            break;
    }
}

/*
 * Sets the primary or secondary display stream on CRT.
 */
void
ViaDisplaySetStreamOnCRT(ScrnInfoPtr pScrn, Bool primary)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplaySetStreamOnCRT\n"));

    if (primary)
        ViaSeqMask(hwp, 0x16, 0x00, 0x40);
    else
        ViaSeqMask(hwp, 0x16, 0x40, 0x40);
}

/*
 * Sets the primary or secondary display stream on internal TMDS.
 */
void
ViaDisplaySetStreamOnDFP(ScrnInfoPtr pScrn, Bool primary)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplaySetStreamOnDFP\n"));

    if (primary)
        ViaCrtcMask(hwp, 0x99, 0x00, 0x10);
    else
        ViaCrtcMask(hwp, 0x99, 0x10, 0x10);
}

void
ViaDisplaySetStreamOnDVO(ScrnInfoPtr pScrn, int port, Bool primary)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    int regNum;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaDisplaySetStreamOnDVO, port: %d\n", port));

    switch (port) {
        case VIA_DI_PORT_DVP0:
            regNum = 0x96;
            break;
        case VIA_DI_PORT_DVP1:
            regNum = 0x9B;
            break;
        case VIA_DI_PORT_DFPLOW:
            regNum = 0x97;
            break;
        case VIA_DI_PORT_DFPHIGH:
            regNum = 0x99;
            break;
    }

    if (primary)
        ViaCrtcMask(hwp, regNum, 0x00, 0x10);
    else
        ViaCrtcMask(hwp, regNum, 0x10, 0x10);
}

static void
ViaCRTCSetGraphicsRegisters(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    /* graphics registers */
    hwp->writeGr(hwp, 0x00, 0x00);
    hwp->writeGr(hwp, 0x01, 0x00);
    hwp->writeGr(hwp, 0x02, 0x00);
    hwp->writeGr(hwp, 0x03, 0x00);
    hwp->writeGr(hwp, 0x04, 0x00);
    hwp->writeGr(hwp, 0x05, 0x40);
    hwp->writeGr(hwp, 0x06, 0x05);
    hwp->writeGr(hwp, 0x07, 0x0F);
    hwp->writeGr(hwp, 0x08, 0xFF);

    ViaGrMask(hwp, 0x20, 0, 0xFF);
    ViaGrMask(hwp, 0x21, 0, 0xFF);
    ViaGrMask(hwp, 0x22, 0, 0xFF);
}

static void
ViaCRTCSetAttributeRegisters(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 i;

    /* attribute registers */
    for (i = 0; i <= 0xF; i++) {
        hwp->writeAttr(hwp, i, i);
    }
    hwp->writeAttr(hwp, 0x10, 0x41);
    hwp->writeAttr(hwp, 0x11, 0xFF);
    hwp->writeAttr(hwp, 0x12, 0x0F);
    hwp->writeAttr(hwp, 0x13, 0x00);
    hwp->writeAttr(hwp, 0x14, 0x00);
}

static Bool
via_xf86crtc_resize (ScrnInfoPtr scrn, int width, int height)
{
    scrn->virtualX = width;
    scrn->virtualY = height;
    return TRUE;
}

static const
xf86CrtcConfigFuncsRec via_xf86crtc_config_funcs = {
    via_xf86crtc_resize
};

void
VIALoadRgbLut(ScrnInfoPtr pScrn, int start, int numColors, LOCO *colors)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    int i, j;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIALoadRgbLut\n"));

    hwp->enablePalette(hwp);
    hwp->writeDacMask(hwp, 0xFF);

    /* We need the same palette contents for both 16 and 24 bits, but X doesn't
     * play: X's colormap handling is hopelessly intertwined with almost every
     * X subsystem.  So we just space out RGB values over the 256*3. */

    switch (pScrn->bitsPerPixel) {
        case 16:
            for (i = start; i < numColors; i++) {
                hwp->writeDacWriteAddr(hwp, i * 4);
                for (j = 0; j < 4; j++) {
                    hwp->writeDacData(hwp, colors[i / 2].red);
                    hwp->writeDacData(hwp, colors[i].green);
                    hwp->writeDacData(hwp, colors[i / 2].blue);
                }
            }
            break;
        case 8:
        case 24:
        case 32:
            for (i = start; i < numColors; i++) {
                hwp->writeDacWriteAddr(hwp, i);
                hwp->writeDacData(hwp, colors[i].red);
                hwp->writeDacData(hwp, colors[i].green);
                hwp->writeDacData(hwp, colors[i].blue);
            }
            break;
        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Unsupported bitdepth: %d\n", pScrn->bitsPerPixel);
            break;
    }
    hwp->disablePalette(hwp);
}

static void
via_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue,
					int size)
{
	ScrnInfoPtr pScrn = crtc->scrn;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    int SR1A, SR1B, CR67, CR6A;
    LOCO colors[size];
    int i, index;

    for (i = 0; i < size; i++) {
		colors[i].red = red[i] >> 8;
		colors[i].green = green[i] >> 8;
		colors[i].blue = blue[i] >> 8;
	}

    if (pScrn->bitsPerPixel != 8) {

        if (pBIOSInfo->FirstCRTC->IsActive) {

            switch (pVia->Chipset) {
                case VIA_CLE266:
                case VIA_KM400:
                    ViaSeqMask(hwp, 0x16, 0x80, 0x80);
                    break;
                default:
                    ViaCrtcMask(hwp, 0x33, 0x80, 0x80);
                    break;
            }

            ViaSeqMask(hwp, 0x1A, 0x00, 0x01);
            VIALoadRgbLut(pScrn, 0, size, colors);
        }

        /* If secondary is enabled, adjust its palette too. */
        if (pBIOSInfo->SecondCRTC->IsActive) {
            if (!(pVia->Chipset == VIA_CLE266
                  && CLE266_REV_IS_AX(pVia->ChipRev))) {
                ViaSeqMask(hwp, 0x1A, 0x01, 0x01);
                ViaCrtcMask(hwp, 0x6A, 0x02, 0x02);
                switch (pVia->Chipset) {
                    case VIA_CLE266:
                    case VIA_KM400:
                    case VIA_K8M800:
                    case VIA_PM800:
                        break;
                    default:
                        ViaCrtcMask(hwp, 0x6A, 0x20, 0x20);
                        break;
                }
                VIALoadRgbLut(pScrn, 0, size, colors);
            }
        }

    } else {

        SR1A = hwp->readSeq(hwp, 0x1A);
        SR1B = hwp->readSeq(hwp, 0x1B);
        CR67 = hwp->readCrtc(hwp, 0x67);
        CR6A = hwp->readCrtc(hwp, 0x6A);

        if (pBIOSInfo->SecondCRTC->IsActive) {
            ViaSeqMask(hwp, 0x1A, 0x01, 0x01);
            ViaSeqMask(hwp, 0x1B, 0x80, 0x80);
            ViaCrtcMask(hwp, 0x67, 0x00, 0xC0);
            ViaCrtcMask(hwp, 0x6A, 0xC0, 0xC0);
        }

        for (i = 0; i < size; i++) {
            hwp->writeDacWriteAddr(hwp, i);
            hwp->writeDacData(hwp, colors[i].red);
            hwp->writeDacData(hwp, colors[i].green);
            hwp->writeDacData(hwp, colors[i].blue);
        }

        if (pBIOSInfo->SecondCRTC->IsActive) {
            hwp->writeSeq(hwp, 0x1A, SR1A);
            hwp->writeSeq(hwp, 0x1B, SR1B);
            hwp->writeCrtc(hwp, 0x67, CR67);
            hwp->writeCrtc(hwp, 0x6A, CR6A);

            /* Screen 0 palette was changed by mode setting of Screen 1,
             * so load it again. */
            for (i = 0; i < size; i++) {
                hwp->writeDacWriteAddr(hwp, i);
                hwp->writeDacData(hwp, colors[i].red);
                hwp->writeDacData(hwp, colors[i].green);
                hwp->writeDacData(hwp, colors[i].blue);
            }
        }
    }
}

void
ViaGammaDisable(ScrnInfoPtr pScrn)
{

    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    switch (pVia->Chipset) {
        case VIA_CLE266:
        case VIA_KM400:
            ViaSeqMask(hwp, 0x16, 0x00, 0x80);
            break;
        default:
            ViaCrtcMask(hwp, 0x33, 0x00, 0x80);
            break;
    }

    /* Disable gamma on secondary */
    /* This is needed or the hardware will lockup */
    ViaSeqMask(hwp, 0x1A, 0x00, 0x01);
    ViaCrtcMask(hwp, 0x6A, 0x00, 0x02);
    switch (pVia->Chipset) {
        case VIA_CLE266:
        case VIA_KM400:
        case VIA_K8M800:
        case VIA_PM800:
            break;
        default:
            ViaCrtcMask(hwp, 0x6A, 0x00, 0x20);
            break;
    }
}

void
ViaCRTCInit(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    hwp->writeSeq(hwp, 0x10, 0x01); /* unlock extended registers */
    ViaCrtcMask(hwp, 0x47, 0x00, 0x01); /* unlock CRT registers */
    ViaCRTCSetGraphicsRegisters(pScrn);
    ViaCRTCSetAttributeRegisters(pScrn);
}

void
ViaFirstCRTCSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    CARD16 temp;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaFirstCRTCSetMode\n"));

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Setting up %s\n", mode->name));

    ViaCrtcMask(hwp, 0x11, 0x00, 0x80); /* modify starting address */
    ViaCrtcMask(hwp, 0x03, 0x80, 0x80); /* enable vertical retrace access */

    /* Set Misc Register */
    temp = 0x23;
    if (mode->Flags & V_NHSYNC)
        temp |= 0x40;
    if (mode->Flags & V_NVSYNC)
        temp |= 0x80;
    temp |= 0x0C; /* Undefined/external clock */
    hwp->writeMiscOut(hwp, temp);

    /* Sequence registers */
    hwp->writeSeq(hwp, 0x00, 0x00);

#if 0
    if (mode->Flags & V_CLKDIV2)
        hwp->writeSeq(hwp, 0x01, 0x09);
    else
#endif
        hwp->writeSeq(hwp, 0x01, 0x01);

    hwp->writeSeq(hwp, 0x02, 0x0F);
    hwp->writeSeq(hwp, 0x03, 0x00);
    hwp->writeSeq(hwp, 0x04, 0x0E);

    ViaSeqMask(hwp, 0x15, 0x02, 0x02);

    /* bpp */
    switch (pScrn->bitsPerPixel) {
        case 8:
            /* Only CLE266.AX use 6bits LUT. */
            if (pVia->Chipset == VIA_CLE266 && pVia->ChipRev < 15)
                ViaSeqMask(hwp, 0x15, 0x22, 0xFE);
            else
                ViaSeqMask(hwp, 0x15, 0xA2, 0xFE);
            break;
        case 16:
            ViaSeqMask(hwp, 0x15, 0xB6, 0xFE);
            break;
        case 24:
        case 32:
            ViaSeqMask(hwp, 0x15, 0xAE, 0xFE);
            break;
        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unhandled bitdepth: %d\n",
                       pScrn->bitsPerPixel);
            break;
    }

    switch (pVia->ChipId) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            break;
        default:
            ViaSeqMask(hwp, 0x16, 0x08, 0xBF);
            ViaSeqMask(hwp, 0x17, 0x1F, 0xFF);
            ViaSeqMask(hwp, 0x18, 0x4E, 0xFF);
            ViaSeqMask(hwp, 0x1A, 0x08, 0xFD);
            break;
    }

    /* Crtc registers */
    /* horizontal total : 4100 */
    temp = (mode->CrtcHTotal >> 3) - 5;
    hwp->writeCrtc(hwp, 0x00, temp & 0xFF);
    ViaCrtcMask(hwp, 0x36, temp >> 5, 0x08);

    /* horizontal address : 2048 */
    temp = (mode->CrtcHDisplay >> 3) - 1;
    hwp->writeCrtc(hwp, 0x01, temp & 0xFF);

    /* horizontal blanking start : 2048 */
    /* temp = (mode->CrtcHDisplay >> 3) - 1; */
    temp = (mode->CrtcHBlankStart >> 3) - 1;
    hwp->writeCrtc(hwp, 0x02, temp & 0xFF);
    /* If HblankStart has more bits anywhere, add them here */

    /* horizontal blanking end : start + 1025 */
    /* temp = (mode->CrtcHTotal >> 3) - 1; */
    temp = (mode->CrtcHBlankEnd >> 3) - 1;
    ViaCrtcMask(hwp, 0x03, temp, 0x1F);
    ViaCrtcMask(hwp, 0x05, temp << 2, 0x80);
    ViaCrtcMask(hwp, 0x33, temp >> 1, 0x20);

    /* CrtcHSkew ??? */

    /* horizontal sync start : 4095 */
    temp = mode->CrtcHSyncStart >> 3;
    hwp->writeCrtc(hwp, 0x04, temp & 0xFF);
    ViaCrtcMask(hwp, 0x33, temp >> 4, 0x10);

    /* horizontal sync end : start + 256 */
    temp = mode->CrtcHSyncEnd >> 3;
    ViaCrtcMask(hwp, 0x05, temp, 0x1F);

    /* vertical total : 2049 */
    temp = mode->CrtcVTotal - 2;
    hwp->writeCrtc(hwp, 0x06, temp & 0xFF);
    ViaCrtcMask(hwp, 0x07, temp >> 8, 0x01);
    ViaCrtcMask(hwp, 0x07, temp >> 4, 0x20);
    ViaCrtcMask(hwp, 0x35, temp >> 10, 0x01);

    /* vertical address : 2048 */
    temp = mode->CrtcVDisplay - 1;
    hwp->writeCrtc(hwp, 0x12, temp & 0xFF);
    ViaCrtcMask(hwp, 0x07, temp >> 7, 0x02);
    ViaCrtcMask(hwp, 0x07, temp >> 3, 0x40);
    ViaCrtcMask(hwp, 0x35, temp >> 8, 0x04);

    /* Primary starting address -> 0x00, adjustframe does the rest */
    hwp->writeCrtc(hwp, 0x0C, 0x00);
    hwp->writeCrtc(hwp, 0x0D, 0x00);
    ViaCrtcMask(hwp, 0x48, 0x00, 0x03); /* is this even possible on CLE266A ? */
    hwp->writeCrtc(hwp, 0x34, 0x00);

    /* vertical sync start : 2047 */
    temp = mode->CrtcVSyncStart;
    hwp->writeCrtc(hwp, 0x10, temp & 0xFF);
    ViaCrtcMask(hwp, 0x07, temp >> 6, 0x04);
    ViaCrtcMask(hwp, 0x07, temp >> 2, 0x80);
    ViaCrtcMask(hwp, 0x35, temp >> 9, 0x02);

    /* vertical sync end : start + 16 -- other bits someplace? */
    ViaCrtcMask(hwp, 0x11, mode->CrtcVSyncEnd, 0x0F);

    /* line compare: We are not doing splitscreen so 0x3FFF */
    hwp->writeCrtc(hwp, 0x18, 0xFF);
    ViaCrtcMask(hwp, 0x07, 0x10, 0x10);
    ViaCrtcMask(hwp, 0x09, 0x40, 0x40);
    ViaCrtcMask(hwp, 0x33, 0x07, 0x06);
    ViaCrtcMask(hwp, 0x35, 0x10, 0x10);

    /* zero Maximum scan line */
    ViaCrtcMask(hwp, 0x09, 0x00, 0x1F);
    hwp->writeCrtc(hwp, 0x14, 0x00);

    /* vertical blanking start : 2048 */
    /* temp = mode->CrtcVDisplay - 1; */
    temp = mode->CrtcVBlankStart - 1;
    hwp->writeCrtc(hwp, 0x15, temp & 0xFF);
    ViaCrtcMask(hwp, 0x07, temp >> 5, 0x08);
    ViaCrtcMask(hwp, 0x09, temp >> 4, 0x20);
    ViaCrtcMask(hwp, 0x35, temp >> 7, 0x08);

    /* vertical blanking end : start + 257 */
    /* temp = mode->CrtcVTotal - 1; */
    temp = mode->CrtcVBlankEnd - 1;
    hwp->writeCrtc(hwp, 0x16, temp);

    /* FIXME: check if this is really necessary here */
    switch (pVia->ChipId) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            break;
        default:
            /* some leftovers */
            hwp->writeCrtc(hwp, 0x08, 0x00);
            ViaCrtcMask(hwp, 0x32, 0, 0xFF);  /* ? */
            ViaCrtcMask(hwp, 0x33, 0, 0xC8);
            break;
    }

    /* offset */
    temp = (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3)) >> 3;
    /* Make sure that this is 32-byte aligned. */
    if (temp & 0x03) {
        temp += 0x03;
        temp &= ~0x03;
    }
    hwp->writeCrtc(hwp, 0x13, temp & 0xFF);
    ViaCrtcMask(hwp, 0x35, temp >> 3, 0xE0);

    /* fetch count */
    temp = (mode->CrtcHDisplay * (pScrn->bitsPerPixel >> 3)) >> 3;
    /* Make sure that this is 32-byte aligned. */
    if (temp & 0x03) {
        temp += 0x03;
        temp &= ~0x03;
    }

    hwp->writeSeq(hwp, 0x1C, ((temp >> 1)+1) & 0xFF);
    ViaSeqMask(hwp, 0x1D, temp >> 9, 0x03);

    switch (pVia->ChipId) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            break;
        default:
            /* some leftovers */
            ViaCrtcMask(hwp, 0x32, 0, 0xFF);
            ViaCrtcMask(hwp, 0x33, 0, 0xC8);
            break;
    }
}

void
ViaFirstCRTCSetStartingAddress(ScrnInfoPtr pScrn, int x, int y)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD32 Base;
    CARD32 tmp;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaFirstCRTCSetStartingAddress\n"));

    Base = (y * pScrn->displayWidth + x) * (pScrn->bitsPerPixel / 8);
    Base = Base >> 1;

    hwp->writeCrtc(hwp, 0x0C, (Base & 0xFF00) >> 8);
    hwp->writeCrtc(hwp, 0x0D, Base & 0xFF);
    /* FIXME The proper starting address for CR48 is 0x1F - Bits[28:24] */
    if (!(pVia->Chipset == VIA_CLE266 && CLE266_REV_IS_AX(pVia->ChipRev)))
        ViaCrtcMask(hwp, 0x48, Base >> 24, 0x0F);
    /* CR34 are fire bits. Must be written after CR0C CR0D CR48.  */
    hwp->writeCrtc(hwp, 0x34, (Base & 0xFF0000) >> 16);
}

void
ViaSecondCRTCSetStartingAddress(ScrnInfoPtr pScrn, int x, int y)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD32 Base;
    CARD32 tmp;

    Base = (y * pScrn->displayWidth + x) * (pScrn->bitsPerPixel / 8);
    Base = (Base + pScrn->fbOffset) >> 3;

    tmp = hwp->readCrtc(hwp, 0x62) & 0x01;
    tmp |= (Base & 0x7F) << 1;
    hwp->writeCrtc(hwp, 0x62, tmp);

    hwp->writeCrtc(hwp, 0x63, (Base & 0x7F80) >> 7);
    hwp->writeCrtc(hwp, 0x64, (Base & 0x7F8000) >> 15);
    hwp->writeCrtc(hwp, 0xA3, (Base & 0x03800000) >> 23);
}

void
ViaSecondCRTCHorizontalQWCount(ScrnInfoPtr pScrn, int width)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD16 temp;

    /* fetch count */
    temp = (width * (pScrn->bitsPerPixel >> 3)) >> 3;
    /* Make sure that this is 32-byte aligned. */
    if (temp & 0x03) {
        temp += 0x03;
        temp &= ~0x03;
    }
    hwp->writeCrtc(hwp, 0x65, (temp >> 1) & 0xFF);
    ViaCrtcMask(hwp, 0x67, temp >> 7, 0x0C);
}

void
ViaSecondCRTCHorizontalOffset(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD16 temp;

    /* offset */
    temp = (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3)) >> 3;
    /* Make sure that this is 32-byte aligned. */
    if (temp & 0x03) {
        temp += 0x03;
        temp &= ~0x03;
        }

    hwp->writeCrtc(hwp, 0x66, temp & 0xFF);
    ViaCrtcMask(hwp, 0x67, temp >> 8, 0x03);
}

void
ViaSecondCRTCSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD16 temp;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "mode: %p\n", mode);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "mode->name: %p\n", mode->name);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "mode->name: %s\n", mode->name);


    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaSecondCRTCSetMode\n"));
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Setting up %s\n", mode->name));
    /* bpp */
    switch (pScrn->bitsPerPixel) {
        case 8:
            ViaCrtcMask(hwp, 0x67, 0x00, 0xC0);
            break;
        case 16:
            ViaCrtcMask(hwp, 0x67, 0x40, 0xC0);
            break;
        case 24:
        case 32:
            ViaCrtcMask(hwp, 0x67, 0xC0, 0xC0);
            break;
        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unhandled bitdepth: %d\n",
                       pScrn->bitsPerPixel);
            break;
    }

    switch (pVia->ChipId) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            break;
        default:
            ViaSeqMask(hwp, 0x16, 0x08, 0xBF);
            ViaSeqMask(hwp, 0x17, 0x1F, 0xFF);
            ViaSeqMask(hwp, 0x18, 0x4E, 0xFF);
            ViaSeqMask(hwp, 0x1A, 0x08, 0xFD);
            break;
    }

    /* Crtc registers */
    /* horizontal total : 4096 */
    temp = mode->CrtcHTotal - 1;
    hwp->writeCrtc(hwp, 0x50, temp & 0xFF);
    ViaCrtcMask(hwp, 0x55, temp >> 8, 0x0F);

    /* horizontal address : 2048 */
    temp = mode->CrtcHDisplay - 1;
    hwp->writeCrtc(hwp, 0x51, temp & 0xFF);
    ViaCrtcMask(hwp, 0x55, temp >> 4, 0x70);

    /* horizontal blanking start : 2048 */
    /* temp = mode->CrtcHDisplay - 1; */
    temp = mode->CrtcHBlankStart - 1;
    hwp->writeCrtc(hwp, 0x52, temp & 0xFF);
    ViaCrtcMask(hwp, 0x54, temp >> 8, 0x07);

    /* horizontal blanking end : 4096 */
    /* temp = mode->CrtcHTotal - 1; */
    temp = mode->CrtcHBlankEnd - 1;
    hwp->writeCrtc(hwp, 0x53, temp & 0xFF);
    ViaCrtcMask(hwp, 0x54, temp >> 5, 0x38);
    ViaCrtcMask(hwp, 0x5D, temp >> 5, 0x40);

    /* horizontal sync start : 2047 */
    temp = mode->CrtcHSyncStart;
    hwp->writeCrtc(hwp, 0x56, temp & 0xFF);
    ViaCrtcMask(hwp, 0x54, temp >> 2, 0xC0);
    ViaCrtcMask(hwp, 0x5C, temp >> 3, 0x80);

    if (pVia->ChipId != VIA_CLE266 && pVia->ChipId != VIA_KM400)
        ViaCrtcMask(hwp, 0x5D, temp >> 4, 0x80);

    /* horizontal sync end : sync start + 512 */
    temp = mode->CrtcHSyncEnd;
    hwp->writeCrtc(hwp, 0x57, temp & 0xFF);
    ViaCrtcMask(hwp, 0x5C, temp >> 2, 0x40);

    /* vertical total : 2048 */
    temp = mode->CrtcVTotal - 1;
    hwp->writeCrtc(hwp, 0x58, temp & 0xFF);
    ViaCrtcMask(hwp, 0x5D, temp >> 8, 0x07);


    /* vertical address : 2048 */
    temp = mode->CrtcVDisplay - 1;
    hwp->writeCrtc(hwp, 0x59, temp & 0xFF);
    ViaCrtcMask(hwp, 0x5D, temp >> 5, 0x38);

    /* vertical blanking start : 2048 */
    /* temp = mode->CrtcVDisplay - 1; */
    temp = mode->CrtcVBlankStart - 1;
    hwp->writeCrtc(hwp, 0x5A, temp & 0xFF);
    ViaCrtcMask(hwp, 0x5C, temp >> 8, 0x07);

    /* vertical blanking end : 2048 */
    /* temp = mode->CrtcVTotal - 1; */
    temp = mode->CrtcVBlankEnd - 1;
    hwp->writeCrtc(hwp, 0x5B, temp & 0xFF);
    ViaCrtcMask(hwp, 0x5C, temp >> 5, 0x38);

    /* vertical sync start : 2047 */
    temp = mode->CrtcVSyncStart;
    hwp->writeCrtc(hwp, 0x5E, temp & 0xFF);
    ViaCrtcMask(hwp, 0x5F, temp >> 3, 0xE0);

    /* vertical sync end : start + 32 */
    temp = mode->CrtcVSyncEnd;
    ViaCrtcMask(hwp, 0x5F, temp, 0x1F);

    switch (pVia->ChipId) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            break;
        default:
            /* some leftovers */
            hwp->writeCrtc(hwp, 0x08, 0x00);
            ViaCrtcMask(hwp, 0x32, 0, 0xFF);  /* ? */
            ViaCrtcMask(hwp, 0x33, 0, 0xC8);
            break;
    }

    ViaSecondCRTCHorizontalOffset(pScrn);
    ViaSecondCRTCHorizontalQWCount(pScrn, mode->CrtcHDisplay);
}

/*
 * Checks for limitations imposed by the available VGA timing registers.
 */
ModeStatus
ViaFirstCRTCModeValid(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaFirstCRTCModeValid\n"));

    if (mode->CrtcHTotal > 4100)
        return MODE_BAD_HVALUE;

    if (mode->CrtcHDisplay > 2048)
        return MODE_BAD_HVALUE;

    if (mode->CrtcHBlankStart > 2048)
        return MODE_BAD_HVALUE;

    if ((mode->CrtcHBlankEnd - mode->CrtcHBlankStart) > 1025)
        return MODE_HBLANK_WIDE;

    if (mode->CrtcHSyncStart > 4095)
        return MODE_BAD_HVALUE;

    if ((mode->CrtcHSyncEnd - mode->CrtcHSyncStart) > 256)
        return MODE_HSYNC_WIDE;

    if (mode->CrtcVTotal > 2049)
        return MODE_BAD_VVALUE;

    if (mode->CrtcVDisplay > 2048)
        return MODE_BAD_VVALUE;

    if (mode->CrtcVSyncStart > 2047)
        return MODE_BAD_VVALUE;

    if ((mode->CrtcVSyncEnd - mode->CrtcVSyncStart) > 16)
        return MODE_VSYNC_WIDE;

    if (mode->CrtcVBlankStart > 2048)
        return MODE_BAD_VVALUE;

    if ((mode->CrtcVBlankEnd - mode->CrtcVBlankStart) > 257)
        return MODE_VBLANK_WIDE;

    return MODE_OK;
}

ModeStatus
ViaSecondCRTCModeValid(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaSecondCRTCModeValid\n"));

    if (mode->CrtcHTotal > 4096)
        return MODE_BAD_HVALUE;

    if (mode->CrtcHDisplay > 2048)
        return MODE_BAD_HVALUE;

    if (mode->CrtcHBlankStart > 2048)
        return MODE_BAD_HVALUE;

    if (mode->CrtcHBlankEnd > 4096)
        return MODE_HBLANK_WIDE;

    if (mode->CrtcHSyncStart > 2047)
        return MODE_BAD_HVALUE;

    if ((mode->CrtcHSyncEnd - mode->CrtcHSyncStart) > 512)
        return MODE_HSYNC_WIDE;

    if (mode->CrtcVTotal > 2048)
        return MODE_BAD_VVALUE;

    if (mode->CrtcVDisplay > 2048)
        return MODE_BAD_VVALUE;

    if (mode->CrtcVBlankStart > 2048)
        return MODE_BAD_VVALUE;

    if (mode->CrtcVBlankEnd > 2048)
        return MODE_VBLANK_WIDE;

    if (mode->CrtcVSyncStart > 2047)
        return MODE_BAD_VVALUE;

    if ((mode->CrtcVSyncEnd - mode->CrtcVSyncStart) > 32)
        return MODE_VSYNC_WIDE;

    return MODE_OK;
}

/*
 * Not tested yet
 */
void
ViaShadowCRTCSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaShadowCRTCSetMode\n"));

    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD16 temp;

    temp = (mode->CrtcHTotal >> 3) - 5;
    hwp->writeCrtc(hwp, 0x6D, temp & 0xFF);
    ViaCrtcMask(hwp, 0x71, temp >> 5, 0x08);

    temp = (mode->CrtcHBlankEnd >> 3) - 1;
    hwp->writeCrtc(hwp, 0x6E, temp & 0xFF);

    temp = mode->CrtcVTotal - 2;
    hwp->writeCrtc(hwp, 0x6F, temp & 0xFF);
    ViaCrtcMask(hwp, 0x71, temp >> 8, 0x07);

    temp = mode->CrtcVDisplay - 1;
    hwp->writeCrtc(hwp, 0x70, temp & 0xFF);
    ViaCrtcMask(hwp, 0x71, temp >> 4, 0x70);

    temp = mode->CrtcVBlankStart - 1;
    hwp->writeCrtc(hwp, 0x72, temp & 0xFF);
    ViaCrtcMask(hwp, 0x74, temp >> 4, 0x70);

    temp = mode->CrtcVTotal - 1;
    hwp->writeCrtc(hwp, 0x73, temp & 0xFF);
    ViaCrtcMask(hwp, 0x74, temp >> 8, 0x07);

    ViaCrtcMask(hwp, 0x76, mode->CrtcVSyncEnd, 0x0F);

    temp = mode->CrtcVSyncStart;
    hwp->writeCrtc(hwp, 0x75, temp & 0xFF);
    ViaCrtcMask(hwp, 0x76, temp >> 4, 0x70);
}

void
viaCursorStore(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaCursorStore\n"));

    if (pVia->CursorPipe) {
                pVia->CursorControl1 = VIAGETREG(pVia->CursorRegControl);
    } else {
                pVia->CursorControl0 = VIAGETREG(pVia->CursorRegControl);
    }

    pVia->CursorTransparentKey = VIAGETREG(pVia->CursorRegTransKey);


    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
                if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                        pVia->CursorPrimHiInvtColor = VIAGETREG(VIA_REG_PRIM_HI_INVTCOLOR);
                        pVia->CursorV327HiInvtColor = VIAGETREG(VIA_REG_V327_HI_INVTCOLOR);
                }
                if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                /* TODO add saves here */
                }
                pVia->CursorFifo = VIAGETREG(pVia->CursorRegFifo);
                break;
        default:
                /* TODO add saves here */
                break;
    }
}

void
viaCursorRestore(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaCursorRestore\n"));

    if (pVia->CursorPipe) {
                VIASETREG(pVia->CursorRegControl, pVia->CursorControl1);
    } else {
                VIASETREG(pVia->CursorRegControl, pVia->CursorControl0);
    }

    VIASETREG(pVia->CursorRegBase, pVia->cursorOffset);

    VIASETREG(pVia->CursorRegTransKey, pVia->CursorTransparentKey);


    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
                if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                        VIASETREG(VIA_REG_PRIM_HI_INVTCOLOR, pVia->CursorPrimHiInvtColor);
                        VIASETREG(VIA_REG_V327_HI_INVTCOLOR, pVia->CursorV327HiInvtColor);
                }
                if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                        /* TODO add real restores here */
                        VIASETREG(VIA_REG_HI_INVTCOLOR, 0X00FFFFFF);
                        VIASETREG(VIA_REG_ALPHA_PREFIFO, 0xE0000);
                }
                VIASETREG(pVia->CursorRegFifo, pVia->CursorFifo);
                break;
        default:
                /* TODO add real restores here */
                VIASETREG(VIA_REG_ALPHA_PREFIFO, 0xE0000);
                VIASETREG(pVia->CursorRegFifo, 0xE0F0000);
    }
}

/*
 * display the current cursor
 */

void
viaShowCursor(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    switch(pVia->Chipset) {
        case VIA_CX700:
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
             if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                 VIASETREG(VIA_REG_HI_CONTROL0, 0x36000005);
             }
             if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                 VIASETREG(VIA_REG_HI_CONTROL1, 0xb6000005);
             }
             break;

        default:
            /*temp = 0x76000005;
              temp =
                (1 << 30) |
                (1 << 29) |
                (1 << 28) |
                (1 << 26) |
                (1 << 25) |
                (1 << 26) |
                (1 << 25) |
                (1 <<  2) |
                (1 <<  0);
            */

            /* Duoview */
            if (pVia->CursorPipe) {
                /* Mono Cursor Display Path [bit31]: Secondary */
                /* FIXME For CLE266 and KM400 try to enable 32x32 cursor size [bit1] */
                VIASETREG(VIA_REG_ALPHA_CONTROL, 0xF6000005);
            } else {
                /* Mono Cursor Display Path [bit31]: Primary */
                VIASETREG(VIA_REG_ALPHA_CONTROL, 0x76000005);
            }
    }
}

/* hide the current cursor */
void
viaHideCursor(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 temp;

    switch(pVia->Chipset) {
        case VIA_CX700:
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
             if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                 temp = VIAGETREG(VIA_REG_HI_CONTROL0);
                 VIASETREG(VIA_REG_HI_CONTROL0, temp & 0xFFFFFFFA);
             }
             if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                 temp = VIAGETREG(VIA_REG_HI_CONTROL1);
                 VIASETREG(VIA_REG_HI_CONTROL1, temp & 0xFFFFFFFA);
             }
             break;

        default:
             temp = VIAGETREG(VIA_REG_ALPHA_CONTROL);
             /* Hardware cursor disable [bit0] */
             VIASETREG(VIA_REG_ALPHA_CONTROL, temp & 0xFFFFFFFA);
    }
}

/*
    Set the cursor position to (x,y).  X and/or y may be negative
    indicating that the cursor image is partially offscreen on
    the left and/or top edges of the screen.
*/
static void
viaSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    VIAPtr pVia = VIAPTR(pScrn);
    unsigned xoff, yoff;

    if (x < 0) {
        xoff = ((-x) & 0xFE);
        x = 0;
    } else {
        xoff = 0;
    }

    if (y < 0) {
        yoff = ((-y) & 0xFE);
        y = 0;
    } else {
        yoff = 0;
    }

    switch(pVia->Chipset) {
        case VIA_CX700:
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
             if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                 VIASETREG(VIA_REG_HI_POS0,    ((x    << 16) | (y    & 0x07ff)));
                 VIASETREG(VIA_REG_HI_OFFSET0, ((xoff << 16) | (yoff & 0x07ff)));
             }
             if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                 VIASETREG(VIA_REG_HI_POS1,    ((x    << 16) | (y    & 0x07ff)));
                 VIASETREG(VIA_REG_HI_OFFSET1, ((xoff << 16) | (yoff & 0x07ff)));
             }
             break;

        default:
            VIASETREG(VIA_REG_ALPHA_POS,    ((x    << 16) | (y    & 0x07ff)));
            VIASETREG(VIA_REG_ALPHA_OFFSET, ((xoff << 16) | (yoff & 0x07ff)));
    }
}

static Bool
viaUseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    return (pVia->hwcursor
            && pVia->CursorARGBSupported
            && pCurs->bits->width <= pVia->CursorMaxWidth
            && pCurs->bits->height <= pVia->CursorMaxHeight);
}

/*
    If the driver is unable to use a hardware cursor for reasons
    other than the cursor being larger than the maximum specified
    in the MaxWidth or MaxHeight field below, it can supply the
    UseHWCursor function.  If UseHWCursor is provided by the driver,
    it will be called whenever the cursor shape changes or the video
    mode changes.  This is useful for when the hardware cursor cannot
    be used in interlaced or doublescan modes.
*/
static Bool
viaUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    return (pVia->hwcursor
            /* Can't enable HW cursor on both CRTCs at the same time. */
            && !(pVia->pBIOSInfo->FirstCRTC->IsActive
                 && pVia->pBIOSInfo->SecondCRTC->IsActive)
            && pCurs->bits->width <= pVia->CursorMaxWidth
            && pCurs->bits->height <= pVia->CursorMaxHeight);
}

/*
    Load Mono Cursor Image
*/
static CARD32 mono_cursor_color[] = {
        0x00000000,
        0x00000000,
        0xffffffff,
        0xff000000,
};

static void
viaLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 temp;
    CARD32 *dst;
    CARD8 chunk;
    int i, j;

    pVia->CursorARGB = FALSE;

    dst = (CARD32*)(pVia->cursorMap);

    if (pVia->CursorARGBSupported) {
#define ARGB_PER_CHUNK  (8 * sizeof (chunk) / 2)
                for (i = 0; i < (pVia->CursorMaxWidth * pVia->CursorMaxHeight / ARGB_PER_CHUNK); i++) {
                    chunk = *src++;
                    for (j = 0; j < ARGB_PER_CHUNK; j++, chunk >>= 2)
                        *dst++ = mono_cursor_color[chunk & 3];
                }

                pVia->CursorFG = mono_cursor_color[3];
                pVia->CursorBG = mono_cursor_color[2];
    } else {
        memcpy(dst, (CARD8*)src, pVia->CursorSize);
    }
    switch(pVia->Chipset) {
        case VIA_CX700:
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
             if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                 temp = VIAGETREG(VIA_REG_HI_CONTROL0);
                 VIASETREG(VIA_REG_HI_CONTROL0, temp & 0xFFFFFFFE);
             }
             if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                 temp = VIAGETREG(VIA_REG_HI_CONTROL1);
                 VIASETREG(VIA_REG_HI_CONTROL1, temp & 0xFFFFFFFE);
             }
             break;

        default:
             temp = VIAGETREG(VIA_REG_ALPHA_CONTROL);
             VIASETREG(VIA_REG_ALPHA_CONTROL, temp);
    }
}

/*
    Set the cursor foreground and background colors.  In 8bpp, fg and
    bg are indices into the current colormap unless the
    HARDWARE_CURSOR_TRUECOLOR_AT_8BPP flag is set.  In that case
    and in all other bpps the fg and bg are in 8-8-8 RGB format.
*/

static void
viaSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 pixel;
    CARD32 temp;
    CARD32 *dst;
    int i;

    if (pVia->CursorFG)
        return;

    fg |= 0xff000000;
    bg |= 0xff000000;

    /* Don't recolour the image if we don't have to. */
    if (fg == pVia->CursorFG && bg == pVia->CursorBG)
        return;

    dst = (CARD32*)pVia->cursorMap;
    for (i = 0; i < pVia->CursorMaxWidth * pVia->CursorMaxHeight; i++, dst++)
        if ((pixel = *dst))
            *dst = (pixel == pVia->CursorFG) ? fg : bg;

    pVia->CursorFG = fg;
    pVia->CursorBG = bg;

    switch(pVia->Chipset) {
        case VIA_CX700:
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
             if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                 temp = VIAGETREG(VIA_REG_HI_CONTROL0);
                 VIASETREG(VIA_REG_HI_CONTROL0, temp & 0xFFFFFFFE);
             }
             if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                 temp = VIAGETREG(VIA_REG_HI_CONTROL1);
                 VIASETREG(VIA_REG_HI_CONTROL1, temp & 0xFFFFFFFE);
             }
             break;
        default:
             temp = VIAGETREG(VIA_REG_ALPHA_CONTROL);
             VIASETREG(VIA_REG_ALPHA_CONTROL, temp & 0xFFFFFFFE);
    }
}

static void
viaLoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int x, y, w, h;
    CARD32 *image = (CARD32*)pCurs->bits->argb;
    CARD32 *dst = (CARD32*)pVia->cursorMap;

    pVia->CursorARGB = TRUE;

    w = pCurs->bits->width;
    h = pCurs->bits->height;

    for (y = 0; y < h; y++) {

        for (x = 0; x < w; x++)
            *dst++ = *image++;
        /* pad to the right with transparent */
        for (; x < pVia->CursorMaxHeight; x++)
            *dst++ = 0;
    }

    /* pad below with transparent */
    for (; y < pVia->CursorMaxHeight; y++)
        for (x = 0; x < pVia->CursorMaxWidth; x++)
            *dst++ = 0;

}

void
viaCursorSetFB(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if ((pVia->FBFreeEnd - pVia->FBFreeStart) > pVia->CursorSize) {
        pVia->CursorStart = pVia->FBFreeEnd - pVia->CursorSize;
        pVia->FBFreeEnd = pVia->CursorStart;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CursorStart: 0x%x\n", pVia->CursorStart);
    }
}

Bool
UMSHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

        switch (pVia->Chipset) {
                case VIA_CLE266:
                case VIA_KM400:
                        /* FIXME Mono HW Cursors not working */
                        pVia->hwcursor = FALSE;
                        pVia->CursorARGBSupported = FALSE;
                        pVia->CursorMaxWidth = 32;
                        pVia->CursorMaxHeight = 32;
                        pVia->CursorSize = ((pVia->CursorMaxWidth * pVia->CursorMaxHeight) / 8) * 2;
                        break;
                default:
                        pVia->CursorARGBSupported = TRUE;
                        pVia->CursorMaxWidth = 64;
                        pVia->CursorMaxHeight = 64;
                        pVia->CursorSize = pVia->CursorMaxWidth * (pVia->CursorMaxHeight + 1) << 2;
                        break;
    }

    if (pVia->NoAccel)
        viaCursorSetFB(pScrn);

    pVia->cursorMap = pVia->FBBase + pVia->CursorStart;

    if (pVia->cursorMap == NULL)
                return FALSE;

    pVia->cursorOffset = pScrn->fbOffset + pVia->CursorStart;
    memset(pVia->cursorMap, 0x00, pVia->CursorSize);

    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
                        if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                                pVia->CursorRegControl  = VIA_REG_HI_CONTROL0;
                                pVia->CursorRegBase     = VIA_REG_HI_BASE0;
                                pVia->CursorRegPos      = VIA_REG_HI_POS0;
                                pVia->CursorRegOffset   = VIA_REG_HI_OFFSET0;
                                pVia->CursorRegFifo     = VIA_REG_HI_FIFO0;
                                pVia->CursorRegTransKey = VIA_REG_HI_TRANSKEY0;
                        }
                        if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                                pVia->CursorRegControl  = VIA_REG_HI_CONTROL1;
                                pVia->CursorRegBase     = VIA_REG_HI_BASE1;
                                pVia->CursorRegPos      = VIA_REG_HI_POS1;
                                pVia->CursorRegOffset   = VIA_REG_HI_OFFSET1;
                                pVia->CursorRegFifo     = VIA_REG_HI_FIFO1;
                                pVia->CursorRegTransKey = VIA_REG_HI_TRANSKEY1;
                        }
                        break;
                default:
                        pVia->CursorRegControl = VIA_REG_ALPHA_CONTROL;
                        pVia->CursorRegBase = VIA_REG_ALPHA_BASE;
                        pVia->CursorRegPos = VIA_REG_ALPHA_POS;
                        pVia->CursorRegOffset = VIA_REG_ALPHA_OFFSET;
                        pVia->CursorRegFifo = VIA_REG_ALPHA_FIFO;
                        pVia->CursorRegTransKey = VIA_REG_ALPHA_TRANSKEY;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAHWCursorInit\n"));
    infoPtr = xf86CreateCursorInfoRec();
    if (!infoPtr)
        return FALSE;

    pVia->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = pVia->CursorMaxWidth;
    infoPtr->MaxHeight = pVia->CursorMaxHeight;
    infoPtr->Flags = (HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1 |
                      HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
                      HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                      0);

    infoPtr->SetCursorColors = viaSetCursorColors;
    infoPtr->SetCursorPosition = viaSetCursorPosition;
    infoPtr->LoadCursorImage = viaLoadCursorImage;
    infoPtr->HideCursor = viaHideCursor;
    infoPtr->ShowCursor = viaShowCursor;
    infoPtr->UseHWCursor = viaUseHWCursor;

    /* ARGB Cursor init */
    infoPtr->UseHWCursorARGB = viaUseHWCursorARGB;
    if (pVia->CursorARGBSupported) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "HWCursor ARGB enabled\n"));
        infoPtr->LoadCursorARGB = viaLoadCursorARGB;
    }

    /* Set cursor location in frame buffer. */
    VIASETREG(VIA_REG_CURSOR_MODE, pVia->cursorOffset);

	if (pVia->pBIOSInfo->lvds)
		pVia->CursorPipe = (pVia->pBIOSInfo->lvds->status == XF86OutputStatusConnected) ? 1 : 0;

    /* Init HI_X0 */
    VIASETREG(pVia->CursorRegControl, 0);
    VIASETREG(pVia->CursorRegBase, pVia->cursorOffset);
    VIASETREG(pVia->CursorRegTransKey, 0);

    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
                        if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                                VIASETREG(VIA_REG_PRIM_HI_INVTCOLOR, 0x00FFFFFF);
                                VIASETREG(VIA_REG_V327_HI_INVTCOLOR, 0x00FFFFFF);
                                VIASETREG(pVia->CursorRegFifo, 0x0D000D0F);
                        }
                        if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                                VIASETREG(VIA_REG_HI_INVTCOLOR, 0X00FFFFFF);
                                VIASETREG(VIA_REG_ALPHA_PREFIFO, 0xE0000);
                                VIASETREG(pVia->CursorRegFifo, 0xE0F0000);

                                /* Just in case */
                                VIASETREG(VIA_REG_HI_BASE0, pVia->cursorOffset);
                        }
                        break;
        default:
                        VIASETREG(VIA_REG_HI_INVTCOLOR, 0X00FFFFFF);
                        VIASETREG(VIA_REG_ALPHA_PREFIFO, 0xE0000);
                        VIASETREG(pVia->CursorRegFifo, 0xE0F0000);
        }

    return xf86InitCursor(pScreen, infoPtr);
}

static void
via_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

    if (pVia->pVbe) {
        ViaVbeDPMS(pScrn, mode);
    } else {

        switch (mode) {
            case DPMSModeOn:
                if (pBIOSInfo->Simultaneous->IsActive)
                    ViaDisplayEnableSimultaneous(pScrn);
                break;

            case DPMSModeStandby:
            case DPMSModeSuspend:
            case DPMSModeOff:
                if (pBIOSInfo->Simultaneous->IsActive)
                    ViaDisplayDisableSimultaneous(pScrn);
                break;

			default:
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid DPMS mode %d\n",
                           mode);
                break;
        }

		vgaHWSaveScreen(pScrn->pScreen, mode);
    }
}

static void
via_crtc_save(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	VIAPtr pVia = VIAPTR(pScrn);

	if (pVia->pVbe && pVia->vbeSR) {
		ViaVbeSaveRestore(pScrn, MODE_SAVE);
	} else {
		VIASave(pScrn);
	}

	vgaHWUnlock(hwp);
}

static void
via_crtc_restore(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	VIAPtr pVia = VIAPTR(pScrn);

	if (pVia->pVbe && pVia->vbeSR)
		ViaVbeSaveRestore(pScrn, MODE_RESTORE);
	else
		VIARestore(pScrn);

	vgaHWLock(hwp);
}

static Bool
via_crtc_lock(xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    VIAPtr pVia = VIAPTR(pScrn);
    Bool ret = TRUE;

#ifdef XF86DRI
    if (pVia->directRenderingType)
        DRILock(screenInfo.screens[pScrn->scrnIndex], 0);
#endif

    viaAccelSync(pScrn);

#ifdef XF86DRI
    if (pVia->directRenderingType)
        VIADRIRingBufferCleanup(pScrn);
#endif

    if (pVia->VQEnable)
        viaDisableVQ(pScrn);

    return ret;
}

static void
via_crtc_unlock(xf86CrtcPtr crtc)
{
#ifdef XF86DRI
    ScrnInfoPtr pScrn = crtc->scrn;
    VIAPtr pVia = VIAPTR(pScrn);

    if (pVia->directRenderingType) {
        kickVblank(pScrn);
        VIADRIRingBufferInit(pScrn);
        DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
    }
#endif
}

static Bool
via_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
                        DisplayModePtr adjusted_mode)
{
    return TRUE;
}

static void
via_crtc_prepare (xf86CrtcPtr crtc)
{
}

static void
via_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode,
					DisplayModePtr adjusted_mode,
					int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	VIAPtr pVia = VIAPTR(pScrn);

	pVia->OverlaySupported = FALSE;
	pScrn->vtSema = TRUE;

	if (!pVia->pVbe) {

		if (!vgaHWInit(pScrn, adjusted_mode))
			return;

		if (pVia->UseLegacyModeSwitch) {
			if (!pVia->IsSecondary)
				ViaModePrimaryLegacy(crtc, adjusted_mode);
			else
				ViaModeSecondaryLegacy(crtc, adjusted_mode);
		} else {
			ViaCRTCInit(pScrn);
			ViaModeSet(crtc, adjusted_mode);
		}

	} else {

		if (!ViaVbeSetMode(pScrn, adjusted_mode))
			return;

		/*
		 * FIXME: pVia->IsSecondary is not working here.  We should be able
		 * to detect when the display is using the secondary head.
		 * TODO: This should be enabled for other chipsets as well.
		 */
		if (pVia->pBIOSInfo->lvds && pVia->pBIOSInfo->lvds->status == XF86OutputStatusConnected) {
			switch (pVia->Chipset) {
			case VIA_P4M900:
			case VIA_VX800:
			case VIA_VX855:
			case VIA_VX900:
				/*
				 * Since we are using virtual, we need to adjust
				 * the offset to match the framebuffer alignment.
				 */
				if (pScrn->displayWidth != adjusted_mode->CrtcHDisplay)
					ViaSecondCRTCHorizontalOffset(pScrn);
				break;
			}
		}
	}

	/* Enable the graphics engine. */
	if (!pVia->NoAccel)
		VIAInitialize3DEngine(pScrn);

	pScrn->AdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
}

static void
via_crtc_commit (xf86CrtcPtr crtc)
{
}

static void
via_crtc_set_origin(xf86CrtcPtr crtc, int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    int scrnIndex = pScrn->scrnIndex;
    VIAPtr pVia = VIAPTR(pScrn);

    if (pVia->pVbe) {
        ViaVbeAdjustFrame(scrnIndex, x, y);
    } else {
        if (pVia->UseLegacyModeSwitch) {
            if (!pVia->IsSecondary)
                ViaFirstCRTCSetStartingAddress(pScrn, x, y);
            else
                ViaSecondCRTCSetStartingAddress(pScrn, x, y);
        } else {
            if (pVia->pBIOSInfo->FirstCRTC->IsActive)
                ViaFirstCRTCSetStartingAddress(pScrn, x, y);

            if (pVia->pBIOSInfo->SecondCRTC->IsActive)
                ViaSecondCRTCSetStartingAddress(pScrn, x, y);
        }
    }

    VIAVidAdjustFrame(pScrn, x, y);
}

static void *
via_crtc_shadow_allocate (xf86CrtcPtr crtc, int width, int height)
{
    return NULL;
}

static PixmapPtr
via_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
    return NULL;
}

static void
via_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
}

static void
via_crtc_set_cursor_colors (xf86CrtcPtr crtc, int bg, int fg)
{
    ScrnInfoPtr pScrn = crtc->scrn;

    viaSetCursorColors(pScrn, bg, fg);
}

static void
via_crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;

    viaSetCursorPosition(pScrn, x, y);
}

static void
via_crtc_show_cursor (xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;

    viaShowCursor(pScrn);
}

static void
via_crtc_hide_cursor (xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;

    viaHideCursor(pScrn);
}

static void
via_crtc_load_cursor_argb (xf86CrtcPtr crtc, CARD32 *image)
{
    ScrnInfoPtr pScrn = crtc->scrn;

    viaLoadCursorImage(pScrn, (unsigned char *)image);
}

static const xf86CrtcFuncsRec via_crtc_funcs = {
	.dpms				 = via_crtc_dpms,
	.save				 = via_crtc_save,
	.restore			 = via_crtc_restore,
	.lock				 = via_crtc_lock,
	.unlock				 = via_crtc_unlock,
	.mode_fixup			 = via_crtc_mode_fixup,
	.prepare			 = via_crtc_prepare,
	.mode_set			 = via_crtc_mode_set,
	.commit				 = via_crtc_commit,
	.gamma_set			 = via_crtc_gamma_set,
	.shadow_create		 = via_crtc_shadow_create,
	.shadow_allocate	 = via_crtc_shadow_allocate,
	.shadow_destroy		 = via_crtc_shadow_destroy,
	.set_cursor_colors	 = via_crtc_set_cursor_colors,
	.set_cursor_position = via_crtc_set_cursor_position,
	.show_cursor		 = via_crtc_show_cursor,
	.hide_cursor		 = via_crtc_hide_cursor,
	.load_cursor_argb	 = via_crtc_load_cursor_argb,
	.set_origin			 = via_crtc_set_origin,
	.destroy			 = NULL,
};

Bool
UMSCrtcInit(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
	int max_pitch, max_height, i;
    VIAPtr pVia = VIAPTR(pScrn);
    ClockRangePtr clockRanges;
    VIABIOSInfoPtr pBIOSInfo;
    xf86CrtcPtr crtc;

    /* Read memory bandwidth from registers. */
    pVia->MemClk = hwp->readCrtc(hwp, 0x3D) >> 4;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "Detected MemClk %d\n", pVia->MemClk));
    if (pVia->MemClk >= VIA_MEM_END) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Unknown Memory clock: %d\n", pVia->MemClk);
        pVia->MemClk = VIA_MEM_END - 1;
    }
    pBIOSInfo = pVia->pBIOSInfo;
    pBIOSInfo->Bandwidth = ViaGetMemoryBandwidth(pScrn);

    if (pBIOSInfo->TVType == TVTYPE_NONE) {
        /* Use jumper to determine TV type. */
        if (hwp->readCrtc(hwp, 0x3B) & 0x02) {
            pBIOSInfo->TVType = TVTYPE_PAL;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                             "Detected TV standard: PAL.\n"));
        } else {
            pBIOSInfo->TVType = TVTYPE_NTSC;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                             "Detected TV standard: NTSC.\n"));
        }
    }

    if (pVia->hwcursor) {
        if (!xf86LoadSubModule(pScrn, "ramdac")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    if (!xf86LoadSubModule(pScrn, "i2c")) {
        VIAFreeRec(pScrn);
        return FALSE;
    } else {
        ViaI2CInit(pScrn);
    }

    if (!xf86LoadSubModule(pScrn, "ddc")) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    pVia->pVbe = NULL;
    if (pVia->useVBEModes) {
        /* VBE doesn't properly initialise int10 itself. */
        if (xf86LoadSubModule(pScrn, "int10")
            && xf86LoadSubModule(pScrn, "vbe")) {
            pVia->pVbe = VBEExtendedInit(NULL, pVia->EntityIndex,
                                         SET_BIOS_SCRATCH |
                                         RESTORE_BIOS_SCRATCH);
        }

        if (!pVia->pVbe)
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "VBE initialisation failed."
                       " Using builtin code to set modes.\n");
    }

    /* Might not belong here temporary fix for bug fix */
    xf86CrtcConfigInit(pScrn, &via_xf86crtc_config_funcs);

	for (i = 0; i < 2; i++) {
		crtc = xf86CrtcCreate(pScrn, &via_crtc_funcs);

		if (!crtc) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86CrtcCreate failed.\n");
			return FALSE;
		}
	}

	switch (pVia->Chipset) {
	case VIA_CLE266:
	case VIA_KM400:
	case VIA_K8M800:
	case VIA_PM800:
	case VIA_VM800:
		max_pitch = 3344;
		max_height = 2508;
		break;

	case VIA_CX700:
	case VIA_K8M890:
	case VIA_P4M890:
	case VIA_P4M900:
		max_pitch = 8192/(pScrn->bitsPerPixel >> 3)-1;
		max_height = max_pitch;
		break;

	default:
		max_pitch = 16384/(pScrn->bitsPerPixel >> 3)-1;
		max_height = max_pitch;
		break;
	}

	xf86CrtcSetSizeRange(pScrn, 320, 200, max_pitch, max_height);

	ViaOutputsDetect(pScrn);
	if (!ViaOutputsSelect(pScrn)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No outputs possible.\n");
		VIAFreeRec(pScrn);
		return FALSE;
	}

	xf86InitialConfiguration(pScrn, TRUE);

    if (pVia->pVbe) {

        if (!ViaVbeModePreInit(pScrn)) {
            VIAFreeRec(pScrn);
            return FALSE;
        }

    } else {
        /* Add own modes. */
        ViaModesAttach(pScrn, pScrn->monitor);

        /*
         * Set up ClockRanges, which describe what clock ranges are
         * available, and what sort of modes they can be used for.
         */
        clockRanges = xnfalloc(sizeof(ClockRange));
        clockRanges->next = NULL;
        clockRanges->minClock = 20000;
        clockRanges->maxClock = 230000;

        clockRanges->clockIndex = -1;
        clockRanges->interlaceAllowed = TRUE;
        clockRanges->doubleScanAllowed = FALSE;

        /*
         * xf86ValidateModes will check that the mode HTotal and VTotal values
         * don't exceed the chipset's limit if pScrn->maxHValue and
         * pScrn->maxVValue are set.  Since our VIAValidMode() already takes
         * care of this, we don't worry about setting them here.
         *
         * CLE266A:
         *   Max Line Pitch: 4080, (FB corruption when higher, driver problem?)
         *   Max Height: 4096 (and beyond)
         *
         * CLE266A: primary AdjustFrame can use only 24 bits, so we are limited
         * to 12x11 bits; 4080x2048 (~2:1), 3344x2508 (4:3), or 2896x2896 (1:1).
         * TODO Test CLE266Cx, KM400, KM400A, K8M800, CN400 please.
         *
         * We should be able to limit the memory available for a mode to 32 MB,
         * but xf86ValidateModes (or miScanLineWidth) fails to catch this
         * properly (apertureSize).
         */

        /* Select valid modes from those available. */
        i = xf86ValidateModes(pScrn,
                              pScrn->monitor->Modes,     /* List of modes available for the monitor */
                              pScrn->display->modes,     /* List of mode names that the screen is requesting */
                              clockRanges,               /* list of clock ranges */
                              NULL,                      /* list of line pitches */
                              256,                       /* minimum line pitch */
                              max_pitch,                 /* maximum line pitch */
                              16 * 8,                    /* pitch increment (in bits), we just want 16 bytes alignment */
                              128,                       /* min virtual height */
                              max_height,                /* maximum virtual height */
                              pScrn->display->virtualX,  /* virtual width */
                              pScrn->display->virtualY,  /* virtual height */
                              pVia->videoRambytes,       /* apertureSize */
                              LOOKUP_BEST_REFRESH);      /* lookup mode flags */

        if (i == -1) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "xf86ValidateModes failure\n");
            VIAFreeRec(pScrn);
            return FALSE;
        }

        /* This function deletes modes in the modes field of the ScrnInfoRec that have been marked as invalid. */        xf86PruneDriverModes(pScrn);

        if (i == 0 || pScrn->modes == NULL) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }
    return TRUE;
}
