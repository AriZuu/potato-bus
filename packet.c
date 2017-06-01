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

#define CHECK_SPACE(pkt,n) do { if (pkt->overflow || (pkt->end + n - pkt->buf > POTATO_BUFSIZE)) { pkt->overflow = true; return -1; } } while (0)

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

const char* pbToString(PbPacket* pkt)
{
  switch (pkt->start[0] >> 4) {
    case PB_MQ_CONNECT:
      return "CONNECT";

    case PB_MQ_CONNACK:
      return "CONNACK";

    case PB_MQ_PUBLISH:
      return "PUBLISH";

    case PB_MQ_PUBACK:
      return "PUBACK";

    case PB_MQ_PUBREC:
      return "PUBREC";

    case PB_MQ_PUBREL:
      return "PUBREL";

    case PB_MQ_PUBCOMP:
      return "PUBCOMP";

    case PB_MQ_SUBSCRIBE:
      return "SUBSCRIBE";

    case PB_MQ_SUBACK:
      return "SUBACK";

    case PB_MQ_UNSUBSCRIBE:
      return "UNSUBSCRIBE";

    case PB_MQ_UNSUBACK:
      return "UNSUBACK";

    case PB_MQ_PINGREQ:
      return "PINGREQ";

    case PB_MQ_PINGRESP:
      return "PINGRESP";

    case PB_MQ_DISCONNECT:
      return "DISCONNECT";

    default:
      return "<bad packet type>";
  }

}

int pbWriteByte(PbPacket* pkt, uint8_t b)
{
  CHECK_SPACE(pkt, 1);

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

int pbWriteInt(PbPacket* pkt, int val)
{
  CHECK_SPACE(pkt, 2);

  *(pkt->end++) = val >> 8;
  *(pkt->end++) = val & 0xff;
  return 2;
}

int pbReadInt(PbPacket* pkt)
{
  int i;

  i = (pkt->ptr[0] << 8) | pkt->ptr[1];
  pkt->ptr += 2;
 
  return i;
}

int pbWriteString(PbPacket* pkt, const char* str)
{
  if (str == NULL)
    return 0;

  int len = strlen(str);

  pbWriteInt(pkt, len);

  CHECK_SPACE(pkt, len);
  memcpy(pkt->end, str, len);

  pkt->end += len;
  return len + 2;
}

const char* pbReadString(PbPacket* pkt)
{
  int len = pbReadInt(pkt);
  uint8_t* str = pkt->ptr - 2;

  // Move string down so that we get space for null character at end.
  // This allows returning pointer to string without allocating additional storage.
  memmove(str, pkt->ptr, len);
  str[len] = '\0';

  pkt->ptr += len;
  return (const char*)str;
}

int pbReadHeader(PbPacket* pkt, int* flags)
{
  int b;

  b = pbReadByte(pkt);

  if (flags)
    *flags = b & 0xf;

  int lenByte;

  do {
  
    lenByte = pbReadByte(pkt);
  } while (lenByte & 0x80);

  return b >> 4;
}

void pbWriteHeader(PbPacket* pkt, int packetType, int packetFlags, int len)
{
  if (pkt->overflow)
    return;

  int lenBytes = 0;

  pkt->start--;
  uint8_t* ptr = pkt->start;

  do {

    if (lenBytes)
      memmove(pkt->start, pkt->start + 1, lenBytes);

    ++lenBytes;
    *ptr = len % 128;
    len  = len / 128;
    if (len > 0)
      *ptr |= 0x80;

    if (lenBytes > 4)
      break; // error

    pkt->start--;

  } while (len > 0);

  *(pkt->start) = (packetType << 4) | packetFlags;
}

int pbWritePublish(PbPacket* pkt, PbPublish* args)
{
  pbInitPacket(pkt);

// Write variable header.

  pbWriteString(pkt, args->topic);

// only if QOS > 0: pbWriteInt(pkt, args->packetId);

// Write payload.

  CHECK_SPACE(pkt, args->len);
  memcpy(pkt->end, args->message, args->len); // message data
  pkt->end += args->len;

// Write header.

  pbWriteHeader(pkt, PB_MQ_PUBLISH, 0, pbLength(pkt));

  if (pkt->overflow)
    return -1;

  return 0;
}

void pbReadPublish(PbPacket* pkt, PbPublish* pub)
{
  uint8_t* buf;
 
  buf = pub->message;
  memset(pub, '\0', sizeof(PbPublish));
  pub->message = buf;

  int flags;

  pbReadHeader(pkt, &flags);

  pub->topic    = pbReadString(pkt);
  if ((flags >> 1) & 3) // packet id is there only if QOS > 0
    pub->packetId = pbReadInt(pkt);

  pub->len      = pkt->end - pkt->ptr;
  pub->message  = pkt->ptr;
}

int pbWriteSubscribe(PbPacket* pkt, PbSubscribe* args)
{
  pbInitPacket(pkt);

// Write variable header.

  pbWriteInt(pkt, args->packetId);

// Write payload.

  pbWriteString(pkt, args->topic); // topic filter
  pbWriteByte(pkt, 0); // QOS

// Write header.

  pbWriteHeader(pkt, PB_MQ_SUBSCRIBE, 2, pbLength(pkt));

  if (pkt->overflow)
    return -1;

  return 0;
}

void pbReadSubAck(PbPacket* pkt, PbSubAck* ack)
{
  memset(ack, '\0', sizeof(PbSubAck));

  pbReadHeader(pkt, NULL);

  ack->packetId     = pbReadInt(pkt);
  ack->returnCode   = pbReadByte(pkt);
}

int pbWriteConnect(PbPacket* pkt, PbConnect* args)
{
  pbInitPacket(pkt);

// Write variable header.

  pbWriteString(pkt, "MQTT");
  pbWriteByte(pkt, 0x4);
  pbWriteByte(pkt, 0x2); // clean session flag
  pbWriteInt(pkt, args->keepAlive); // keepalive

// Write payload.

  pbWriteString(pkt, args->clientId);
  pbWriteString(pkt, NULL); // will topic
  pbWriteString(pkt, NULL); // will message
  pbWriteString(pkt, args->user); // username
  pbWriteString(pkt, args->pass); // password

// Write header.

  pbWriteHeader(pkt, PB_MQ_CONNECT, 0, pbLength(pkt));

  if (pkt->overflow)
    return -1;

  return 0;
}

void pbReadConnectAck(PbPacket* pkt, PbConnectAck* ack)
{
  memset(ack, '\0', sizeof(PbConnectAck));

  pbReadHeader(pkt, NULL);

  ack->sessionPresent = pbReadByte(pkt) & 1;
  ack->returnCode     = pbReadByte(pkt);
}

int pbWritePing(PbPacket* pkt)
{
  pbInitPacket(pkt);
  pbWriteHeader(pkt, PB_MQ_PINGREQ, 0, pbLength(pkt));

  if (pkt->overflow)
    return -1;

  return 0;
}

int pbWriteDisconnect(PbPacket* pkt)
{
  pbInitPacket(pkt);
  pbWriteHeader(pkt, PB_MQ_DISCONNECT, 0, pbLength(pkt));

  if (pkt->overflow)
    return -1;

  return 0;
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


