/*
 * Copyright 2007 The Openchrome Project [openchrome.org]
 * Copyright 1998-2007 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2007 S3 Graphics, Inc. All Rights Reserved.
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

/*
 * Integrated LVDS power management functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "via.h"
#include "via_driver.h"
#include "via_vgahw.h"
#include "via_id.h"
#include <unistd.h>

/*
	1. Formula:
		2^13 X 0.0698uSec [1/14.318MHz] = 8192 X 0.0698uSec =572.1uSec
		Timer = Counter x 572 uSec
	2. Note:
		0.0698 uSec is too small to compute for hardware. So we multiply a
		reference value(2^13) to make it big enough to compute for hardware.
	3. Note:
		The meaning of the TD0~TD3 are count of the clock.
		TD(sec) = (sec)/(per clock) x (count of clocks)
*/

#define TD0 200
#define TD1 25
#define TD2 0
#define TD3 25

static void
ViaLVDSSoftwarePowerFirstSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaLVDSSoftwarePowerFirstSequence: %d\n", on));
    if (on) {

        /* Software control power sequence ON*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0x7F);
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x01);
        usleep(TD0);

        /* VDD ON*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x10);
        usleep(TD1);

        /* DATA ON */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x08);
        usleep(TD2);

        /* VEE ON (unused on vt3353)*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x04);
        usleep(TD3);

        /* Back-Light ON */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x02);
    } else {
        /* Back-Light OFF */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xFD);
        usleep(TD3);

        /* VEE OFF (unused on vt3353)*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xFB);
        usleep(TD2);

        /* DATA OFF */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xF7);
        usleep(TD1);

        /* VDD OFF */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xEF);
    }
}

static void
ViaLVDSSoftwarePowerSecondSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaLVDSSoftwarePowerSecondSequence: %d\n", on));
    if (on) {
        /* Secondary power hardware power sequence enable 0:off 1: on */
        hwp->writeCrtc(hwp, 0xD4, hwp->readCrtc(hwp, 0xD4) & 0xFD);

        /* Software control power sequence ON */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x01);
        usleep(TD0);

        /* VDD ON*/
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x10);
        usleep(TD1);

        /* DATA ON */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x08);
        usleep(TD2);

        /* VEE ON (unused on vt3353)*/
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x04);
        usleep(TD3);

        /* Back-Light ON */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x02);
    } else {
        /* Back-Light OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xFD);
        usleep(TD3);

        /* VEE OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xFB);
        /* Delay TD2 msec. */
        usleep(TD2);

        /* DATA OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xF7);
        /* Delay TD1 msec. */
        usleep(TD1);

        /* VDD OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xEF);
    }
}


static void
ViaLVDSHardwarePowerFirstSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    if (on) {
        /* Use hardware control power sequence. */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xFE);
        /* Turn on back light. */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0x3F);
        /* Turn on hardware power sequence. */
        hwp->writeCrtc(hwp, 0x6A, hwp->readCrtc(hwp, 0x6A) | 0x08);
    } else {
        /* Turn off power sequence. */
        hwp->writeCrtc(hwp, 0x6A, hwp->readCrtc(hwp, 0x6A) & 0xF7);
        usleep(1);
        /* Turn off back light. */
        hwp->writeCrtc(hwp, 0x91, 0xC0);
    }
}

static void
ViaLVDSHardwarePowerSecondSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    if (on) {
        /* Use hardware control power sequence. */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xFE);
        /* Turn on back light. */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0x3F);
        /* Turn on hardware power sequence. */
        hwp->writeCrtc(hwp, 0xD4, hwp->readCrtc(hwp, 0xD4) | 0x02);
    } else {
        /* Turn off power sequence. */
        hwp->writeCrtc(hwp, 0xD4, hwp->readCrtc(hwp, 0xD4) & 0xFD);
        usleep(1);
        /* Turn off back light. */
        hwp->writeCrtc(hwp, 0xD3, 0xC0);
    }
}

static void
ViaLVDSDFPPower(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);

    /* Switch DFP High/Low pads on or off for channels active at EnterVT(). */
    ViaSeqMask(hwp, 0x2A, on ? pVia->SavedReg.SR2A : 0, 0x0F);
}

static void
ViaLVDSPowerChannel(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 lvdsMask;

    if (on) {
        /* LVDS0: 0x7F, LVDS1: 0xBF */
        lvdsMask = 0x7F & 0xBF;
        hwp->writeCrtc(hwp, 0xD2, hwp->readCrtc(hwp, 0xD2) & lvdsMask);
    } else {
        /* LVDS0: 0x80, LVDS1: 0x40 */
        lvdsMask = 0x80 | 0x40;
        hwp->writeCrtc(hwp, 0xD2, hwp->readCrtc(hwp, 0xD2) | lvdsMask);
    }
}

