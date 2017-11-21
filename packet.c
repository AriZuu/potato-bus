/*
 * Copyright (c) 2016, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "potato-bus.h"

void pbDump(PbPacket* pkt)
{
  int len = pbLength(pkt);
  printf("Pkt len %d ", len);

  uint8_t* ptr = pkt->start;
  int i;

  for (i = 0; i < len; i++) {

    printf("0x%X ", *ptr);
    ++ptr;
  }

  printf("\n");
}

int pbRoomLeft(PbPacket* pkt)
{
  return POTATO_BUFSIZE - (pkt->end - pkt->buf);
}

bool pbHasRoom(PbPacket* pkt, int n)
{
  if (pkt->overflow)
    return false;

  if (pkt->end + n - pkt->buf > POTATO_BUFSIZE) {

    pkt->overflow = true;
    return false;
  }

  return true;
}

int pbWriteByte(PbPacket* pkt, uint8_t b)
{
  PB_CHECK_SPACE(pkt, 1);

  *(pkt->end) = b;
  pkt->end++;
  return 1;
}

uint8_t pbReadByte(PbPacket* pkt)
{
  uint8_t b;

  b = *(pkt->ptr);
  pkt->ptr++;

  return b;
}

void pbInitPacket(PbPacket* pkt)
{
  pkt->start    = pkt->buf + PB_MAX_HEADER;
  pkt->ptr      = pkt->start;
  pkt->end      = pkt->start;
  pkt->overflow = false;
}

int pbLength(PbPacket* pkt)
{
  return pkt->end - pkt->start;
}


