/*
 * Copyright 2005-2007 The Openchrome Project  [openchrome.org]
 * Copyright 2004-2006 Luc Verhaegen.
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

/*************************************************************************
 *
 *  File:       via_driver.c
 *  Content:    XFree86 4.0 for VIA/S3G UniChrome
 *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86RAC.h"
#include "shadowfb.h"

#include "globals.h"
#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "svnversion.h"

#include "via_driver.h"
#include "via_video.h"

#include "via.h"

#ifdef XF86DRI
#include "dri.h"
#endif
#include "via_vgahw.h"
#include "via_id.h"

/*
 * prototypes
 */

static void VIAIdentify(int flags);
static Bool VIAProbe(DriverPtr drv, int flags);
static Bool VIAPreInit(ScrnInfoPtr pScrn, int flags);
static Bool VIAEnterVT(int scrnIndex, int flags);
static void VIALeaveVT(int scrnIndex, int flags);
static void VIASave(ScrnInfoPtr pScrn);
static void VIARestore(ScrnInfoPtr pScrn);
static Bool VIAWriteMode(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool VIACloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool VIASaveScreen(ScreenPtr pScreen, int mode);
static Bool VIAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
                          char **argv);
static int VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static void VIAFreeScreen(int scrnIndex, int flags);
static Bool VIASwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
static void VIAAdjustFrame(int scrnIndex, int y, int x, int flags);
static void VIADPMS(ScrnInfoPtr pScrn, int mode, int flags);
static const OptionInfoRec * VIAAvailableOptions(int chipid, int busid);

static Bool VIAMapMMIO(ScrnInfoPtr pScrn);
static Bool VIAMapFB(ScrnInfoPtr pScrn);
static void VIAUnmapMem(ScrnInfoPtr pScrn);

static void VIALoadRgbLut(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors, VisualPtr pVisual);

DriverRec VIA =
{
    VIA_VERSION,
    DRIVER_NAME,
    VIAIdentify,
    VIAProbe,
    VIAAvailableOptions,
    NULL,
    0
};


/* Supported chipsets */

static SymTabRec VIAChipsets[] = {
    {VIA_CLE266,   "CLE266"},
    {VIA_KM400,    "KM400/KN400"},
    {VIA_K8M800,   "K8M800"},
    {VIA_PM800,    "PM800/PM880/CN400"},
    {VIA_VM800,    "VM800/CN700/P4M800Pro"},
    {VIA_K8M890,   "K8M890"},
    {VIA_P4M900,   "P4M900"},
    {VIA_CX700,    "CX700"},
    {VIA_P4M890,   "P4M890"},
    {-1,            NULL }
};

/* This table maps a PCI device ID to a chipset family identifier. */
static PciChipsets VIAPciChipsets[] = {
    /* {VIA_CLE266,   PCI_CHIP_CLE3022,   RES_SHARED_VGA}, */
    {VIA_CLE266,   PCI_CHIP_CLE3122,   RES_SHARED_VGA},
    {VIA_KM400,    PCI_CHIP_VT3205,    RES_SHARED_VGA},
    {VIA_K8M800,   PCI_CHIP_VT3204,    RES_SHARED_VGA},
    {VIA_PM800,    PCI_CHIP_VT3259,    RES_SHARED_VGA},
    {VIA_VM800,    PCI_CHIP_VT3314,    RES_SHARED_VGA},
    {VIA_K8M890,   PCI_CHIP_VT3336,    RES_SHARED_VGA},
    {VIA_P4M900,   PCI_CHIP_VT3364,    RES_SHARED_VGA},
    {VIA_CX700,    PCI_CHIP_VT3157,    RES_SHARED_VGA},
    {VIA_P4M890,   PCI_CHIP_VT3327,    RES_SHARED_VGA},
    {-1,            -1,                RES_UNDEFINED}
};

int gVIAEntityIndex = -1;

typedef enum {
#ifdef HAVE_DEBUG
    OPTION_PRINTVGAREGS,
    OPTION_PRINTTVREGS,
    OPTION_I2CSCAN,
#endif /* HAVE_DEBUG */
    OPTION_VBEMODES,
    OPTION_NOACCEL,
#ifdef VIA_HAVE_EXA
    OPTION_ACCELMETHOD,
    OPTION_EXA_NOCOMPOSITE,
    OPTION_EXA_SCRATCH_SIZE,
#endif
    OPTION_SWCURSOR,
    OPTION_HWCURSOR,
    OPTION_SHADOW_FB,
    OPTION_ROTATE,
    OPTION_VIDEORAM,
    OPTION_ACTIVEDEVICE,
    OPTION_BUSWIDTH,
    OPTION_CENTER,
    OPTION_PANELSIZE,
    OPTION_FORCEPANEL,
    OPTION_TVDOTCRAWL,
    OPTION_TVPROGRESSIVE,
    OPTION_TVTYPE,
    OPTION_TVOUTPUT,
    OPTION_DISABLEVQ,
    OPTION_DRIXINERAMA,
    OPTION_DISABLEIRQ,
    OPTION_TVDEFLICKER,
    OPTION_AGP_DMA,
    OPTION_2D_DMA,
    OPTION_XV_DMA,
    OPTION_VBE_SAVERESTORE,
    OPTION_MAX_DRIMEM,
    OPTION_AGPMEM,
    OPTION_DISABLE_XV_BW_CHECK
} VIAOpts;


static OptionInfoRec VIAOptions[] =
{
#ifdef HAVE_DEBUG /* Don't document these */
    {OPTION_PRINTVGAREGS, "PrintVGARegs", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_PRINTTVREGS,  "PrintTVRegs",  OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_I2CSCAN, "I2CScan", OPTV_BOOLEAN, {0}, FALSE},
#endif /* HAVE_DEBUG */
    {OPTION_VBEMODES,   "VBEModes", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_NOACCEL,    "NoAccel",      OPTV_BOOLEAN, {0}, FALSE},
#ifdef VIA_HAVE_EXA
    {OPTION_ACCELMETHOD, "AccelMethod", OPTV_STRING,  {0}, FALSE},
    {OPTION_EXA_NOCOMPOSITE, "ExaNoComposite", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_EXA_SCRATCH_SIZE, "ExaScratchSize", OPTV_INTEGER, {0}, FALSE},
#endif /* VIA_HAVE_EXA */
    {OPTION_HWCURSOR,   "HWCursor",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_SWCURSOR,   "SWCursor",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_SHADOW_FB,  "ShadowFB",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_ROTATE,     "Rotate",       OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_VIDEORAM,   "VideoRAM",     OPTV_INTEGER, {0}, FALSE},
    {OPTION_ACTIVEDEVICE,     "ActiveDevice",       OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_BUSWIDTH,	"BusWidth",  	OPTV_ANYSTR, {0}, FALSE},
    {OPTION_CENTER,     "Center",       OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_PANELSIZE,  "PanelSize",    OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_FORCEPANEL, "ForcePanel",   OPTV_BOOLEAN, {0}, FALSE}, /* last resort - don't doc */
    {OPTION_TVDOTCRAWL, "TVDotCrawl",   OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_TVDEFLICKER,"TVDeflicker",  OPTV_INTEGER, {0}, FALSE},
    {OPTION_TVPROGRESSIVE, "TVProgressive", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_TVTYPE,     "TVType",       OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVOUTPUT,   "TVOutput",     OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_DISABLEVQ,  "DisableVQ",    OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_DISABLEIRQ, "DisableIRQ", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_AGP_DMA, "EnableAGPDMA", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_2D_DMA, "NoAGPFor2D", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_XV_DMA, "NoXVDMA", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_VBE_SAVERESTORE, "VbeSaveRestore", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_DISABLE_XV_BW_CHECK, "DisableXvBWCheck", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_MAX_DRIMEM,   "MaxDRIMem",     OPTV_INTEGER, {0}, FALSE},
    {OPTION_AGPMEM,   "AGPMem",     OPTV_INTEGER, {0}, FALSE},
    {-1,                NULL,           OPTV_NONE,    {0}, FALSE}
};


static const char *vgaHWSymbols[] = {
    "vgaHWGetHWRec",
    "vgaHWSetMmioFuncs",
    "vgaHWSetStdFuncs",
    "vgaHWGetIOBase",
    "vgaHWSave",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWMapMem",
    "vgaHWUnmapMem",
    "vgaHWInit",
    "vgaHWSaveScreen",
    "vgaHWLock",
    "vgaHWUnlock",
    "vgaHWFreeHWRec",
    "vgaHWGetIndex",  /* Through VGAHWPTR() */
    NULL
};


static const char *ramdacSymbols[] = {
    "xf86InitCursor",
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    NULL
};

static const char *vbeSymbols[] = {
    "vbeDoEDID",
    "VBEDPMSSet",
    "VBEExtendedInit",
    "vbeFree",
    "VBEGetVBEInfo",
    "VBEGetVBEMode",
    "VBEGetModePool",
    "VBEInit",
    "VBEPrintModes",
    "VBESaveRestore",
    "VBESetDisplayStart",
    "VBESetGetLogicalScanlineLength",
    "VBESetLogicalScanline",
    "VBESetModeNames",
    "VBESetModeParameters",
    "VBESetVBEMode",
    "VBEValidateModes",
    "xf86ExecX86int10",
    "xf86Int10AllocPages",
    "xf86Int10FreePages",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86DoEDID_DDC2",
    "xf86SetDDCproperties",
    NULL
};


static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    "xf86CreateI2CDevRec",
    "xf86I2CDevInit",
    "xf86I2CWriteRead",
    "xf86I2CProbeAddress",
    "xf86DestroyI2CDevRec",
    "xf86I2CReadByte",
    "xf86I2CWriteByte",
    NULL
};

static const char *xaaSymbols[] = {
#ifdef X_HAVE_XAAGETROP
    "XAAGetCopyROP",
    "XAAGetCopyROP_PM",
    "XAAGetPatternROP",
#else
    "XAACopyROP",
    "XAACopyROP_PM",
    "XAAPatternROP",
#endif
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    "XAAFillSolidRects",
    NULL
};

#ifdef VIA_HAVE_EXA
static const char *exaSymbols[] = {
  "exaGetVersion",
  "exaDriverInit",
  "exaDriverFini",
  "exaOffscreenAlloc",
  "exaOffscreenFree",
  "exaGetPixmapPitch",
  "exaGetPixmapOffset",
  "exaWaitSync",
#if (EXA_VERSION_MAJOR >= 2)
  "exaDriverAlloc",
#else
  "exaGetVersion",
#endif
  NULL
};
#endif

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

#ifdef USE_FB
static const char *fbSymbols[] = {
    "fbScreenInit",
    "fbPictureInit",
    NULL
};
#else
static const char *cfbSymbols[] = {
    "cfbScreenInit",
    "cfb16ScreenInit",
    "cfb24ScreenInit",
    "cfb24_32ScreenInit",
    "cfb32ScreenInit",
    "cfb16BresS",
    "cfb24BresS",
    NULL
};
#endif

