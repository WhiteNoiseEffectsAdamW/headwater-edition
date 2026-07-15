#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/HeadwaterEdition.h"
#include "images/Logo56.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Headwater leads: the "Headwater Edition" wordmark is the hero, centred.
  // The bitmap is stored pre-rotated 90deg, so on screen it renders
  // HeadwaterEditionHeight wide x HeadwaterEditionWidth tall. For a rotated
  // drawImage the x arg sets the horizontal position and the y arg becomes the
  // framebuffer byte-column origin, so y must be a multiple of 8.
  const int hwX = (pageWidth - HeadwaterEditionHeight) / 2;
  const int hwY = (pageHeight / 2 - 40) & ~7;
  renderer.drawImage(HeadwaterEdition, hwX, hwY, HeadwaterEditionWidth, HeadwaterEditionHeight);

  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 10, tr(STR_BOOTING));

  // CrossPoint credit block, deliberately subordinate to the Headwater identity
  // above: a small logo + the MIT attribution line + version. This is a fork —
  // the code is CrossPoint's (MIT), so it's credited, but Headwater is the brand.
  // The attribution also satisfies the MIT notice inside the running firmware
  // (the download page and the repo LICENSE cover the other two surfaces).
  renderer.drawImage(Logo56, (pageWidth - Logo56Width) / 2, pageHeight - 110, Logo56Width, Logo56Height);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 48, tr(STR_HEADWATER_ATTRIBUTION));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
