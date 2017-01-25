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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#ifdef USE_UNIX_SOCKETS

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#else

#include <picoos.h>
#include <picoos-lwip.h>

#endif

#include "potato-bus.h"

#define MAX_PACKET_ID 65535 
int pbGetPacketId(PbClient *c)
{
  c->packetId++;
  if (c->packetId == MAX_PACKET_ID)
    c->packetId = 1;

  return c->packetId;
}

int pbWritePacket(PbClient* client, PbPacket* pkt)
{
  if (pkt->overflow)
    return PB_TOOBIG;

  int len = pbLength(pkt);
  if (client->writePacket(client, pkt->start, len) != len) {

    close(client->sock);
    client->sock = -1;
    return PB_NETWORK;
  }

  return len;
}

int pbReadPacket(PbClient* client)
{
  uint8_t* ptr = client->packet.buf;

  client->packet.start    = client->packet.buf;
  client->packet.overflow = false;

// Read header

  if (client->readPacket(client, ptr, 1) != 1) {
 
    return PB_TIMEOUT;
  }

  ++ptr;
  int multiplier = 1;
  int len = 0;
  uint8_t b;

  do {

    if (client->readPacket(client, ptr, 1) != 1) {

      return PB_NETWORK;
    }

    b = *ptr;
    ++ptr;

    len += (b & 0x7f) * multiplier;
    multiplier *= 128;
    if (multiplier > 128 * 128 * 128)
      return PB_ERROR;

  } while (b & 0x80);

  if (len) {

    // reserve 1 extra byte at end of message, so
    // it is possible to tack null character at
    // end of payload - just to be C-string friendly.
    if (ptr + len + 1 - client->packet.buf > PB_BUFSIZE) {

      close(client->sock);
      client->sock = -1;
      return PB_TOOBIG;
    }

    int got;

    while ((got = client->readPacket(client, ptr, len)) > 0) {
    
      if (got == -1)
        return PB_NETWORK;

      ptr += got;
      len -= got;
      if (len == 0)
        break;
    }
  }

  int type;

  client->packet.ptr = client->packet.start;
  client->packet.end = ptr;

  // Get packet type from header.
  type = pbReadHeader(&client->packet, NULL);
  
  // Put pointer back to packet start.
  client->packet.ptr = client->packet.start;
  return type;
}

static int writePlainPacket(PbClient* client, const unsigned char* buf, size_t len)
{
  return write(client->sock, buf, len);
}

static int readPlainPacket(PbClient* client, unsigned char* buf, size_t len)
{
  return read(client->sock, buf, len);
}

static int closePlainConnection(PbClient* client)
{
  return close(client->sock);
}

int pbConnect(PbClient* client, const char* host, const char* service, PbConnect* arg)
{
  struct addrinfo hints;
  struct addrinfo *res, *resOrig;
  int    i;

  memset(&hints, '\0', sizeof(hints));

  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  i = getaddrinfo(host, service, &hints, &res);
  if (i != 0) {

    return PB_NETWORK;
  }

  resOrig = res;
  client->sock = -1;

  while (res) {

    client->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client->sock >= 0) {

      if (connect(client->sock, res->ai_addr, res->ai_addrlen) == 0)
        break;

      close(client->sock);
      client->sock = -1;
    }

    res = res->ai_next;
  }

  freeaddrinfo(resOrig);
 
  if (client->sock == -1)
    return PB_NETWORK;

  if (arg->keepAlive) {

    struct timeval tmo;

    tmo.tv_sec = arg->keepAlive / 2;
    if (tmo.tv_sec == 0)
      tmo.tv_usec = 500000;
    else
      tmo.tv_usec = 0;

    setsockopt(client->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmo, sizeof(struct timeval));
  }

  client->writePacket = writePlainPacket;
  client->readPacket  = readPlainPacket;
  client->closeConnection = closePlainConnection;

  int st;

  st = pbWriteConnect(&client->packet, arg);
  if (st < 0)
    return st;

  st = pbWritePacket(client, &client->packet);
  if (st < 0)
    return st;

  pbWaitResponse(client, PB_MQ_CONNACK);
  return PB_SUCCESS;
}

#if POTATO_TLS

static int sslWrite(void* ctx, const unsigned char* buf, size_t len)
{
  PbClient* client = (PbClient*)ctx;

  return write(client->sock, buf, len);
}

static int sslRead(void* ctx, unsigned char*buf, size_t len)
{
  PbClient* client = (PbClient*)ctx;

  return read(client->sock, buf, len);
}

static int writeSslPacket(PbClient* client, const unsigned char* buf, size_t len)
{
  return mbedtls_ssl_write(&client->ssl, buf, len);
}

static int readSslPacket(PbClient* client, unsigned char* buf, size_t len)
{
  return mbedtls_ssl_read(&client->ssl, buf, len);
}

static int closeSslConnection(PbClient* client)
{
  mbedtls_ssl_free(&client->ssl);
  return close(client->sock);
}