#ifdef XFree86LOADER
#ifdef XF86DRI
static const char *drmSymbols[] = {
    "drmAddBufs",
    "drmAddMap",
    "drmAgpAcquire",
    "drmAgpAlloc",
    "drmAgpBase",
    "drmAgpBind",
    "drmAgpDeviceId",
    "drmAgpEnable",
    "drmAgpFree",
    "drmAgpGetMode",
    "drmAgpRelease",
    "drmAgpVendorId",
    "drmCtlInstHandler",
    "drmCtlUninstHandler",
    "drmCommandNone",
    "drmCommandWrite"
    "drmCommandWriteRead",
    "drmFreeVersion",
    "drmGetInterruptFromBusID",
    "drmGetLibVersion",
    "drmGetVersion",
    "drmMap",
    "drmMapBufs",
    "drmUnmap",
    "drmUnmapBufs",
    "drmAgpUnbind",
    "drmRmMap",
    "drmCreateContext",
    "drmAuthMagic",
    "drmDestroyContext",
    "drmSetContextFlags",
    NULL
};

static const char *driSymbols[] = {
    "DRICloseScreen",
    "DRICreateInfoRec",
    "DRIDestroyInfoRec",
    "DRIFinishScreenInit",
    "DRIGetSAREAPrivate",
    "DRILock",
    "DRIQueryVersion",
    "DRIScreenInit",
    "DRIUnlock",
    "DRIOpenConnection",
    "DRICloseConnection",
    "GlxSetVisualConfigs",
    NULL
};
#endif

static MODULESETUPPROTO(VIASetup);

static XF86ModuleVersionInfo VIAVersRec = {
    "via",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
#ifdef XORG_VERSION_CURRENT
    XORG_VERSION_CURRENT,
#else
    XF86_VERSION_CURRENT,
#endif
    VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

XF86ModuleData viaModuleData = {&VIAVersRec, VIASetup, NULL};

static pointer VIASetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
        setupDone = TRUE;
        xf86AddDriver(&VIA, module, 0);
        LoaderRefSymLists(vgaHWSymbols,
#ifdef USE_FB
                          fbSymbols,
#else
                          cfbSymbols,
#endif
                          ramdacSymbols,
                          xaaSymbols,
#ifdef VIA_HAVE_EXA
			  exaSymbols,
#endif
                          shadowSymbols,
                          vbeSymbols,
                          i2cSymbols,
                          ddcSymbols,
#ifdef XF86DRI
			  drmSymbols,
			  driSymbols,
#endif
                          NULL);

        return (pointer) 1;
    }
    else {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;

        return NULL;
    }
} /* VIASetup */

#endif /* XFree86LOADER */

static Bool VIAGetRec(ScrnInfoPtr pScrn)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAGetRec\n"));
    if (pScrn->driverPrivate)
        return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(VIARec), 1);
    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo =
        xnfcalloc(sizeof(VIABIOSInfoRec), 1);
    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->scrnIndex =
	pScrn->scrnIndex;
    ((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->TVI2CDev = NULL;

    ((VIARec *)(pScrn->driverPrivate))->CursorImage = NULL;

    return TRUE;

} /* VIAGetRec */


static void VIAFreeRec(ScrnInfoPtr pScrn)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAFreeRec\n"));
    if (!pScrn->driverPrivate)
        return;

    if (VIAPTR(pScrn)->pVbe)
	vbeFree(VIAPTR(pScrn)->pVbe);

    if (((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->TVI2CDev)
	xf86DestroyI2CDevRec((((VIARec *)(pScrn->driverPrivate))->pBIOSInfo->TVI2CDev), TRUE);
    xfree(((VIARec *)(pScrn->driverPrivate))->pBIOSInfo);

    VIAUnmapMem(pScrn);

    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
} /* VIAFreeRec */



static const OptionInfoRec * VIAAvailableOptions(int chipid, int busid)
{

    return VIAOptions;

} /* VIAAvailableOptions */


static void VIAIdentify(int flags)
{
    xf86PrintChipsets("VIA", "driver for VIA chipsets", VIAChipsets);
} /* VIAIdentify */

static Bool VIAProbe(DriverPtr drv, int flags)
{
    GDevPtr *devSections;
    int     *usedChips;
    int     numDevSections;
    int     numUsed;
    Bool    foundScreen = FALSE;
    int     i;

    /* sanity checks */
    if ((numDevSections = xf86MatchDevice(DRIVER_NAME, &devSections)) <= 0)
        return FALSE;

    if (xf86GetPciVideoInfo() == NULL)
        return FALSE;

    numUsed = xf86MatchPciInstances(DRIVER_NAME,
                                    PCI_VIA_VENDOR_ID,
                                    VIAChipsets,
                                    VIAPciChipsets,
                                    devSections,
                                    numDevSections,
                                    drv,
                                    &usedChips);
    xfree(devSections);

    if (numUsed <= 0)
        return FALSE;

    xf86Msg(X_NOTICE, "VIA Technologies does not support or endorse this driver in any way.\n");
    xf86Msg(X_NOTICE, "For support, please refer to http://www.openchrome.org/ or\n");
    xf86Msg(X_NOTICE, "your X vendor.\n");

#ifdef BUILDCOMMENT
    xf86Msg(X_NOTICE, BUILDCOMMENT);
#endif

    if (flags & PROBE_DETECT) {
        foundScreen = TRUE;
    }
    else {
        for (i = 0; i < numUsed; i++) {
            ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);
            EntityInfoPtr pEnt;
            if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
                 VIAPciChipsets, 0, 0, 0, 0, 0)))
            {
                pScrn->driverVersion = VIA_VERSION;
                pScrn->driverName = DRIVER_NAME;
                pScrn->name = "VIA";
                pScrn->Probe = VIAProbe;
                pScrn->PreInit = VIAPreInit;
                pScrn->ScreenInit = VIAScreenInit;
                pScrn->SwitchMode = VIASwitchMode;
                pScrn->AdjustFrame = VIAAdjustFrame;
                pScrn->EnterVT = VIAEnterVT;
                pScrn->LeaveVT = VIALeaveVT;
                pScrn->FreeScreen = VIAFreeScreen;
                pScrn->ValidMode = ViaValidMode;
                foundScreen = TRUE;
            }
            /*
            xf86ConfigActivePciEntity(pScrn,
                                      usedChips[i],
                                      VIAPciChipsets,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
	    */
            pEnt = xf86GetEntityInfo(usedChips[i]);

            /* CLE266 card support Dual-Head, mark the entity as sharable*/
            if(pEnt->chipset == VIA_CLE266 || pEnt->chipset == VIA_KM400)
            {
                static int instance = 0;
                DevUnion* pPriv;

                xf86SetEntitySharable(usedChips[i]);
                xf86SetEntityInstanceForScreen(pScrn,
                    pScrn->entityList[0], instance);

                if(gVIAEntityIndex < 0)
                {
                    gVIAEntityIndex = xf86AllocateEntityPrivateIndex();
                    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                            gVIAEntityIndex);

                    if (!pPriv->ptr)
                    {
                        VIAEntPtr pVIAEnt;
                        pPriv->ptr = xnfcalloc(sizeof(VIAEntRec), 1);
                        pVIAEnt = pPriv->ptr;
                        pVIAEnt->IsDRIEnabled = FALSE;
                        pVIAEnt->BypassSecondary = FALSE;
                        pVIAEnt->HasSecondary = FALSE;
                        pVIAEnt->IsSecondaryRestored = FALSE;
                    }
                }
                instance++;
            }
            xfree(pEnt);
        }
    }

    xfree(usedChips);

    return foundScreen;

} /* VIAProbe */

#ifdef XF86DRI
static void kickVblank(ScrnInfoPtr pScrn)
{
    /*
     * Switching mode will clear registers that make vblank
     * interrupts happen. If the driver thinks interrupts
     * are enabled, make sure vblank interrupts go through.
     * registers are not documented in VIA docs.
     */

    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIADRIPtr pVIADRI = pVia->pDRIInfo->devPrivate;

    if (pVIADRI->irqEnabled) {
	hwp->writeCrtc(hwp, 0x11, hwp->readCrtc(hwp, 0x11) | 0x30);
    }
}
#endif

static int LookupChipSet(PciChipsets *pset, int chipSet)
{
  while (pset->numChipset >= 0) {
    if (pset->numChipset == chipSet) return pset->PCIid;
    pset++;
  }
  return -1;
}
      

static int LookupChipID(PciChipsets* pset, int ChipID)
{
    /* Is there a function to do this for me? */
    while (pset->numChipset >= 0)
    {
        if (pset->PCIid == ChipID)
            return pset->numChipset;

        pset++;
    }

    return -1;

} /* LookupChipID */

static void
VIAProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
	xf86LoaderReqSymLists(vbeSymbols, NULL);
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}

