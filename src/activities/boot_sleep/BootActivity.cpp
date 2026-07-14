#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/HeadwaterEdition.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);

  // "Headwater Edition" attribution line (EB Garamond, baked 1-bit wordmark).
  // The bitmap is stored pre-rotated 90deg, so on screen it renders
  // HeadwaterEditionHeight wide x HeadwaterEditionWidth tall. For a rotated
  // drawImage the x arg sets the horizontal position and the y arg becomes the
  // framebuffer byte-column origin, so y must be a multiple of 8.
  const int hwX = (pageWidth - HeadwaterEditionHeight) / 2;
  const int hwY = (pageHeight / 2 + 98) & ~7;
  renderer.drawImage(HeadwaterEdition, hwX, hwY, HeadwaterEditionWidth, HeadwaterEditionHeight);

  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 135, tr(STR_BOOTING));
  // MIT attribution — rides inside the running firmware (the download page and the
  // repo LICENSE cover the other two surfaces).
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 48, tr(STR_HEADWATER_ATTRIBUTION));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
