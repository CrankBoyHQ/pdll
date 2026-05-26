#include <stdlib.h>
#include <string.h>

#include "pdll.h"
#include "pd_api.h"

static PlaydateAPI *pd;

#define BALL_R 14
static float x = 200.0f, y = 120.0f;
static float vx = 3.0f, vy = 2.4f;

static uint32_t rng = 0xe75498a8;
static uint32_t xrand(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}

static void randomize(void) {
  float sx = (xrand() & 1) ? 1.0f : -1.0f;
  float sy = (xrand() & 1) ? 1.0f : -1.0f;
  vx = sx * (1.5f + (float)(xrand() % 350) / 100.0f);
  vy = sy * (1.5f + (float)(xrand() % 350) / 100.0f);
}

static void update(void) {
  x += vx;
  y += vy;
  if (x < BALL_R) {
    x = BALL_R;
    vx = -vx;
  }
  if (x > 400 - BALL_R) {
    x = 400 - BALL_R;
    vx = -vx;
  }
  if (y < BALL_R) {
    y = BALL_R;
    vy = -vy;
  }
  if (y > 240 - BALL_R) {
    y = 240 - BALL_R;
    vy = -vy;
  }

  pd->graphics->fillEllipse((int)(x - BALL_R), (int)(y - BALL_R), BALL_R * 2,
                            BALL_R * 2, 0.0f, 360.0f, kColorBlack);
}

PDLL_EXPORT(update, randomize);

int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, uint32_t arg) {
  // must be first!
  PDLL_EVENT(playdate, event, arg);

  if (event == kEventInit) {
    pd = playdate;
    rng ^= pd->system->getCurrentTimeMilliseconds();
    pd->system->logToConsole(pdll ? "[ball.bin] init (via pdll)"
                                  : "[ball.bin] init");

    char *msg = (char *)malloc(32);
    if (msg) {
      strcpy(msg, "[ball.bin] malloc works");
      pd->system->logToConsole("%s", msg);
      free(msg);
    } else {
      pd->system->logToConsole("[ball.bin] malloc returned NULL");
    }
  } else if (event == kEventTerminate) {
    pd->system->logToConsole("[ball.bin] terminate");
  }
  return 0;
}