static Bool VIAPreInit(ScrnInfoPtr pScrn, int flags)
{
    EntityInfoPtr   pEnt;
    VIAPtr          pVia;
    VIABIOSInfoPtr  pBIOSInfo;
    MessageType     from = X_DEFAULT;
    ClockRangePtr   clockRanges;
    char            *s = NULL;
#ifndef USE_FB
    char            *mod = NULL;
    const char      *reqSym = NULL;
#endif
    vgaHWPtr        hwp;
    int             i, bMemSize = 0;
    Bool            UseVBEModes = FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAPreInit\n"));

    if (pScrn->numEntities > 1)
        return FALSE;

    if (flags & PROBE_DETECT)
        return FALSE;

    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;

    xf86LoaderReqSymLists(vgaHWSymbols, NULL);
    if (!vgaHWGetHWRec(pScrn))
        return FALSE;

#if 0
    /* Here we can alter the number of registers saved and restored by the
     * standard vgaHWSave and Restore routines.
     */
    vgaHWSetRegCounts(pScrn, VGA_NUM_CRTC, VGA_NUM_SEQ, VGA_NUM_GFX, VGA_NUM_ATTR);
#endif

    if (!VIAGetRec(pScrn)) {
        return FALSE;
    }

    pVia = VIAPTR(pScrn);
    pBIOSInfo = pVia->pBIOSInfo;

    pVia->IsSecondary = FALSE;
    pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pEnt->resources) {
        xfree(pEnt);
        VIAFreeRec(pScrn);
        return FALSE;
    }

    pVia->EntityIndex = pEnt->index;

    if(xf86IsEntityShared(pScrn->entityList[0]))
    {
        if(xf86IsPrimInitDone(pScrn->entityList[0]))
        {
            DevUnion* pPriv;
            VIAEntPtr pVIAEnt;
            VIAPtr pVia1;

            pVia->IsSecondary = TRUE;
            pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                    gVIAEntityIndex);
            pVIAEnt = pPriv->ptr;
            if (pVIAEnt->BypassSecondary) {
		xfree(pEnt);
		VIAFreeRec(pScrn);
		return FALSE;
	    }
            pVIAEnt->pSecondaryScrn = pScrn;
            pVIAEnt->HasSecondary = TRUE;
            pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
            pVia1->HasSecondary = TRUE;
	    pVia->sharedData = pVia1->sharedData;
        }
        else
        {
            DevUnion* pPriv;
            VIAEntPtr pVIAEnt;

            xf86SetPrimInitDone(pScrn->entityList[0]);
            pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                    gVIAEntityIndex);
	    pVia->sharedData = xnfcalloc(sizeof(ViaSharedRec),1);
            pVIAEnt = pPriv->ptr;
            pVIAEnt->pPrimaryScrn = pScrn;
            pVIAEnt->IsDRIEnabled = FALSE;
            pVIAEnt->BypassSecondary = FALSE;
            pVIAEnt->HasSecondary = FALSE;
            pVIAEnt->RestorePrimary = FALSE;
            pVIAEnt->IsSecondaryRestored = FALSE;
        }
    } else {
	pVia->sharedData = xnfcalloc(sizeof(ViaSharedRec),1);
    }

    if (flags & PROBE_DETECT) {
        VIAProbeDDC(pScrn, pVia->EntityIndex);
        return TRUE;
    }

    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * We support depths of 8, 16 and 24.
     * We support bpp of 8, 16, and 32.
     */

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
        xfree(pEnt);
        VIAFreeRec(pScrn);
        return FALSE;
    }
    else {
        switch (pScrn->depth) {
        case 8:
        case 16:
        case 24:
        case 32:
            /* OK */
            break;
        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Given depth (%d) is not supported by this driver\n",
                       pScrn->depth);
            xfree(pEnt);
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    xf86PrintDepthBpp(pScrn);

    if (pScrn->depth == 32) {
        pScrn->depth = 24;
    }

    if (pScrn->depth > 8) {
        rgb zeros = {0, 0, 0};

        if (!xf86SetWeight(pScrn, zeros, zeros)) {
	    xfree(pEnt);
	    VIAFreeRec(pScrn);
            return FALSE;
	} else {
            /* TODO check weight returned is supported */
            ;
        }
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
        return FALSE;
    }
    else {
        /* We don't currently support DirectColor at > 8bpp */
        if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
                       " (%s) is not supported at depth %d.\n",
                       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
            xfree(pEnt);
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    xf86CollectOptions(pScrn, NULL);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
        pScrn->rgbBits = 6;

    pVia->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
    xf86RegisterResources(pEnt->index, NULL, ResNone);

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, VIAOptions);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
	       "Starting to parse config file options...\n");
    pVia->shadowFB = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_SHADOW_FB, &pVia->shadowFB) ? 
	X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "ShadowFB is %s.\n",
	       pVia->shadowFB ? "enabled" : "disabled");

    if ((s = xf86GetOptValString(VIAOptions, OPTION_ROTATE))) {
        if (!xf86NameCmp(s, "CW")) {
            /* accel is disabled below for shadowFB */
            pVia->shadowFB = TRUE;
            pVia->rotate = 1;
            pVia->hwcursor = FALSE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Rotating screen clockwise - acceleration disabled.\n");
        }
        else if(!xf86NameCmp(s, "CCW")) {
            pVia->shadowFB = TRUE;
            pVia->rotate = -1;
            pVia->hwcursor = FALSE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
                       "counter clockwise - acceleration disabled.\n");
        }
        else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
                       "value for Option \"Rotate\".\n", s);
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "Valid options are \"CW\" or \"CCW\".\n");
        }
    }

    pVia->NoAccel = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_NOACCEL, &pVia->NoAccel) ? 
	X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Acceleration is %s.\n",
	       !pVia->NoAccel ? "enabled" : "disabled");

    if (pVia->shadowFB && !pVia->NoAccel) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "HW acceleration not supported with \"shadowFB\".\n");
        pVia->NoAccel = TRUE;
    }

#ifdef VIA_HAVE_EXA 
    if(!pVia->NoAccel) {
        from = X_DEFAULT;
	if((s = (char *)xf86GetOptValString(VIAOptions, OPTION_ACCELMETHOD))) {
	    if(!xf86NameCmp(s,"XAA")) {
		from = X_CONFIG;
		pVia->useEXA = FALSE;
	    }
	    else if(!xf86NameCmp(s,"EXA")) {
		from = X_CONFIG;
		pVia->useEXA = TRUE;
	    }
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "Using %s acceleration architecture.\n",
		   pVia->useEXA ? "EXA" : "XAA");

	pVia->noComposite = FALSE;
	if (pVia->useEXA) {
	    from = xf86GetOptValBool(VIAOptions, OPTION_EXA_NOCOMPOSITE, 
				     &pVia->noComposite) ? 
		X_CONFIG : X_DEFAULT;
	    xf86DrvMsg(pScrn->scrnIndex, from, 
		       "EXA composite acceleration %s.\n",
		       !pVia->noComposite ? "enabled" : "disabled");
	    
	    pVia->exaScratchSize = VIA_SCRATCH_SIZE / 1024;
	    from = xf86GetOptValInteger(VIAOptions, OPTION_EXA_SCRATCH_SIZE, 
					&pVia->exaScratchSize) ? 
		X_CONFIG : X_DEFAULT;
	    xf86DrvMsg( pScrn->scrnIndex, from,
			"EXA scratch area size is %dkB.\n", 
			pVia->exaScratchSize );
	}
    }
#endif /* VIA_HAVE_EXA */

    /*
     * The SWCursor setting takes priority over HWCursor.  The default
     * if neither is specified is HW.
     */

    from = X_DEFAULT;
    pVia->hwcursor = pVia->shadowFB ? FALSE : TRUE;
    if (xf86GetOptValBool(VIAOptions, OPTION_HWCURSOR, &pVia->hwcursor))
        from = X_CONFIG;

    if (xf86GetOptValBool(VIAOptions, OPTION_SWCURSOR, &pVia->hwcursor)) {
        pVia->hwcursor = !pVia->hwcursor;
        from = X_CONFIG;
    }

    if (pVia->IsSecondary) 
	pVia->hwcursor = FALSE;

    if (pVia->hwcursor) {
	xf86DrvMsg(pScrn->scrnIndex, from, 
		   "Hardware two-color cursors.\n"
		   "\tSoftware full color cursors.\n");
    } else {
	xf86DrvMsg(pScrn->scrnIndex, from, "Using software cursors.\n");
    }

    from = X_DEFAULT;

    pScrn->videoRam = pEnt->device->videoRam;
    from = xf86GetOptValInteger(VIAOptions, OPTION_VIDEORAM, 
				&pScrn->videoRam) ? 
	X_CONFIG : X_PROBED;
    xf86DrvMsg( pScrn->scrnIndex, X_CONFIG,
		"VideoRAM %dkB\n", pScrn->videoRam );

    pVia->VQEnable = TRUE;
    from = xf86GetOptValBool(VIAOptions, OPTION_DISABLEVQ, &pVia->VQEnable)
	? X_CONFIG : X_DEFAULT;
    if (from == X_CONFIG)
         pVia->VQEnable = !pVia->VQEnable;
    xf86DrvMsg(pScrn->scrnIndex, from, "GPU virtual command queue will be %s.\n",
	       (pVia->VQEnable) ? "enabled" : "disabled");

    pVia->DRIIrqEnable = TRUE;
    from = xf86GetOptValBool(VIAOptions, OPTION_DISABLEIRQ, 
			     &pVia->DRIIrqEnable)
	? X_CONFIG : X_DEFAULT;
    if (from == X_CONFIG)
         pVia->DRIIrqEnable = !pVia->DRIIrqEnable;
    xf86DrvMsg(pScrn->scrnIndex, from, "DRI IRQ will be %s if DRI is enabled.\n",
	       (pVia->DRIIrqEnable) ? "enabled" : "disabled");

    pVia->agpEnable = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_AGP_DMA, &pVia->agpEnable)
	? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "AGP DMA will be %s if DRI is enabled.\n",
	       (pVia->agpEnable) ? "enabled" : "disabled");

    pVia->dma2d = TRUE;
    if (pVia->agpEnable) {
	from = xf86GetOptValBool(VIAOptions, OPTION_2D_DMA, &pVia->dma2d)
	    ? X_CONFIG : X_DEFAULT;
	if (from == X_CONFIG)
	    pVia->dma2d = !pVia->dma2d;
	xf86DrvMsg(pScrn->scrnIndex, from, "AGP DMA will %sbe used for 2D "
		   "acceleration.\n",
		   (pVia->dma2d) ? "" : "not ");
    }

    pVia->dmaXV = TRUE;
    from = xf86GetOptValBool(VIAOptions, OPTION_XV_DMA, &pVia->dmaXV)
	? X_CONFIG : X_DEFAULT;	
    if (from == X_CONFIG)
         pVia->dmaXV = !pVia->dmaXV;
    xf86DrvMsg(pScrn->scrnIndex, from, "PCI DMA will %sbe used for XV "
	       "image transfer if DRI is enabled.\n",
	       (pVia->dmaXV) ? "" : "not ");

    UseVBEModes = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_VBEMODES, &UseVBEModes)
      ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Will %senable VBE modes.\n",
	       UseVBEModes ? "" : "not ");
    

    pVia->vbeSR = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_VBE_SAVERESTORE, &pVia->vbeSR)
	? X_CONFIG : X_DEFAULT;	
    xf86DrvMsg(pScrn->scrnIndex, from, "VBE VGA register save & restore "
	       "will %sbe used\n\tif VBE modes are enabled.\n",
	       (pVia->vbeSR) ? "" : "not ");

#ifdef HAVE_DEBUG
    pVia->disableXvBWCheck = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_DISABLE_XV_BW_CHECK, 
			     &pVia->disableXvBWCheck)
	? X_CONFIG : X_DEFAULT;	
    xf86DrvMsg(pScrn->scrnIndex, from, "Xv Bandwidth check is %s.\n",
	       pVia->disableXvBWCheck ? "disabled" : "enabled");
    if (pVia->disableXvBWCheck) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
		   "You may get a \"snowy\" screen"
		   " when using the Xv overlay.\n");
    }