int pbConnectSSL(PbClient*            client,
                 const char*          host,
                 const char*          service,
                 PbConnect*           arg)
{
  struct addrinfo hints;
  struct addrinfo *res, *resOrig;
  int    i;
  int    st;

  memset(&hints, '\0', sizeof(hints));

  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  i = getaddrinfo(host, service, &hints, &res);
  if (i != 0) {

    return PB_NETWORK;
  }

  resOrig = res;
  client->sock = -1;

  while (res) {

    client->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client->sock >= 0) {

      if (connect(client->sock, res->ai_addr, res->ai_addrlen) == 0)
        break;

      close(client->sock);
      client->sock = -1;
    }

    res = res->ai_next;
  }

  freeaddrinfo(resOrig);
 
  if (client->sock == -1)
    return PB_NETWORK;

  if (arg->keepAlive) {

    struct timeval tmo;

    tmo.tv_sec = arg->keepAlive / 2;
    if (tmo.tv_sec == 0)
      tmo.tv_usec = 500000;
    else
      tmo.tv_usec = 0;

    setsockopt(client->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmo, sizeof(struct timeval));
  }

  mbedtls_ssl_init(&client->ssl);
  st = mbedtls_ssl_set_hostname(&client->ssl, host);
  if (st != 0)
    return PB_MBEDTLS;

  st = mbedtls_ssl_setup(&client->ssl, arg->sslConf);
  if (st != 0)
    return PB_MBEDTLS;

  mbedtls_ssl_set_bio(&client->ssl, client, sslWrite, sslRead, NULL);

  client->writePacket = writeSslPacket;
  client->readPacket  = readSslPacket;
  client->closeConnection = closeSslConnection;

  st = pbWriteConnect(&client->packet, arg);
  if (st < 0)
    return st;

  st = pbWritePacket(client, &client->packet);
  if (st < 0)
    return st;

  pbWaitResponse(client, PB_MQ_CONNACK);
  return PB_SUCCESS;
}

#endif


int pbConnectURL(PbClient*            client,
                 const char*          url,
                 PbConnect*           arg)
{
  char* ptr;
  const char* start;
  bool  ssl;
  const char* port;
  char  host[64];
  size_t   max;

  ptr = strchr(url, ':');
  if (ptr == NULL)
    return PB_BADURL;

  if (!strncmp(url, "mqtt", ptr - url) || !strncmp(url, "tcp", ptr - url))
    ssl = false;
  else if (!strncmp(url, "mqtts", ptr - url) || !strncmp(url, "ssl", ptr - url))
    ssl = true;
  else
    return PB_BADURL;

  start = ptr + 1;
  if (strncmp(start, "//", 2))
    return PB_BADURL;

  start += 2;
  ptr = strchr(start, ':');
  if (ptr == NULL) {

    strlcpy(host, start, sizeof(host));
    port = ssl ? "8883" : "1883";
  }
  else {

    max = ptr - start;
    if (max > sizeof(host))
      max = sizeof(host);

    strlcpy(host, start, max);
    port = ptr + 1;
  }

  if (ssl)
    return pbConnectSSL(client, host, port, arg);

  return pbConnect(client, host, port, arg);
}

int pbDisconnect(PbClient* client)
{
  int st;

  st = pbWriteDisconnect(&client->packet);
  if (st < 0)
    return st;

  st = pbWritePacket(client, &client->packet);
  if (st < 0)
    return st;

  client->closeConnection(client);
  client->sock = -1;
  return PB_SUCCESS;
}

int pbPing(PbClient* client)
{
  int st;

  st = pbWritePing(&client->packet);
  if (st < 0)
    return st;

  st = pbWritePacket(client, &client->packet);
  if (st < 0)
    return st;

  pbWaitResponse(client, PB_MQ_PINGRESP);
  return PB_SUCCESS;
}

int pbPublish(PbClient* client, PbPublish* arg)
{
  int st;

  arg->packetId = pbGetPacketId(client);
  st = pbWritePublish(&client->packet, arg);
  if (st < 0)
    return st;

  st = pbWritePacket(client, &client->packet);
  if (st < 0)
    return st;

  return PB_SUCCESS;
}

int pbSubscribe(PbClient* client, PbSubscribe* arg)
{
  int st;

  arg->packetId = pbGetPacketId(client);
  st = pbWriteSubscribe(&client->packet, arg);
  if (st < 0)
    return st;

  st = pbWritePacket(client, &client->packet);
  if (st < 0)
    return st;

  pbWaitResponse(client, PB_MQ_SUBACK);
  return PB_SUCCESS;
}

int pbEvent(PbClient* client)
{
  if (client->sock == -1)
    return PB_NETWORK;

  return pbReadPacket(client);
}

int pbWaitResponse(PbClient* client, int expect)
{
  int type;

  do {

    type = pbEvent(client);
    if (type == PB_TIMEOUT) {

      type = pbPing(client);
      if (type < 0)
        break;
    }

  } while (type != expect && type >= 0);

  return type;
}


