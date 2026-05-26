#include <string.h>

#define PDLL_IMPLEMENTATION
#include "pdll.h"
#include "pd_api.h"

static PlaydateAPI *pd;
static LCDFont *font;

static pdll_t *lib;
static void (*ball_update)(void);
static void (*ball_randomize)(void);

static void load(void) {
  lib = pdll_open(pd, "ball", PDLL_FILE_PDX | PDLL_FILE_DATA | PDLL_ALIGN_256);
  if (!lib) {
    pd->system->logToConsole("pdll_open failed: %s", pdll_get_error());
    return;
  }
  ball_update = (void (*)(void))pdll_symbol(lib, "update");
  ball_randomize = (void (*)(void))pdll_symbol(lib, "randomize");
  pd->system->logToConsole("loaded ball: update=%p randomize=%p",
                           (void *)ball_update, (void *)ball_randomize);
}

static void unload(void) {
  if (!lib)
    return;
  pdll_close(lib);
  lib = NULL;
  ball_update = NULL;
  ball_randomize = NULL;
  pd->system->logToConsole("unloaded");
}

static int update(void *ud) {
  (void)ud;

  PDButtons pushed;
  pd->system->getButtonState(NULL, &pushed, NULL);
  if (pushed & kButtonA) {
    if (!lib)
      load();
    else if (ball_randomize)
      ball_randomize();
  }
  if (pushed & kButtonB)
    unload();

  pd->graphics->clear(kColorWhite);

  if (lib && ball_update) {
    ball_update();
  } else if (font) {
    const char *msg = "Press A to load ball.bin, B to unload it";
    pd->graphics->setFont(font);
    pd->graphics->drawText(msg, strlen(msg), kASCIIEncoding, 70, 110);
  }

  pd->system->drawFPS(0, 0);
  return 1;
}

int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, uint32_t arg) {
  (void)arg;
  if (event == kEventInit) {
    pd = playdate;
    const char *err = NULL;
    font = pd->graphics->loadFont("/System/Fonts/Asheville-Sans-14-Bold.pft",
                                  NULL);
    pd->display->setRefreshRate(50.0f);
    pd->system->setUpdateCallback(update, NULL);
  }
  return 0;
}