#endif

    pVia->maxDriSize = 0;
    from = xf86GetOptValInteger(VIAOptions, OPTION_MAX_DRIMEM, 
				&pVia->maxDriSize)
	? X_CONFIG : X_DEFAULT;	
    if (pVia->maxDriSize > 0)
	xf86DrvMsg( pScrn->scrnIndex, from,
		    "Will impose a %dkB limit on video-ram set aside for DRI.\n", 
		    pVia->maxDriSize );
    else
	xf86DrvMsg( pScrn->scrnIndex, from, 
		    "Will not impose a limit on video-ram set aside for DRI.\n");

    pVia->agpMem = AGP_SIZE / 1024;
    from = xf86GetOptValInteger(VIAOptions, OPTION_AGPMEM, 
				&pVia->agpMem)
	? X_CONFIG : X_DEFAULT;	
    xf86DrvMsg(pScrn->scrnIndex, from, 
	       "Will try to allocate %dkB of AGP memory.\n", 
	       pVia->agpMem );

    /* ActiveDevice Option for device selection */
    pVia->ActiveDevice = 0x00;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_ACTIVEDEVICE))) {
	if (xf86strstr(s, "CRT"))
	    pVia->ActiveDevice |= VIA_DEVICE_CRT;
	if (xf86strstr(s, "LCD"))
	    pVia->ActiveDevice |= VIA_DEVICE_LCD;
	if (xf86strstr(s, "DFP")) /* just treat this the same as LCD */
	    pVia->ActiveDevice |= VIA_DEVICE_LCD;
	if (xf86strstr(s, "TV"))
	    pVia->ActiveDevice |= VIA_DEVICE_TV;
    }

    /* Digital Output Bus Width Option */
    pBIOSInfo->BusWidth = VIA_DI_12BIT;
    from = X_DEFAULT;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_BUSWIDTH))) {
	from = X_CONFIG;
        if (!xf86NameCmp(s, "12BIT")) {
            pBIOSInfo->BusWidth = VIA_DI_12BIT;
        } else if (!xf86NameCmp(s, "24BIT")) {
            pBIOSInfo->BusWidth = VIA_DI_24BIT;
        }
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Digital output bus width is %d bits.\n",
	       (pBIOSInfo->BusWidth == VIA_DI_12BIT) ? 12:24);


    /* LCD Center/Expend Option */
    pBIOSInfo->Center = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_CENTER, &pBIOSInfo->Center)
	? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "DVI Center is %s.\n",
	       pBIOSInfo->Center ? "enabled" : "disabled");


    /* Panel Size Option */
    pBIOSInfo->PanelSize = VIA_PANEL_INVALID;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_PANELSIZE))) {
        if (!xf86NameCmp(s, "640x480")) {
            pBIOSInfo->PanelSize = VIA_PANEL6X4;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 640x480\n");
        }
        else if (!xf86NameCmp(s, "800x600")) {
            pBIOSInfo->PanelSize = VIA_PANEL8X6;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 800x600\n");
        }
        else if(!xf86NameCmp(s, "1024x768")) {
            pBIOSInfo->PanelSize = VIA_PANEL10X7;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1024x768\n");
        }
        else if (!xf86NameCmp(s, "1280x768")) {
            pBIOSInfo->PanelSize = VIA_PANEL12X7;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1280x768\n");
        }
        else if (!xf86NameCmp(s, "1280x800")) {
            pBIOSInfo->PanelSize = VIA_PANEL12X8;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1280x800\n");
        }
        else if (!xf86NameCmp(s, "1280x1024")) {
            pBIOSInfo->PanelSize = VIA_PANEL12X10;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1280x1024\n");
        }
        else if (!xf86NameCmp(s, "1400x1050")) {
            pBIOSInfo->PanelSize = VIA_PANEL14X10;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is 1400x1050\n");
        }
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT,
		   "Panel size is not selected from config file.\n");
    }
	

    /* Force the use of the Panel? */
    pBIOSInfo->ForcePanel = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_FORCEPANEL, 
			     &pBIOSInfo->ForcePanel)
	? X_CONFIG:X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Panel will %sbe forced.\n",
	       pBIOSInfo->ForcePanel ? "" : "not ");
    
    pBIOSInfo->TVDotCrawl = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_TVDOTCRAWL, 
			     &pBIOSInfo->TVDotCrawl)
	? X_CONFIG:X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "TV dotCrawl is %s.\n",
	       pBIOSInfo->TVDotCrawl ? "enabled" : "disabled");

    /* TV Deflicker */
    pBIOSInfo->TVDeflicker = 0;
    from = xf86GetOptValInteger(VIAOptions, OPTION_TVDEFLICKER, 
				&pBIOSInfo->TVDeflicker)
	? X_CONFIG:X_DEFAULT;
    xf86DrvMsg( pScrn->scrnIndex, from, "TV deflicker is set to %d.\n", 
		pBIOSInfo->TVDeflicker );
    
    pBIOSInfo->TVType = TVTYPE_NONE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVTYPE))) {
        if (!xf86NameCmp(s, "NTSC")) {
            pBIOSInfo->TVType = TVTYPE_NTSC;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is NTSC.\n");
        }
        else if(!xf86NameCmp(s, "PAL")) {
            pBIOSInfo->TVType = TVTYPE_PAL;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is PAL.\n");
        }
        else if(!xf86NameCmp(s, "480P")) {
            pBIOSInfo->TVType = TVTYPE_480P;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is SDTV 480P.\n");
        }
        else if(!xf86NameCmp(s, "576P")) {
            pBIOSInfo->TVType = TVTYPE_576P;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is SDTV 576P.\n");
        }
        else if(!xf86NameCmp(s, "720P")) {
            pBIOSInfo->TVType = TVTYPE_720P;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is HDTV 720P.\n");
        }
        else if(!xf86NameCmp(s, "1080I")) {
            pBIOSInfo->TVType = TVTYPE_1080I;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is HDTV 1080i.\n");
        }
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, 
		   "No default TV type is set.\n");
    }
	
    /* TV out put signal Option */
    pBIOSInfo->TVOutput = TVOUTPUT_NONE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVOUTPUT))) {
        if (!xf86NameCmp(s, "S-Video")) {
            pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is S-Video.\n");
        }
        else if(!xf86NameCmp(s, "Composite")) {
            pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is Composite.\n");
        }
        else if(!xf86NameCmp(s, "SC")) {
            pBIOSInfo->TVOutput = TVOUTPUT_SC;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is SC.\n");
        }
        else if(!xf86NameCmp(s, "RGB")) {
            pBIOSInfo->TVOutput = TVOUTPUT_RGB;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is RGB.\n");
        }
        else if(!xf86NameCmp(s, "YCbCr")) {
            pBIOSInfo->TVOutput = TVOUTPUT_YCBCR;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is YCbCr.\n");
        }
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, 
		   "No default TV output signal type is set.\n");
    }

    /*
    xf86SetOperatingState(RES_SHARED_VGA, pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMemShared, pEnt->index, ResDisableOpr);
    */

    if (pEnt->device->chipset && *pEnt->device->chipset) {
        pScrn->chipset = pEnt->device->chipset;
        pVia->Chipset = xf86StringToToken(VIAChipsets, pScrn->chipset);
        pVia->ChipId = pEnt->device->chipID = LookupChipSet(VIAPciChipsets, pVia->Chipset);
        from = X_CONFIG;
    } else if (pEnt->device->chipID >= 0) {
        pVia->ChipId = pEnt->device->chipID;
        pVia->Chipset = LookupChipID(VIAPciChipsets, pVia->ChipId);
        pScrn->chipset = (char *)xf86TokenToString(VIAChipsets,
                                                   pVia->Chipset);
        from = X_CONFIG;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
                   pEnt->device->chipID);
    } else {
        from = X_PROBED;
        pVia->ChipId = pVia->PciInfo->chipType;
        pVia->Chipset = LookupChipID(VIAPciChipsets, pVia->ChipId);
        pScrn->chipset = (char *)xf86TokenToString(VIAChipsets,
                                                   pVia->Chipset);
    }

    if (pEnt->device->chipRev >= 0) {
        pVia->ChipRev = pEnt->device->chipRev;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
                   pVia->ChipRev);
    }
    else {
        /*pVia->ChipRev = pVia->PciInfo->chipRev;*/
        /* Read PCI bus 0, dev 0, function 0, index 0xF6 to get chip rev. */
        pVia->ChipRev = pciReadByte(pciTag(0, 0, 0), 0xF6);
    }

    if (pEnt->device->videoRam != 0) {
        if (!pScrn->videoRam)
    	    pScrn->videoRam = pEnt->device->videoRam;
    	else {
        	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
        	"Video Memory Size in Option is %d KB, Detect is %d KB!",
            pScrn->videoRam, pEnt->device->videoRam);
    	}
    }

    xfree(pEnt);

    VIAVidHWDiffInit(pScrn);

    /* maybe throw in some more sanity checks here */

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);

    pVia->PciTag = pciTag(pVia->PciInfo->bus, pVia->PciInfo->device,
                          pVia->PciInfo->func);

    if (!VIAMapMMIO(pScrn)) {
	VIAFreeRec(pScrn);
        return FALSE;
    }
    hwp = VGAHWPTR(pScrn);


#ifdef HAVE_DEBUG
    pVia->PrintVGARegs = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_PRINTVGAREGS, 
			     &pVia->PrintVGARegs)
	? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Will %sprint VGA Registers.\n",
	       pVia->PrintVGARegs ? "" : "not ");
    if (pVia->PrintVGARegs)
	ViaVgahwPrint(VGAHWPTR(pScrn)); /* Do this as early as possible */

    pVia->I2CScan = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_I2CSCAN, &pVia->I2CScan)
	? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Will %sscan I2C buses.\n",
	       pVia->I2CScan ? "" : "not ");
