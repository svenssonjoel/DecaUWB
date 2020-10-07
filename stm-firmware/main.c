/*
    Copyright 2019 Joel Svensson	svenssonjoel@yahoo.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// File also contains code distributed as part of Chibios under license below.

/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
 */

#include "ctype.h"
#include "string.h"

#include "ch.h"
#include "hal.h"
#include "chvt.h"
#include "chtime.h"

#include "usbcfg.h"
#include "chprintf.h"

static SerialConfig serial_cfg = {
  115200,
  0,
  USART_CR2_STOP1_BITS,
  0
};


BaseSequentialStream *chp = NULL;

int inputline(BaseSequentialStream *chp, char *buffer, int size) {
  int n = 0;
  unsigned char c;
  for (n = 0; n < size - 1; n++) {

    c = streamGet(chp);
    switch (c) {
    case 127: /* fall through to below */
    case '\b': /* backspace character received */
      if (n > 0)
        n--;
      buffer[n] = 0;
      streamPut(chp,0x8); /* output backspace character */
      streamPut(chp,' ');
      streamPut(chp,0x8);
      n--; /* set up next iteration to deal with preceding char location */
      break;
    case '\n': /* fall through to \r */
    case '\r':
      buffer[n] = 0;
      return n;
    default:
      if (isprint(c)) { /* ignore non-printable characters */
        streamPut(chp,c);
        buffer[n] = c;
      } else {
        n -= 1;
      }
      break;
    }
  }
  buffer[size - 1] = 0;
  return 0; // Filled up buffer without reading a linebreak
}

static THD_WORKING_AREA(tx_thread_area, 2048);
static THD_FUNCTION(tx_thread, arg) {
  (void) arg;

  unsigned char c; 
  while (1) {
    
    while ((c = streamGet(chp))) {
	streamPut(chp, c);
	sdWrite(&SD4, &c, 1);
      }
     chThdSleepMilliseconds(100);
  }
}


static THD_WORKING_AREA(rx_thread_area, 2048);

static THD_FUNCTION(rx_thread, arg) {
  (void) arg;

  while(1) {

    uint8_t c; 
    if (sdReadTimeout(&SD4, &c, 1, 100)) {
      chprintf(chp,"%c", c);
    }
  }
}

void serial_write(char *data) {

  sdWrite(&SD4, (uint8_t*) data, strlen(data));
  chprintf(chp, "%s", data);
}

int main(void) {
  halInit();
  chSysInit();

  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);	

  chp = (BaseSequentialStream*)&SDU1;

  /* Serial */
  palSetPadMode(GPIOC, 10, PAL_MODE_ALTERNATE(8) | PAL_STM32_OSPEED_HIGHEST |
                           PAL_STM32_OTYPE_PUSHPULL);
  palSetPadMode(GPIOC, 11, PAL_MODE_ALTERNATE(8) | PAL_STM32_OSPEED_HIGHEST |
                           PAL_STM32_OTYPE_PUSHPULL);
  sdStart(&SD4, &serial_cfg);
  
  (void)chThdCreateStatic(rx_thread_area,
  			  sizeof(rx_thread_area),
  			  NORMALPRIO,
  			  rx_thread, NULL);

  (void)chThdCreateStatic(tx_thread_area,
  			  sizeof(tx_thread_area),
  			  NORMALPRIO,
			  tx_thread, NULL);

  chThdSleepMilliseconds(10000);
  chprintf(chp, "Sending rr\r\n");
  serial_write("\r");
  chThdSleepMilliseconds(100);
  serial_write("\r");
   
  while(true) {
    
    chThdSleepMilliseconds(1000);
  }

}
