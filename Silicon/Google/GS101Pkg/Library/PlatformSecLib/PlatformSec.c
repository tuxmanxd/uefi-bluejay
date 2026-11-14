/** @file
  GS101 platform SEC hooks.

  Two jobs before DXE starts:
    * stop the two cluster watchdogs. ABL arms watchdog_cl0 with a 30 s
      timeout; clearing WTCON.ENABLE (bit 5) and WTCON.RSTEN (bit 0) stops
      the counter. The PMU reset mask is secure-only on gs101, but the
      counter is what pixelios observed to matter on bluejay.
    * re-arm the DECON0 hardware trigger. ABL disables it, and direct writes
      to the bootloader framebuffer only appear after the command-mode
      trigger value is written back.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

#include <GS101.h>

#define WTCON_ENABLE  (1U << 5)
#define WTCON_RSTEN   (1U << 0)
#define WTCON_STOP    (WTCON_ENABLE | WTCON_RSTEN)

/**
  Fill the bootloader framebuffer with a single colour.

  This is the only feedback channel available on bluejay: there is no
  reachable UART, and the platform's debug output is not wired to the
  display. Each SEC stage paints a distinct colour so the on-screen state
  identifies the last stage that ran.
**/
STATIC
VOID
Gs101PaintFramebuffer (
  IN UINT32  Colour
  )
{
  volatile UINT32  *Framebuffer;
  UINTN            Index;
  UINTN            Count;

  Framebuffer = (volatile UINT32 *)(UINTN)GS101_FRAMEBUFFER_BASE;
  Count       = GS101_FRAMEBUFFER_SIZE / sizeof (UINT32);

  for (Index = 0; Index < Count; Index++) {
    Framebuffer[Index] = Colour;
  }
}

STATIC
VOID
Gs101StopWatchdog (
  IN CONST CHAR8  *RegionName
  )
{
  EFI_STATUS                   Status;
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  UINT32                       Value;

  Status = LocateMemoryRegionByName ((CHAR8 *)RegionName, &Region);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "SEC: %a region missing, watchdog untouched\n", RegionName));
    return;
  }

  Value = MmioRead32 ((UINTN)Region.Address);
  if ((Value & WTCON_STOP) != 0) {
    MmioWrite32 ((UINTN)Region.Address, Value & ~WTCON_STOP);
    DEBUG ((DEBUG_INFO, "SEC: %a WTCON %08x -> %08x\n",
            RegionName, Value, MmioRead32 ((UINTN)Region.Address)));
  }
}

STATIC
VOID
Gs101RearmDeconTrigger (
  VOID
  )
{
  EFI_STATUS                   Status;
  EFI_MEMORY_REGION_DESCRIPTOR Region;

  Status = LocateMemoryRegionByName ("DRM Decon", &Region);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "SEC: DRM Decon region missing, framebuffer trigger untouched\n"));
    return;
  }

  if ((Region.Length <= GS101_DECON_TRIG_CON) ||
      (Region.Address != GS101_DECON0_BASE))
  {
    DEBUG ((DEBUG_WARN, "SEC: unexpected DRM Decon region %lx+%lx\n",
            Region.Address, Region.Length));
    return;
  }

  MmioWrite32 ((UINTN)Region.Address + GS101_DECON_TRIG_CON, GS101_DECON_TRIG_EN);
  DEBUG ((DEBUG_INFO, "SEC: DECON0 trigger re-armed\n"));
}

VOID
PlatformInitialize (
  VOID
  )
{
  // Stage A: framebuffer is writable (red)
  Gs101PaintFramebuffer (0x00FF0000);
  Gs101RearmDeconTrigger ();
  // Stage B: DECON0 trigger re-armed (green)
  Gs101PaintFramebuffer (0x0000FF00);
  Gs101StopWatchdog ("Watchdog CL0");
  Gs101StopWatchdog ("Watchdog CL1");
  // Stage C: both cluster watchdogs cleared (blue)
  Gs101PaintFramebuffer (0x000000FF);
}