#endif /* HAVE_DEBUG */

    if (pVia->Chipset == VIA_CLE266)
	ViaDoubleCheckCLE266Revision(pScrn);

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset Rev.: %d\n", pVia->ChipRev);

    ViaCheckCardId(pScrn);   
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
	       "...Finished parsing config file options.\n");


    /* read memory bandwidth from registers */
    pVia->MemClk = hwp->readCrtc(hwp, 0x3D) >> 4;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Detected MemClk %d\n", pVia->MemClk));
    if (pVia->MemClk >= VIA_MEM_END) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unknown Memory clock: %d\n", pVia->MemClk);
	pVia->MemClk = VIA_MEM_END - 1;
    }
    pBIOSInfo->Bandwidth = ViaGetMemoryBandwidth(pScrn);

    if (pBIOSInfo->TVType == TVTYPE_NONE) {
        /* use jumper to determine TV Type */

        if (hwp->readCrtc(hwp, 0x3B) & 0x02) {
            pBIOSInfo->TVType = TVTYPE_PAL;
	    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Detected TV Standard: PAL.\n"));
        }
        else {
            pBIOSInfo->TVType = TVTYPE_NTSC;
	    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Detected TV Standard: NTSC.\n"));
        }
    }

    {
        Gamma zeros = {0.0, 0.0, 0.0};

        if (!xf86SetGamma(pScrn, zeros)) {
	    VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    /* Detect amount of installed RAM */
    if (pScrn->videoRam < 16384 || pScrn->videoRam > 65536) {
	if(pVia->Chipset == VIA_CLE266) {
	    bMemSize = hwp->readSeq(hwp, 0x34);
	    if (!bMemSize) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "CR34 says nothing; trying CR39.\n");
		bMemSize = hwp->readSeq(hwp, 0x39);
	    }
	} else
	    bMemSize = hwp->readSeq(hwp, 0x39);

	if (bMemSize > 16 && bMemSize <= 128)
	    pScrn->videoRam = (bMemSize + 1) << 9;
	else if (bMemSize > 0 && bMemSize < 31)
	    pScrn->videoRam = bMemSize << 12;
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Memory size detection failed: using 16MB.\n");
	    pScrn->videoRam = 16 << 10;	/* Assume the basic 16MB */
	}
    }

    /* Split FB for SAMM */
    /* FIXME: For now, split FB into two equal sections. This should
     *        be able to be adjusted by user with a config option. */
    if (pVia->IsSecondary) {
        DevUnion* pPriv;
        VIAEntPtr pVIAEnt;
        VIAPtr    pVia1;

        pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
              gVIAEntityIndex);
        pVIAEnt = pPriv->ptr;
        pScrn->videoRam = pScrn->videoRam >> 1;
        pVIAEnt->pPrimaryScrn->videoRam = pScrn->videoRam;
        pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
        pVia1->videoRambytes = pScrn->videoRam << 10;
        pVia->FrameBufferBase += (pScrn->videoRam << 10);
    }

    pVia->videoRambytes = pScrn->videoRam << 10;
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,"videoram =  %dk\n",
               pScrn->videoRam);

    if (!xf86LoadSubModule(pScrn, "i2c")) {
        VIAFreeRec(pScrn);
        return FALSE;
    }
    else {
        xf86LoaderReqSymLists(i2cSymbols,NULL);
        ViaI2CInit(pScrn);
    }

    if (!xf86LoadSubModule(pScrn, "ddc")) {
	VIAFreeRec(pScrn);
	return FALSE;
    } else {
	xf86LoaderReqSymLists(ddcSymbols, NULL);
	
	if (pVia->pI2CBus1) {
	    pVia->DDC1 = xf86DoEDID_DDC2(pScrn->scrnIndex, pVia->pI2CBus1);
	    if (pVia->DDC1) {
		xf86PrintEDID(pVia->DDC1);
		xf86SetDDCproperties(pScrn, pVia->DDC1);
	    }
	}
    }

    ViaOutputsDetect(pScrn);
    if (!ViaOutputsSelect(pScrn)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No outputs possible.\n");
	VIAFreeRec(pScrn);
	return FALSE;
    }

    if (pBIOSInfo->PanelActive && ((pVia->Chipset == VIA_K8M800) ||
				   (pVia->Chipset == VIA_PM800) ||
                    (pVia->Chipset == VIA_VM800) ||
                    (pVia->Chipset == VIA_K8M890)|| 
		    (pVia->Chipset == VIA_P4M900))) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Panel on K8M800, PM800 ,VM800, K8M890 or P4M900 is"
		   " currently not supported.\n");
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Using VBE to set modes to"
		   " work around this.\n");
	UseVBEModes = TRUE;
    }

    pVia->pVbe = NULL;
    if (UseVBEModes) {
	/* VBE doesn't properly initialise int10 itself */
	if (xf86LoadSubModule(pScrn, "int10") && xf86LoadSubModule(pScrn, "vbe")) {
	    xf86LoaderReqSymLists(vbeSymbols, NULL);
	    pVia->pVbe = VBEExtendedInit(NULL, pVia->EntityIndex,
					 SET_BIOS_SCRATCH | RESTORE_BIOS_SCRATCH);
	}

	if (!pVia->pVbe)
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "VBE initialisation failed."
		       " Using builtin code to set modes.\n");
    }

    if (pVia->pVbe) {
	
	if (!ViaVbeModePreInit( pScrn )) {
	    VIAFreeRec(pScrn);
	    return FALSE;
	}	 

    } else {
	/* Add own Modes */
	ViaModesAttach(pScrn, pScrn->monitor);

	/*
	 * Setup the ClockRanges, which describe what clock ranges are available,
	 * and what sort of modes they can be used for.
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
	 * CLE266A: primary AdjustFrame only is able to use 24bits, so we are
	 * limited to 12x11bits; 4080x2048 (~2:1), 3344x2508 (4:3) or 2896x2896
	 * (1:1). 
	 * Test CLE266Cx, KM400, KM400A, K8M800, PM800, CN400 please.
	 *
	 * We should be able to limit the memory available for a mode to 32MB,
	 * yet xf86ValidateModes (or miScanLineWidth) fails to catch this properly
	 * (apertureSize).
	 */

	/* Select valid modes from those available */
	i = xf86ValidateModes(pScrn,
			      pScrn->monitor->Modes,    /* availModes */
			      pScrn->display->modes,    /* modeNames */
			      clockRanges,              /* list of clock ranges */
			      NULL,                     /* list of line pitches */
			      256,                      /* mini line pitch */
			      3344,                     /* max line pitch */
			      32*8,                      /* pitch inc (bits) */
			      128,                      /* min height */
			      2508,                     /* max height */
			      pScrn->display->virtualX, /* virtual width */
			      pScrn->display->virtualY, /* virtual height */
			      pVia->videoRambytes,      /* apertureSize */
			      LOOKUP_BEST_REFRESH);     /* lookup mode flags */


	if (i == -1) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86ValidateModes failure\n");
	    VIAFreeRec(pScrn);
	    return FALSE;
	}

	xf86PruneDriverModes(pScrn);

	if (i == 0 || pScrn->modes == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	    VIAFreeRec(pScrn);
	    return FALSE;
	}

    }
    
    /* Set up screen parameters. */
    pVia->Bpp = pScrn->bitsPerPixel >> 3;
    pVia->Bpl = pScrn->displayWidth * pVia->Bpp;

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);
    pScrn->currentMode = pScrn->modes;
    xf86PrintModes(pScrn);
    xf86SetDpi(pScrn, 0, 0);

#ifdef USE_FB
    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);

#else
    /* load bpp-specific modules */
    switch (pScrn->bitsPerPixel) {
        case 8:
            mod = "cfb";
            reqSym = "cfbScreenInit";
            break;
        case 16:
            mod = "cfb16";
            reqSym = "cfb16ScreenInit";
            break;
        case 32:
            mod = "cfb32";
            reqSym = "cfb32ScreenInit";
            break;
    }

    if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    xf86LoaderReqSymbols(reqSym, NULL);
#endif

    if (!pVia->NoAccel) {
#ifdef VIA_HAVE_EXA
	if(pVia->useEXA) {
#if (EXA_VERSION_MAJOR >= 2)
	    XF86ModReqInfo req;
	    int errmaj, errmin;
	    
	    memset(&req, 0, sizeof(req));
	    req.majorversion = 2;
	    req.minorversion = 0;
	    if (!LoadSubModule(pScrn->module, "exa", NULL, NULL, NULL, &req,
			       &errmaj, &errmin)) {
		    LoaderErrorMsg(NULL, "exa", errmaj, errmin);
		    VIAFreeRec(pScrn);
		    return FALSE;
	    }
	    
#else
	    
	    if(!xf86LoadSubModule(pScrn, "exa")) {
		VIAFreeRec(pScrn);
		return FALSE;
	    }
#endif /* EXA_VERSION */
	    xf86LoaderReqSymLists(exaSymbols, NULL);
	}
#endif /* VIA_HAVE_EXA */
        if(!xf86LoadSubModule(pScrn, "xaa")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    if (pVia->hwcursor) {
        if (!xf86LoadSubModule(pScrn, "ramdac")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    if (pVia->shadowFB) {
        if (!xf86LoadSubModule(pScrn, "shadowfb")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    VIAUnmapMem(pScrn);

    return TRUE;
}


static Bool VIAEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr      pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    Bool        ret;

    /* FIXME: Rebind AGP memory here */
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAEnterVT\n"));
    
    if (pVia->pVbe) {
	if (pVia->vbeSR) 
	    ViaVbeSaveRestore(pScrn, MODE_SAVE);
	else
	    VIASave(pScrn);
	ret = ViaVbeSetMode(pScrn, pScrn->currentMode); 
    } else {
	VIASave(pScrn);
	ret = VIAWriteMode(pScrn, pScrn->currentMode);
    }
    vgaHWUnlock(hwp);

    VIASaveScreen(pScrn->pScreen, SCREEN_SAVER_ON);


    /* Patch for APM suspend resume, HWCursor has garbage */
    if (pVia->hwcursor)
	ViaCursorRestore(pScrn); 

    /* restore video status */
    if (!pVia->IsSecondary)
        viaRestoreVideo(pScrn);

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
        kickVblank(pScrn);
	VIADRIRingBufferInit(pScrn);
	viaDRIOffscreenRestore(pScrn);
    }
#endif 

    if (pVia->NoAccel) {
	memset(pVia->FBBase, 0x00, pVia->Bpl * pScrn->virtualY);
    } else {
	viaAccelFillRect(pScrn, 0, 0, pScrn->displayWidth, pScrn->virtualY,
			 0x00000000);
	viaAccelSyncMarker(pScrn);
    }

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
	DRIUnlock(screenInfo.screens[scrnIndex]);
    }
#endif 

    return ret;
}


static void VIALeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    hwp = VGAHWPTR(pScrn);
    VIAPtr      pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIALeaveVT\n"));

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
	volatile drm_via_sarea_t *saPriv = (drm_via_sarea_t *)
	    DRIGetSAREAPrivate(pScrn->pScreen);

	DRILock(screenInfo.screens[scrnIndex], 0);
	saPriv->ctxOwner = ~0;
    }
#endif

    viaAccelSync(pScrn);

    /*
     * A soft reset helps fix 3D hang on VT switch.
     */
    if (pVia->Chipset != VIA_K8M890 && pVia->Chipset != VIA_P4M900)
        hwp->writeSeq(hwp, 0x1A, pVia->SavedReg.SR1A | 0x40);

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
	VIADRIRingBufferCleanup(pScrn);
	viaDRIOffscreenSave(pScrn);
    }
#endif

    if (pVia->VQEnable) 
        viaDisableVQ(pScrn);

    /* Save video status and turn off all video activities */

    if (!pVia->IsSecondary)
        viaSaveVideo(pScrn);

    if (pVia->hwcursor)
	ViaCursorStore(pScrn);

    if (pVia->pVbe && pVia->vbeSR) 
	ViaVbeSaveRestore(pScrn, MODE_RESTORE);
    else
	VIARestore(pScrn);

    vgaHWLock(hwp);

}


