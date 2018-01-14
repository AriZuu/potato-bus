/*
 * Copyright (c) 2017, Ari Suutari <ari@stonepile.fi>.
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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef USE_UNIX_SOCKETS

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#else

#include <picoos.h>
#include <picoos-lwip.h>

#endif

#include "potato-bus.h"

int pbGet(PbClient*            client,
          const char*          url,
          mbedtls_ssl_config*  sslConf)
{
  bool  ssl;
  PbUrl urlParts;
  int   st;
  PbPacket* pkt = &client->packet;

  pbInitPacket(pkt);
  if (!pbHasRoom(pkt, strlen(url)))
    return PB_BADURL;

  strcpy((char*)pkt->start, url);
  if (pbUrlTok(&urlParts, (char*)pkt->start) == -1)
    return PB_BADURL;

  if (!strcmp(urlParts.protocol, "http")) {

    ssl = false;
    if (urlParts.port == NULL)
      urlParts.port = "80";

    st = pbConnectSocket(client, &urlParts, NULL);
    if (st != PB_SUCCESS)
      return st;
  }
#if POTATO_TLS
  else if (!strcmp(urlParts.protocol, "https")) {

    ssl = true;
    if (urlParts.port == NULL)
      urlParts.port = "443";

    st = pbConnectSocket(client, &urlParts, sslConf);
    if (st != PB_SUCCESS)
      return st;
  }
#endif
  else
    return PB_BADURL;

  struct timeval tmo;

  tmo.tv_sec = 60;
  tmo.tv_usec = 0;

  setsockopt(client->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmo, sizeof(struct timeval));

  if (client->writePacket(client, (uint8_t*)"GET /", 5) == -1 ||
      client->writePacket(client, (uint8_t*)urlParts.path, strlen(urlParts.path)) == -1 ||
      client->writePacket(client, (uint8_t*)" HTTP/1.0\r\n\r\n", 13) == -1) {

    pbDisconnectSocket(client);
    return PB_NETWORK;
  }

  unsigned char c;
  int contentLength = POTATO_BUFSIZE - 1;
  bool first = true;

  st = PB_SUCCESS;

  // Loop through headers.
  do {

    // Collect header.

    pbInitPacket(pkt);
    while (client->readPacket(client, &c, 1) == 1) {

      if (c == '\r') {

        // Consume \n
        client->readPacket(client, &c, 1);
        break;
      }

      pbWriteByte(pkt, c);
    }

    pbWriteByte(pkt, '\0');

    if (!pkt->overflow && pbLength(pkt) > 1) {
   
      char* hdr = (char*)pkt->start;
      char* ptr;
      int   httpCode;

      if (first) {

        first = false;
        ptr = strchr(hdr, ' ');
        if (ptr == NULL) {

          st = PB_HTTP;
          break;
        }

        *ptr = '\0';
        if (strcmp(hdr, "HTTP/1.1") && strcmp(hdr, "HTTP/1.0")) {

          st = PB_HTTP;
          break;
        }

        ++ptr;
        while (*ptr == ' ')
          ++ptr;

        httpCode = strtol(ptr, NULL, 10);
        if (httpCode != 200) {

          st = -httpCode;
          break;
        }
      }
      else {

        ptr = strchr(hdr, ':');
        if (ptr != NULL) {
  
          *ptr = '\0';
           if (!strcasecmp(hdr, "Content-Length")) {

             ++ptr;
             while (*ptr == ' ')
               ++ptr;

             contentLength = strtol(ptr, NULL, 10);
             if (contentLength > POTATO_BUFSIZE - 1)
               contentLength = POTATO_BUFSIZE - 1;
           }
        }
      }
    }

  } while (pbLength(pkt) > 1);

  pbInitPacket(pkt);

  if (st == PB_SUCCESS) {

    int len;

    pbInitPacket(pkt);
    while (contentLength > 0 && (len = client->readPacket(client, pkt->end, pbRoomLeft(pkt) - 1)) > 0) {

      pkt->end += len;
      contentLength -= len;
      if (pbRoomLeft(pkt) <= 1)
        break;
    }

    st = pbLength(pkt);
  }

  pbDisconnectSocket(client);
  return st;
}