void
ViaLVDSPower(ScrnInfoPtr pScrn, Bool on)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaLVDSPower %d\n", on));
    VIAPtr pVia = VIAPTR(pScrn);

    /*
     * VX800, CX700 have HW issue, so we'd better use SW power sequence
     * Fix Ticket #308
     */
    switch (pVia->Chipset) {
        case VIA_VX800:
        case VIA_CX700:
            ViaLVDSSoftwarePowerFirstSequence(pScrn, on);
            ViaLVDSSoftwarePowerSecondSequence(pScrn, on);
            break;
        default:
            ViaLVDSHardwarePowerFirstSequence(pScrn, on);
            ViaLVDSHardwarePowerSecondSequence(pScrn, on);
    }

    ViaLVDSDFPPower(pScrn, on);
    ViaLVDSPowerChannel(pScrn, on);
}

static void
via_lvds_create_resources(xf86OutputPtr output)
{
}

static Bool
via_lvds_set_property(xf86OutputPtr output, Atom property,
						RRPropertyValuePtr value)
{
	return FALSE;
}

static Bool
via_lvds_get_property(xf86OutputPtr output, Atom property)
{
	return FALSE;
}

static void
via_lvds_dpms(xf86OutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	VIAPtr pVia = VIAPTR(pScrn);

	switch (mode) {
	case DPMSModeOn:
		switch (pVia->Chipset) {
		case VIA_P4M900:
		case VIA_CX700:
		case VIA_VX800:
		case VIA_VX855:
		case VIA_VX900:
			ViaLVDSPower(pScrn, TRUE);
			break;
		}
		ViaLCDPower(pScrn, TRUE);
		break;

	case DPMSModeStandby:
	case DPMSModeSuspend:
	case DPMSModeOff:
		switch (pVia->Chipset) {
		case VIA_P4M900:
		case VIA_CX700:
		case VIA_VX800:
		case VIA_VX855:
		case VIA_VX900:
			ViaLVDSPower(pScrn, FALSE);
			break;
		}
		ViaLCDPower(pScrn, FALSE);
		break;
	}
}

static void
via_lvds_save(xf86OutputPtr output)
{
}

static void
via_lvds_restore(xf86OutputPtr output)
{
}

static int
via_lvds_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	return 0;
}

static Bool
via_lvds_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
					DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void
via_lvds_prepare(xf86OutputPtr output)
{
}

static void
via_lvds_commit(xf86OutputPtr output)
{
}

static void
via_lvds_mode_set(xf86OutputPtr  output, DisplayModePtr mode,
					DisplayModePtr adjusted_mode)
{
}

static xf86OutputStatus
via_lvds_detect(xf86OutputPtr output)
{
	xf86OutputStatus status = XF86OutputStatusDisconnected;
	ScrnInfoPtr pScrn = output->scrn;
	VIAPtr pVia = VIAPTR(pScrn);

	if (!pVia->UseLegacyModeSwitch)
		status = ViaPanelPreInit(pScrn);
	return status;
}

static DisplayModePtr
via_lvds_get_modes(xf86OutputPtr output)
{
	return NULL;
}

static void
via_lvds_destroy(xf86OutputPtr output)
{
}

static const xf86OutputFuncsRec via_lvds_funcs = {
	.create_resources	= via_lvds_create_resources,
	.set_property		= via_lvds_set_property,
	.get_property		= via_lvds_get_property,
	.dpms				= via_lvds_dpms,
	.save				= via_lvds_save,
	.restore			= via_lvds_restore,
	.mode_valid			= via_lvds_mode_valid,
	.mode_fixup			= via_lvds_mode_fixup,
	.prepare			= via_lvds_prepare,
	.commit				= via_lvds_commit,
	.mode_set			= via_lvds_mode_set,
	.detect				= via_lvds_detect,
	.get_modes			= via_lvds_get_modes,
	.destroy			= via_lvds_destroy,
};

void
via_lvds_init(ScrnInfoPtr pScrn)
{
	VIAPtr pVia = VIAPTR(pScrn);
	VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
	xf86OutputPtr output = NULL;

	if (pBIOSInfo->ForcePanel) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Enabling panel from config.\n");
		output = xf86OutputCreate(pScrn, &via_lvds_funcs, "LVDS");
	} else if (pVia->Id && (pVia->Id->Outputs & VIA_DEVICE_LCD)) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"Enabling panel from PCI-subsystem ID information.\n");
		output = xf86OutputCreate(pScrn, &via_lvds_funcs, "LVDS");
	}
	pBIOSInfo->lvds = output;
}