static void
VIASave(ScrnInfoPtr pScrn)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    VIARegPtr       Regs = &pVia->SavedReg;
    int i;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIASave\n"));

    if(pVia->IsSecondary)
    {
        DevUnion* pPriv;
        VIAEntPtr pVIAEnt;
        VIAPtr   pVia1;
        vgaHWPtr hwp1;

	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Secondary\n"));

        pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
              gVIAEntityIndex);
        pVIAEnt = pPriv->ptr;
        hwp1 = VGAHWPTR(pVIAEnt->pPrimaryScrn);
        pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
        hwp->SavedReg = hwp1->SavedReg;
        pVia->SavedReg = pVia1->SavedReg;
    }
    else {
	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Primary\n"));

        vgaHWProtect(pScrn, TRUE);

        if (xf86IsPrimaryPci(pVia->PciInfo))
            vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_ALL);
        else
            vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_MODE);

        /* Unlock extended regs */
	hwp->writeSeq(hwp, 0x10, 0x01);

	Regs->SR14 = hwp->readSeq(hwp, 0x14);
	Regs->SR15 = hwp->readSeq(hwp, 0x15);
	Regs->SR16 = hwp->readSeq(hwp, 0x16);
	Regs->SR17 = hwp->readSeq(hwp, 0x17);
	Regs->SR18 = hwp->readSeq(hwp, 0x18);
	Regs->SR19 = hwp->readSeq(hwp, 0x19);
	Regs->SR1A = hwp->readSeq(hwp, 0x1A);
	Regs->SR1B = hwp->readSeq(hwp, 0x1B);
	Regs->SR1C = hwp->readSeq(hwp, 0x1C);
	Regs->SR1D = hwp->readSeq(hwp, 0x1D);
	Regs->SR1E = hwp->readSeq(hwp, 0x1E);
	Regs->SR1F = hwp->readSeq(hwp, 0x1F);

	Regs->SR22 = hwp->readSeq(hwp, 0x22);
	Regs->SR23 = hwp->readSeq(hwp, 0x23);
	Regs->SR24 = hwp->readSeq(hwp, 0x24);
	Regs->SR25 = hwp->readSeq(hwp, 0x25);
	Regs->SR26 = hwp->readSeq(hwp, 0x26);
	Regs->SR27 = hwp->readSeq(hwp, 0x27);
	Regs->SR28 = hwp->readSeq(hwp, 0x28);
	Regs->SR29 = hwp->readSeq(hwp, 0x29);
	Regs->SR2A = hwp->readSeq(hwp, 0x2A);
	Regs->SR2B = hwp->readSeq(hwp, 0x2B);

	Regs->SR2E = hwp->readSeq(hwp, 0x2E);	

        switch (pVia->Chipset)
        {
            case VIA_CLE266:
            case VIA_KM400:
                Regs->SR44 = hwp->readSeq(hwp, 0x44);
                Regs->SR45 = hwp->readSeq(hwp, 0x45);
                Regs->SR46 = hwp->readSeq(hwp, 0x46);
                Regs->SR47 = hwp->readSeq(hwp, 0x47);
                break;
            default:
                Regs->SR44 = hwp->readSeq(hwp, 0x44);
                Regs->SR45 = hwp->readSeq(hwp, 0x45);
                Regs->SR46 = hwp->readSeq(hwp, 0x46);
                Regs->SR47 = hwp->readSeq(hwp, 0x47);
                /*Regs->SR4A = hwp->readSeq(hwp, 0x4a);
                Regs->SR4B = hwp->readSeq(hwp, 0x4b);
                Regs->SR4C = hwp->readSeq(hwp, 0x4c);*/
                break;
        }


        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Crtc...\n"));

	Regs->CR13 = hwp->readCrtc(hwp, 0x13);

	Regs->CR32 = hwp->readCrtc(hwp, 0x32);
	Regs->CR33 = hwp->readCrtc(hwp, 0x33);
	Regs->CR34 = hwp->readCrtc(hwp, 0x34);
	Regs->CR35 = hwp->readCrtc(hwp, 0x35);
	Regs->CR36 = hwp->readCrtc(hwp, 0x36);

	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "TVSave...\n"));
	if (pBIOSInfo->TVI2CDev)
	    ViaTVSave(pScrn);

        /* Save LCD control regs */
        for (i = 0; i < 68; i++)
	    Regs->CRTCRegs[i] = hwp->readCrtc(hwp, i + 0x50);

        vgaHWProtect(pScrn, FALSE);
    }
}

static void 
VIARestore(ScrnInfoPtr pScrn)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    VIARegPtr       Regs = &pVia->SavedReg;
    int             i;
    CARD8           tmp;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIARestore\n"));

    /* Secondary? */

    vgaHWProtect(pScrn, TRUE);

    /* Unlock extended regs */
    hwp->writeSeq(hwp, 0x10, 0x01);

    hwp->writeCrtc(hwp, 0x6A, 0x00);
    hwp->writeCrtc(hwp, 0x6B, 0x00);
    hwp->writeCrtc(hwp, 0x6C, 0x00);
 
    if (pBIOSInfo->TVI2CDev)
	ViaTVRestore(pScrn);

    /* Restore the standard vga regs */
    if (xf86IsPrimaryPci(pVia->PciInfo))
        vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_ALL);
    else
        vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_MODE);

    /* Restore extended regs */
    hwp->writeSeq(hwp, 0x14, Regs->SR14);
    hwp->writeSeq(hwp, 0x15, Regs->SR15);
    hwp->writeSeq(hwp, 0x16, Regs->SR16);
    hwp->writeSeq(hwp, 0x17, Regs->SR17);
    hwp->writeSeq(hwp, 0x18, Regs->SR18);
    hwp->writeSeq(hwp, 0x19, Regs->SR19);
    hwp->writeSeq(hwp, 0x1A, Regs->SR1A);
    hwp->writeSeq(hwp, 0x1B, Regs->SR1B);
    hwp->writeSeq(hwp, 0x1C, Regs->SR1C);
    hwp->writeSeq(hwp, 0x1D, Regs->SR1D);
    hwp->writeSeq(hwp, 0x1E, Regs->SR1E);
    hwp->writeSeq(hwp, 0x1F, Regs->SR1F);

    hwp->writeSeq(hwp, 0x22, Regs->SR22);
    hwp->writeSeq(hwp, 0x23, Regs->SR23);
    hwp->writeSeq(hwp, 0x24, Regs->SR24);
    hwp->writeSeq(hwp, 0x25, Regs->SR25);
    hwp->writeSeq(hwp, 0x26, Regs->SR26);
    hwp->writeSeq(hwp, 0x27, Regs->SR27);
    hwp->writeSeq(hwp, 0x28, Regs->SR28);
    hwp->writeSeq(hwp, 0x29, Regs->SR29);
    hwp->writeSeq(hwp, 0x2A, Regs->SR2A);
    hwp->writeSeq(hwp, 0x2B, Regs->SR2B);

    hwp->writeSeq(hwp, 0x2E, Regs->SR2E);
    
    hwp->writeSeq(hwp, 0x44, Regs->SR44);
    hwp->writeSeq(hwp, 0x45, Regs->SR45);
    hwp->writeSeq(hwp, 0x46, Regs->SR46);
    hwp->writeSeq(hwp, 0x47, Regs->SR47);

    /* Reset dotclocks */
    ViaSeqMask(hwp, 0x40, 0x06, 0x06);
    ViaSeqMask(hwp, 0x40, 0x00, 0x06);
	
    hwp->writeCrtc(hwp, 0x13, Regs->CR13);
    hwp->writeCrtc(hwp, 0x32, Regs->CR32);
    hwp->writeCrtc(hwp, 0x33, Regs->CR33);
    hwp->writeCrtc(hwp, 0x34, Regs->CR34);
    hwp->writeCrtc(hwp, 0x35, Regs->CR35);
    hwp->writeCrtc(hwp, 0x36, Regs->CR36);

    /* Restore LCD control regs */
    for (i = 0; i < 68; i++)
        hwp->writeCrtc(hwp, i + 0x50, Regs->CRTCRegs[i]);

    if (pBIOSInfo->PanelActive)
	ViaLCDPower(pScrn, TRUE);

    ViaDisablePrimaryFIFO(pScrn);

    /* Reset clock */
    tmp = hwp->readMiscOut(hwp);
    hwp->writeMiscOut(hwp, tmp);

    vgaHWProtect(pScrn, FALSE);
}

static Bool
VIAMapMMIO(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAMapMMIO\n"));

    pVia->FrameBufferBase = pVia->PciInfo->memBase[0];
    pVia->MmioBase = pVia->PciInfo->memBase[1];

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping MMIO @ 0x%lx with size 0x%x\n",
               pVia->MmioBase, VIA_MMIO_REGSIZE);

    pVia->MapBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                                  pVia->MmioBase, VIA_MMIO_REGSIZE);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping BitBlt MMIO @ 0x%lx with size 0x%x\n",
               pVia->MmioBase + VIA_MMIO_BLTBASE, VIA_MMIO_BLTSIZE);

    pVia->BltBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                                  pVia->MmioBase + VIA_MMIO_BLTBASE,
                                  VIA_MMIO_BLTSIZE);

    if (!pVia->MapBase || !pVia->BltBase) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Internal error: cound not map registers\n");
        return FALSE;
    }

    /* Memory mapped IO for Video Engine */
    pVia->VidMapBase = pVia->MapBase + 0x200;
    /* Memory mapped IO for Mpeg Engine */
    pVia->MpegMapBase = pVia->MapBase + 0xc00;

    /* Set up MMIO vgaHW */
    {
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	CARD8 val;

	vgaHWSetMmioFuncs(hwp, pVia->MapBase, 0x8000);
	
	val = hwp->readEnable(hwp);
	hwp->writeEnable(hwp, val | 0x01);
	
	val = hwp->readMiscOut(hwp);
	hwp->writeMiscOut(hwp, val | 0x01);
	
	/* Unlock Extended IO Space */
	hwp->writeSeq(hwp, 0x10, 0x01);

	/* Enable MMIO */
	if (pVia->IsSecondary)
	    ViaSeqMask(hwp, 0x1A, 0x38, 0x38);
	else
	    ViaSeqMask(hwp, 0x1A, 0x68, 0x68);

	vgaHWGetIOBase(hwp);
    }

    return TRUE;
}


static Bool VIAMapFB(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAMapFB\n"));
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping framebuffer @ 0x%lx with size 0x%lx\n",
               pVia->FrameBufferBase, pVia->videoRambytes);

    if (pVia->videoRambytes) {

	/*
	 * FIXME: This is a hack to get rid of offending wrongly sized
	 * MTRR regions set up by the VIA BIOS. Should be taken care of
	 * in the OS support layer.
	 */

        unsigned char *tmp; 
        tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
			    pVia->PciTag, pVia->FrameBufferBase,
			    pVia->videoRambytes);
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)tmp,
                        pVia->videoRambytes);

	/*
	 * And, as if this wasn't enough, 2.6 series kernels doesn't
	 * remove MTRR regions on the first attempt. Try again.
	 */

        tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
			    pVia->PciTag, pVia->FrameBufferBase,
			    pVia->videoRambytes);
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)tmp,
                        pVia->videoRambytes);

	/*
	 * End of hack.
	 */

        pVia->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                                     pVia->PciTag, pVia->FrameBufferBase,
                                     pVia->videoRambytes);

        if (!pVia->FBBase) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Internal error: could not map framebuffer\n");
            return FALSE;
        }

        pVia->FBFreeStart = (pScrn->displayWidth * pScrn->bitsPerPixel >> 3) *
                            pScrn->virtualY;
        pVia->FBFreeEnd = pVia->videoRambytes;

        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                   "Frame buffer start: %p, free start: 0x%x end: 0x%x\n",
                   pVia->FBBase, pVia->FBFreeStart, pVia->FBFreeEnd);
    }

    pScrn->memPhysBase = pVia->PciInfo->memBase[0];
    pScrn->fbOffset = 0;
    if(pVia->IsSecondary) pScrn->fbOffset = pScrn->videoRam << 10;

    return TRUE;
}


