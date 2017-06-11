/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include "gfx.h"
#include "gfxconf.h"

#if (GFX_USE_GINPUT && GINPUT_NEED_TOGGLE)
#include "../../ugfx/src/ginput/ginput_driver_toggle.h"

#include <badge_input.h>
#include <stdio.h>

#include "ginput_lld_toggle_config.h"

GINPUT_TOGGLE_DECLARE_STRUCTURE();
static GListener _pl;
static uint32_t bits_set;
static uint8_t button_lookup[BADGE_BUTTONS] = {
                                                GKEY_UP,
                                                GKEY_DOWN,
                                                GKEY_LEFT,
                                                GKEY_RIGHT,
                                                GKEY_ENTER,
                                                'a',
                                                's',
                                                'z',
                                                'x',
                                                GKEY_DEL,
                                               };


void keyboard_callback(void *param, GEvent *pe){
  if(pe){
    GEventKeyboard *ke = (GEventKeyboard *) pe;
    bits_set = 0;
    uint8_t button = ke->c[0];
    uint8_t state = ke->c[0];
    for(uint8_t i = 0; i < BADGE_BUTTONS; i++){
      if(button == button_lookup[i]){
        if(state == GKEYSTATE_KEYUP){
          bits_set &= ~(1<<i);
        } else {
          bits_set |= (1<<i);
        }
      }
    }
    ginputToggleWakeupI();
  }
}

void
ginput_lld_toggle_init(const GToggleConfig *ptc)
{
  geventListenerInit(&_pl);
  geventAttachSource(&_pl, ginputGetKeyboard(0), GLISTEN_KEYUP);
  _pl.callback = keyboard_callback;
}

unsigned
ginput_lld_toggle_getbits(const GToggleConfig *ptc)
{
    return bits_set;
}

#endif /*  GFX_USE_GINPUT && GINPUT_NEED_TOGGLE */

