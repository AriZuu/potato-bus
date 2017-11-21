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
  int st;

  st = mbedtls_ssl_write(&client->ssl, buf, len);
  if (st < 0)
    client->sslResult = st;

  return st;
}

static int readSslPacket(PbClient* client, unsigned char* buf, size_t len)
{
  int st;

  st =  mbedtls_ssl_read(&client->ssl, buf, len);
  if (st < 0)
    client->sslResult = st;

  return st;
}

static int closeSslConnection(PbClient* client)
{
  mbedtls_ssl_free(&client->ssl);
  return close(client->sock);
}

#endif

bool pbIsSSL_URL(const char* url)
{
  char* ptr;
  ptr = strchr(url, ':');
  if (ptr == NULL)
    return false;

  if (!strncmp(url, "mqtt", ptr - url) || !strncmp(url, "tcp", ptr - url))
    return false;

  if (!strncmp(url, "mqtts", ptr - url) || !strncmp(url, "ssl", ptr - url))
    return true;

  return false;
}

int pbConnectSocket(PbClient*            client,
                    const PbUrl*         url,
                    mbedtls_ssl_config*  sslConf)
{
  struct addrinfo hints;
  struct addrinfo *res, *resOrig;
  int    i;

#if POTATO_TLS
  int    st;

  client->sslResult = 0;
#endif

  memset(&hints, '\0', sizeof(hints));

  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  i = getaddrinfo(url->host, url->port, &hints, &res);
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

#if POTATO_TLS
  if (sslConf != NULL) {

    mbedtls_ssl_init(&client->ssl);
    st = mbedtls_ssl_set_hostname(&client->ssl, url->host);
    if (st != 0) {

      close(client->sock);
      client->sslResult = st;
      return PB_MBEDTLS;
    }

    st = mbedtls_ssl_setup(&client->ssl, sslConf);
    if (st != 0) {

      close(client->sock);
      client->sslResult = st;
      return PB_MBEDTLS;
    }

    mbedtls_ssl_set_bio(&client->ssl, client, sslWrite, sslRead, NULL);

    client->writePacket = writeSslPacket;
    client->readPacket  = readSslPacket;
    client->closeConnection = closeSslConnection;
  }
  else {

#endif

    client->writePacket = writePlainPacket;
    client->readPacket  = readPlainPacket;
    client->closeConnection = closePlainConnection;

#if POTATO_TLS
  }
#endif

  return PB_SUCCESS;
}

int pbDisconnectSocket(PbClient* client)
{
  client->closeConnection(client);
  client->sock = -1;
  return PB_SUCCESS;
}

int pbUrlTok(PbUrl* url, char* urlString)
{
  char* ptr;
  char* start;

  memset(url, '\0', sizeof (PbUrl));

  // First extract protocol.
  ptr = strstr(urlString, ":");
  if (ptr != NULL) {

    *ptr = '\0';
    url->protocol = urlString;
    start = ptr + 1;
  }
  else {

    url->protocol = "http";
    start = urlString;
  }

  if (strncmp(start, "//", 2))
    return -1;

  start = start + 2;

  // Check for username/password
  ptr = strchr(start, '@');
  if (ptr != NULL) {

    *ptr = '\0';
    char* separator;

    separator = strchr(start, ':');
    if (separator) {

      *separator = '\0';
      url->username = start;
      url->password = separator + 1;
    }
    else {

      url->username = start;
    }

    start = ptr + 1;
  }

  // Extract host and remaining path (relative)
  url->host = start;
  ptr = strchr(start, '/');
  if (ptr) {

    *ptr = '\0';
    url->path = ptr + 1;
  }
  else
    url->path = "";
  
  // Check port in host
  ptr = strchr(url->host, ':');
  if (ptr != NULL) {

    *ptr = '\0';
    url->port = ptr + 1;
  }

  return 0;
}