static void
VIAUnmapMem(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAUnmapMem\n"));

    /* Disable MMIO */
    ViaSeqMask(VGAHWPTR(pScrn), 0x1A, 0x00, 0x60);

    if (pVia->MapBase)
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pVia->MapBase, VIA_MMIO_REGSIZE);

    if (pVia->BltBase)
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pVia->BltBase, VIA_MMIO_BLTSIZE);

    if (pVia->FBBase)
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pVia->FBBase, pVia->videoRambytes);
}

static void
VIALoadRgbLut(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors, VisualPtr pVisual)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    int i, j, index;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIALoadRgbLut\n"));

    hwp->enablePalette(hwp);
    hwp->writeDacMask(hwp, 0xFF);

    /* We need the same palette contents for both 16 and 24 bits, but X doesn't
     * play: X's colormap handling is hopelessly intertwined with almost every
     * X subsystem.  So we just space out RGB values over the 256*3. */

    switch (pScrn->bitsPerPixel) {
	case 16:
  	    for (i = 0; i < numColors; i++) {
	        index = indices[i];
	        hwp->writeDacWriteAddr(hwp, index * 4);
	        for (j = 0; j < 4; j++) {
		    hwp->writeDacData(hwp, colors[index/2].red);
		    hwp->writeDacData(hwp, colors[index].green);
		    hwp->writeDacData(hwp, colors[index/2].blue);
	        }
	    }
	    break;
	case 8:
	case 24:
	case 32:
	    for (i = 0; i < numColors; i++) {
	        index = indices[i];
	        hwp->writeDacWriteAddr(hwp, index);
	        hwp->writeDacData(hwp, colors[index].red);
	        hwp->writeDacData(hwp, colors[index].green);
	        hwp->writeDacData(hwp, colors[index].blue);
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
VIALoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
	       LOCO *colors, VisualPtr pVisual)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    int i, index;
    int SR1A, SR1B, CR67, CR6A;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIALoadPalette\n"));

    if (pScrn->bitsPerPixel != 8) {
	switch(pVia->Chipset) {
	    case VIA_CLE266:
	    case VIA_KM400:
		ViaSeqMask(hwp, 0x16, 0x80, 0x80);
		break;
	    default:
		ViaCrtcMask(hwp, 0x33, 0x80, 0x80);
		break;
	}

	ViaSeqMask(hwp, 0x1A, 0x00, 0x01);
	VIALoadRgbLut(pScrn, numColors, indices, colors, pVisual);

	/* If secondary is enabled, adjust its palette too. */
	if (hwp->readCrtc(hwp, 0x6A) & 0x80) {
	    ViaSeqMask(hwp, 0x1A, 0x01, 0x01);
	    ViaCrtcMask(hwp, 0x6A, 0x02, 0x02);
	    switch(pVia->Chipset) {
		case VIA_K8M800:
		case VIA_PM800:
		    break;
	        default:
		    ViaSeqMask(hwp, 0x6A, 0x20, 0x20);
		    break;
	    }
	    VIALoadRgbLut(pScrn, numColors, indices, colors, pVisual);
	}

	return;
    }

    SR1A = hwp->readSeq(hwp, 0x1A);
    SR1B = hwp->readSeq(hwp, 0x1B);
    CR67 = hwp->readCrtc(hwp, 0x67);
    CR6A = hwp->readCrtc(hwp, 0x6A);

    if (pVia->IsSecondary) {
	ViaSeqMask(hwp, 0x1A, 0x01, 0x01);
	ViaSeqMask(hwp, 0x1B, 0x80, 0x80);
	ViaCrtcMask(hwp, 0x67, 0x00, 0xC0);
	ViaCrtcMask(hwp, 0x6A, 0xC0, 0xC0);
    }
    
    for (i = 0; i < numColors; i++) {
	index = indices[i];
	hwp->writeDacWriteAddr(hwp, index);
	hwp->writeDacData(hwp, colors[index].red);
	hwp->writeDacData(hwp, colors[index].green);
	hwp->writeDacData(hwp, colors[index].blue);
    }

    if (pVia->IsSecondary) {
	hwp->writeSeq(hwp, 0x1A, SR1A);
	hwp->writeSeq(hwp, 0x1B, SR1B);
	hwp->writeCrtc(hwp, 0x67, CR67);
	hwp->writeCrtc(hwp, 0x6A, CR6A);

	/* Screen 0 palette was changed by mode setting of Screen 1,
	 * so load again */
	for (i = 0; i < numColors; i++) {
	    index = indices[i];
	    hwp->writeDacWriteAddr(hwp, index);
	    hwp->writeDacData(hwp, colors[index].red);
	    hwp->writeDacData(hwp, colors[index].green);
	    hwp->writeDacData(hwp, colors[index].blue);
        }
    }
}

/*
 *
 */
static Bool 
VIAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    
    pScrn->pScreen = pScreen;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAScreenInit\n"));

    if (!VIAMapFB(pScrn))
        return FALSE;

    if (!VIAMapMMIO(pScrn))
        return FALSE;

    if (pVia->pVbe && pVia->vbeSR) {
	ViaVbeSaveRestore(pScrn, MODE_SAVE);
    } else {
	VIASave(pScrn);
    }

    vgaHWUnlock(hwp);

    pVia->FirstInit = TRUE;
    if (pVia->pVbe) {
        vgaHWBlankScreen(pScrn, FALSE);
	if (!ViaVbeSetMode(pScrn, pScrn->currentMode)) {
	    vgaHWBlankScreen(pScrn, TRUE);
	    return FALSE;
	}
    } else {
        vgaHWBlankScreen(pScrn, FALSE);
	if (!VIAWriteMode(pScrn, pScrn->currentMode)) {
	    vgaHWBlankScreen(pScrn, TRUE);
	    return FALSE;
	}
    }
    pVia->FirstInit = FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    VIASaveScreen(pScreen, SCREEN_SAVER_ON);
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Blanked\n"));

    miClearVisualTypes();

    if (pScrn->bitsPerPixel > 8 && !pVia->IsSecondary) {
        if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
                              pScrn->rgbBits, pScrn->defaultVisual))
            return FALSE;
        if (!miSetPixmapDepths())
            return FALSE;
    } else {
        if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
                              pScrn->rgbBits, pScrn->defaultVisual))
            return FALSE;
        if (!miSetPixmapDepths())
            return FALSE;
    }

#ifdef XF86DRI
    pVia->directRenderingEnabled = VIADRIScreenInit(pScreen);
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Visuals set up\n"));

    if (!VIAInternalScreenInit(scrnIndex, pScreen))
	return FALSE;

    xf86SetBlackWhitePixels(pScreen);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- B & W\n"));

    if (pScrn->bitsPerPixel > 8) {
        VisualPtr visual;

        visual = pScreen->visuals + pScreen->numVisuals;
        while (--visual >= pScreen->visuals) {
            if ((visual->class | DynamicClass) == DirectColor) {
                visual->offsetRed = pScrn->offset.red;
                visual->offsetGreen = pScrn->offset.green;
                visual->offsetBlue = pScrn->offset.blue;
                visual->redMask = pScrn->mask.red;
                visual->greenMask = pScrn->mask.green;
                visual->blueMask = pScrn->mask.blue;
            }
        }
    }

#ifdef USE_FB
    /* must be after RGB ordering fixed */
    fbPictureInit(pScreen, 0, 0);
#endif

    if (!pVia->NoAccel) {
        viaInitAccel(pScreen);
    }
 
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    /*xf86SetSilkenMouse(pScreen);*/
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Backing store set up\n"));

    if(!pVia->shadowFB)         /* hardware cursor needs to wrap this layer */
        VIADGAInit(pScreen);

    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- SW cursor set up\n"));

    if (pVia->hwcursor) {
        if (!VIAHWCursorInit(pScreen)) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Hardware cursor initialization failed\n");
        }
    }

    if (pVia->shadowFB)
	ViaShadowFBInit(pScrn, pScreen);

    if (!miCreateDefColormap(pScreen))
        return FALSE;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Def Color map set up\n"));

    if (!xf86HandleColormaps(pScreen, 256, 8, VIALoadPalette, NULL,
                             CMAP_RELOAD_ON_MODE_SWITCH
                             | CMAP_PALETTED_TRUECOLOR))
        return FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Palette loaded\n"));

    pVia->CloseScreen = pScreen->CloseScreen;
    pScreen->SaveScreen = VIASaveScreen;
    pScreen->CloseScreen = VIACloseScreen;

    xf86DPMSInit(pScreen, VIADPMS, 0);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- DPMS set up\n"));

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Color maps etc. set up\n"));
    pVia->agpDMA = FALSE;

#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
	pVia->directRenderingEnabled = VIADRIFinishScreenInit(pScreen);

    if (pVia->directRenderingEnabled) {
	VIADRIPtr pVIADRI = pVia->pDRIInfo->devPrivate;

        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "direct rendering enabled\n");
	pVia->agpDMA = pVia->dma2d && pVIADRI->ringBufActive;
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "direct rendering disabled\n");
    }
#endif
    if (!pVia->NoAccel)
        viaFinishInitAccel(pScreen);

    if (pVia->NoAccel) {
	memset(pVia->FBBase, 0x00, pVia->videoRambytes);
    } else {
#ifdef XF86DRI
	if (pVia->directRenderingEnabled)
	    DRILock(screenInfo.screens[scrnIndex], 0);
#endif
	viaAccelFillRect(pScrn, pScrn->frameX0, pScrn->frameY0, 
		       pScrn->displayWidth, pScrn->virtualY,
		       0x00000000);
	viaAccelSyncMarker(pScrn);
#ifdef XF86DRI
	if (pVia->directRenderingEnabled)
	    	DRIUnlock(screenInfo.screens[scrnIndex]);
#endif
    }
    vgaHWBlankScreen(pScrn, TRUE);

    if (pVia->NoAccel) {

	/*
	 * This is only for Xv in Noaccel path, and since Xv is in some
	 * sense accelerated, it might be a better idea to disable it
	 * altogether.
	 */ 

        BoxRec AvailFBArea;

        AvailFBArea.x1 = 0;
        AvailFBArea.y1 = 0;
        AvailFBArea.x2 = pScrn->displayWidth;
        AvailFBArea.y2 = pScrn->virtualY + 1;
	pVia->FBFreeStart=(AvailFBArea.y2 + 1)*pVia->Bpl;
	xf86InitFBManager(pScreen, &AvailFBArea);
	VIAInitLinear(pScreen);
	pVia->driSize = (pVia->FBFreeEnd - pVia->FBFreeStart - pVia->Bpl);
    }

    viaInitVideo(pScreen);

    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

#ifdef HAVE_DEBUG 
    if (pVia->PrintVGARegs) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAScreenInit: Printing VGA registers.\n");
	ViaVgahwPrint(VGAHWPTR(pScrn));
    }

    if (pVia->PrintTVRegs) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAScreenInit: Printing TV registers.\n");
	ViaTVPrintRegs(pScrn);
    }
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Done\n"));
    return TRUE;
}


static int VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
    VIAPtr          pVia = VIAPTR(pScrn);
    int             width, height, displayWidth;
    unsigned char   *FBStart;

    xf86DrvMsg(scrnIndex, X_INFO, "VIAInternalScreenInit\n");

    displayWidth = pScrn->displayWidth;

    if (pVia->rotate) {
        height = pScrn->virtualX;
        width = pScrn->virtualY;
    } else {
        width = pScrn->virtualX;
        height = pScrn->virtualY;
    }

    if (pVia->shadowFB) {
        pVia->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pVia->ShadowPtr = xalloc(pVia->ShadowPitch * height);
        displayWidth = pVia->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pVia->ShadowPtr;
    }
    else {
        pVia->ShadowPtr = NULL;
        FBStart = pVia->FBBase;
    }

#ifdef USE_FB
    return fbScreenInit(pScreen, FBStart, width, height,
			pScrn->xDpi, pScrn->yDpi, displayWidth,
			pScrn->bitsPerPixel);
#else
    switch (pScrn->bitsPerPixel) {
    case 8:
        return cfbScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
			     pScrn->yDpi, displayWidth);
    case 16:
        return cfb16ScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
			       pScrn->yDpi, displayWidth);
    case 32:
        return cfb32ScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
			       pScrn->yDpi, displayWidth);
    default:
        xf86DrvMsg(scrnIndex, X_ERROR, "Internal error: invalid bpp (%d) in "
		   "VIAInternalScreenInit\n", pScrn->bitsPerPixel);
        return FALSE;
    }
#endif
}

static Bool 
VIAWriteMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAWriteMode\n"));

    pVia->OverlaySupported = FALSE;

    if (!vgaHWInit(pScrn, mode))
        return FALSE;

    pScrn->vtSema = TRUE;

    if (!pVia->IsSecondary)
	ViaModePrimary(pScrn, mode);
    else
	ViaModeSecondary(pScrn, mode);

    /* Enable the graphics engine. */
    if (!pVia->NoAccel) {
#if defined(XF86DRI) || defined(VIA_HAVE_EXA)
	VIAInitialize3DEngine(pScrn);
#endif 
	viaInitialize2DEngine(pScrn);
    }

    VIAAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return TRUE;
}


static Bool VIACloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    hwp = VGAHWPTR(pScrn);
    VIAPtr      pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIACloseScreen\n"));
    /* Is the display currently visible ? */
    if(pScrn->vtSema)
    {

#ifdef XF86DRI
        if (pVia->directRenderingEnabled)
	  DRILock(screenInfo.screens[scrnIndex], 0);
#endif
        /* Wait Hardware Engine idle to exit graphical mode */
        viaAccelSync(pScrn);
 

	/* A soft reset Fixes 3D Hang after X restart */
        if (pVia->Chipset != VIA_K8M890 && pVia->Chipset != VIA_P4M900)	
            hwp->writeSeq(hwp, 0x1A, pVia->SavedReg.SR1A | 0x40);

	if (!pVia->IsSecondary) {
            /* Turn off all video activities */
            viaExitVideo(pScrn); 

            VIAHideCursor(pScrn);
        }

        if (pVia->VQEnable)
	    viaDisableVQ(pScrn);
    }

#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
	VIADRICloseScreen(pScreen);
#endif

    viaExitAccel(pScreen);
    if (pVia->CursorInfoRec) {
        xf86DestroyCursorInfoRec(pVia->CursorInfoRec);
        pVia->CursorInfoRec = NULL;
    }
    if (pVia->ShadowPtr) {
        xfree(pVia->ShadowPtr);
        pVia->ShadowPtr = NULL;
    }
    if (pVia->DGAModes) {
        xfree(pVia->DGAModes);
        pVia->DGAModes = NULL;
    }

    if (pScrn->vtSema) {
	if (pVia->pVbe && pVia->vbeSR)
	    ViaVbeSaveRestore(pScrn, MODE_RESTORE); 
	else 
	    VIARestore(pScrn);

	vgaHWLock(hwp);
        VIAUnmapMem(pScrn);
    }
    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = pVia->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/*
 * This only gets called when a screen is being deleted.  It does not
 * get called routinely at the end of a server generation.
 */
static void VIAFreeScreen(int scrnIndex, int flags)
{
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAFreeScreen\n"));

    VIAFreeRec(xf86Screens[scrnIndex]);

    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	    vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

static Bool VIASaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}

static void
VIAAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    hwp = VGAHWPTR(pScrn);
    VIAPtr      pVia = VIAPTR(pScrn);
    CARD32      Base;

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAAdjustFrame\n"));

    if (pVia->pVbe) {
	ViaVbeAdjustFrame(scrnIndex, x, y, flags);
	VIAVidAdjustFrame(pScrn, x, y);
	return;
    }

    Base = (y * pScrn->displayWidth + x) * (pScrn->bitsPerPixel / 8);

    /* now program the start address registers */
    if (pVia->IsSecondary) {
	Base = (Base + pScrn->fbOffset) >> 3;
	ViaCrtcMask(hwp, 0x62, (Base & 0x7F) << 1 , 0xFE);
	hwp->writeCrtc(hwp, 0x63, (Base & 0x7F80) >>  7);
	hwp->writeCrtc(hwp, 0x64, (Base & 0x7F8000) >>  15);
    }
    else {
        Base = Base >> 1;
        hwp->writeCrtc(hwp, 0x0C, (Base & 0xFF00) >> 8);
	hwp->writeCrtc(hwp, 0x0D, Base & 0xFF);
	hwp->writeCrtc(hwp, 0x34, (Base & 0xFF0000) >> 16);
#if 0
	/* The CLE266A doesn't have this implemented, it seems. -- Luc */
	ViaCrtcMask(hwp, 0x48, Base >> 24, 0x03);
#endif
    }

    VIAVidAdjustFrame(pScrn, x, y);
}


static Bool
VIASwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr      pVia = VIAPTR(pScrn);
    Bool        ret;
    
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIASwitchMode\n"));
    
#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
	DRILock(screenInfo.screens[scrnIndex], 0);
#endif
    
    viaAccelSync(pScrn);
    
#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
	VIADRIRingBufferCleanup(pScrn); 
#endif
    
    if (pVia->VQEnable)
	viaDisableVQ(pScrn);
    
    if (pVia->pVbe)
	ret = ViaVbeSetMode(pScrn, mode);
    else
	ret = VIAWriteMode(pScrn, mode);

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
    	kickVblank(pScrn);
	VIADRIRingBufferInit(pScrn);
	DRIUnlock(screenInfo.screens[scrnIndex]);
    }
#endif 
    return ret;
    
}


static void VIADPMS(ScrnInfoPtr pScrn, int mode, int flags)
{
    vgaHWPtr        hwp = VGAHWPTR(pScrn);
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    CARD8           val;

    if (pVia->pVbe) {
	ViaVbeDPMS(pScrn, mode, flags);
	return;
    }

    /* Clear DPMS setting */
    val = hwp->readCrtc(hwp, 0x36);
    val &= 0xCF;

    /* Turn Off CRT, if user doesn't want crt on */
    if (!pVia->IsSecondary && !pBIOSInfo->CrtActive)
        val |= 0x30;

    switch (mode) {
    case DPMSModeOn:
        if (pBIOSInfo->PanelActive)
            ViaLCDPower(pScrn, TRUE);

        if (pBIOSInfo->TVActive)
	    ViaTVPower(pScrn, TRUE);

	hwp->writeCrtc(hwp, 0x36, val);
        break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
        if (pBIOSInfo->PanelActive)
            ViaLCDPower(pScrn, FALSE);

	if (pBIOSInfo->TVActive)
	    ViaTVPower(pScrn, FALSE);

        val |= 0x30;
	hwp->writeCrtc(hwp, 0x36, val);
        break;
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid DPMS mode %d\n", mode);
        break;
    }
    return;
}

#if defined(XF86DRI) || defined(VIA_HAVE_EXA)
void
VIAInitialize3DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int i;

    VIASETREG(VIA_REG_TRANSET, 0x00010000);

    for (i = 0; i <= 0x7D; i++)
      {
	VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
      }

    VIASETREG(VIA_REG_TRANSET, 0x00020000);

    for (i = 0; i <= 0x94; i++)
      {
	VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
      }

    VIASETREG(VIA_REG_TRANSPACE, 0x82400000);

    VIASETREG(VIA_REG_TRANSET, 0x01020000);


    for (i = 0; i <= 0x94; i++)
      {
	VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
      }

    VIASETREG(VIA_REG_TRANSPACE, 0x82400000);
    VIASETREG(VIA_REG_TRANSET, 0xfe020000);

    for (i = 0; i <= 0x03; i++)
      {
	VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
      }

    VIASETREG(VIA_REG_TRANSET, 0x00030000);

    for (i = 0; i <= 0xff; i++)
      {
	VIASETREG(VIA_REG_TRANSPACE, 0);
      }
    VIASETREG(VIA_REG_TRANSET, 0x00100000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00333004);
    VIASETREG(VIA_REG_TRANSPACE, 0x10000002);
    VIASETREG(VIA_REG_TRANSPACE, 0x60000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x61000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x62000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x63000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x64000000);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);

    if (pVia->ChipRev >= 3 )
      VIASETREG(VIA_REG_TRANSPACE,0x40008c0f);
    else
      VIASETREG(VIA_REG_TRANSPACE,0x4000800f);

    VIASETREG(VIA_REG_TRANSPACE,0x44000000);
    VIASETREG(VIA_REG_TRANSPACE,0x45080C04);
    VIASETREG(VIA_REG_TRANSPACE,0x46800408);
    VIASETREG(VIA_REG_TRANSPACE,0x50000000);
    VIASETREG(VIA_REG_TRANSPACE,0x51000000);
    VIASETREG(VIA_REG_TRANSPACE,0x52000000);
    VIASETREG(VIA_REG_TRANSPACE,0x53000000);

    
    VIASETREG(VIA_REG_TRANSET,0x00fe0000);
    VIASETREG(VIA_REG_TRANSPACE,0x08000001);
    VIASETREG(VIA_REG_TRANSPACE,0x0A000183);
    VIASETREG(VIA_REG_TRANSPACE,0x0B00019F);
    VIASETREG(VIA_REG_TRANSPACE,0x0C00018B);
    VIASETREG(VIA_REG_TRANSPACE,0x0D00019B);
    VIASETREG(VIA_REG_TRANSPACE,0x0E000000);
    VIASETREG(VIA_REG_TRANSPACE,0x0F000000);
    VIASETREG(VIA_REG_TRANSPACE,0x10000000);
    VIASETREG(VIA_REG_TRANSPACE,0x11000000);
    VIASETREG(VIA_REG_TRANSPACE,0x20000000);
}
#endif
