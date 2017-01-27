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

#ifndef _POTATO_BUS_H
#define _POTATO_BUS_H

#include "potato-cfg.h"

#if POTATO_TLS

#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"

#endif

/**
 * @file    potato-bus.h
 * @brief   Include file for Potato Bus MQTT client
 * @author  Ari Suutari <ari@stonepile.fi>
 */

/**
 * @mainpage potato-bus - Simple MQTT client for pico]OS
 * <b> Table Of Contents </b>
 * - @ref client
 * - @ref packet
 * - @ref json
 * @section overview Overview
 * This library contains a simple MQTT client implementation for pico]OS, but
 * parts of it might be useful in other context also. 
 *
 * Library is split into two layers. Lower one is a packet
 * reader/writer without dependencies to Pico]OS, so it should
 * be usable for any envrionment. Higher layer is actual
 * client, which uses BSD socket layer provided by picoos-lwip 
 * library.
 * 
 * For MQTT specification, see http://mqtt.org/documentation
 */

/** @defgroup client   Client API layer */
/** @defgroup packet   Packet reader/writer API */

/*
 * MQTT packet types.
 */
#define PB_MQ_RESERVED0   0
#define PB_MQ_CONNECT     1
#define PB_MQ_CONNACK     2
#define PB_MQ_PUBLISH     3
#define PB_MQ_PUBACK      4
#define PB_MQ_PUBREC      5
#define PB_MQ_PUBREL      6
#define PB_MQ_PUBCOMP     7
#define PB_MQ_SUBSCRIBE   8
#define PB_MQ_SUBACK      9
#define PB_MQ_UNSUBSCRIBE 10
#define PB_MQ_UNSUBACK    11
#define PB_MQ_PINGREQ     12
#define PB_MQ_PINGRESP    13
#define PB_MQ_DISCONNECT  14
#define PB_MQ_RESERVED15  15

/**
 * Return codes.
 */

#define PB_BADURL  -6
#define PB_MBEDTLS -5
#define PB_TIMEOUT -4
#define PB_NETWORK -3
#define PB_TOOBIG  -2
#define PB_ERROR   -1
#define PB_SUCCESS 0

/*
 * Max space used by fixed header (length takes 1 to 4 bytes).
 */
#define PB_MAX_HEADER  5

/** 
 * Max size of MQ packet buffer.
 */
#define PB_BUFSIZE 400

/**
 * Data for publish packet.
 */
typedef struct {

  int packetId;
  const char* topic;
  unsigned char* message;
  int len;
} PbPublish;
  
/**
 * Data for subscribe packet.
 */
typedef struct {

  int packetId;
  const char* topic;
} PbSubscribe;
  
/**
 * Subscribe response data.
 */
typedef struct {
  
  int packetId;
  int returnCode;
} PbSubAck;

/**
 * Data for new connection.
 */
typedef struct {

  int keepAlive;
  const char* clientId;
  const char* user;
  const char* pass;
#if POTATO_TLS
  mbedtls_ssl_config*  sslConf;
#endif
} PbConnect;
  
/**
 * Connection request ack.
 */
typedef struct {
  
  bool sessionPresent;
  int returnCode;
} PbConnectAck;

/**
 * Packet reader/writer work area.
 */
typedef struct {
  
  unsigned char buf[PB_BUFSIZE];
  unsigned char* start;
  unsigned char* ptr;
  unsigned char* end;
  bool overflow;

} PbPacket;

/**
 * Client handle.
 */
typedef struct pbClient {
  
  int sock;
  int packetId;
  PbPacket packet;

  int (*writePacket)(struct pbClient*, const unsigned char*, size_t);
  int (*readPacket)(struct pbClient*, unsigned char*, size_t);
  int (*closeConnection)(struct pbClient*);

#if POTATO_TLS

  mbedtls_ssl_context      ssl;
  int                      sslResult;

#endif
} PbClient;


/**
 * @ingroup client
 * @{
 */

/**
 * Get next packet ID for this client. Used in some MQTT packets,
 * but not in all.
 */
int pbGetPacketId(PbClient *c);

/**
 * Write given packet to broker socket.
 */
int pbWritePacket(PbClient* client, PbPacket* pkt);

/**
 * Read next packet from broker socket.
 */
int pbReadPacket(PbClient* client);

/**
 * Connect to MQTT broker and wait for it to acknowledge new connection.
 */
int pbConnect(PbClient* client, const char* host, const char* service, PbConnect* arg);

/**
 * Connect to MQTT broker using SSL/TLS and wait for it to acknowledge new connection.
 */
int pbConnectSSL(PbClient*            client,
                 const char*          host,
                 const char*          service,
                 PbConnect*           arg);
/**
 * Connect to MQTT broker using URL and wait for it to acknowledge new connection.
 * URL should be like
 *   mqtt://server[:port],
 *   tcp://server[:port], 
 *   mqtts://server[:port], 
 *   ssl://server[:port] 
 * 
 * mqtt: is alias for tcp: and mqtts: is alias for ssl:.
 * Default port is 1883 for tcp and 8883 for ssl.
 */
int pbConnectURL(PbClient*            client,
                 const char*          url,
                 PbConnect*           arg);
/**
 * Send PING to broker and wait for response. Used to handle keepalive processing.
 */
int pbPing(PbClient* client);

/**
 * Publish new data to given topic.
 */
int pbPublish(PbClient* client, PbPublish* arg);

/**
 * Subscribe new topic.
 */
int pbSubscribe(PbClient* client, PbSubscribe* arg);

/**
 * Disconnect from broker.
 */
int pbDisconnect(PbClient* client);

/**
 * Get next event from broker. This is used by application main loop
 * to find out what to do next. Common returns are information about
 * new publish data, timeout or error.
 */
int pbEvent(PbClient* client);

/**
 * Used internally for waiting desired response from broker.
 * (for example, wait for acknowledgement to ping packet).
 */
int pbWaitResponse(PbClient* client, int expect);

/** @} */

/**
 * @ingroup packet
 * @{
 */

/**
 * Write dump of packet to stdout. Useful for debugging.
 */
void pbDump(PbPacket* pkt);

/**
 * Convert packet type to string.
 */
const char* pbToString(PbPacket* pkt);

/**
 * Write a byte to packet buffer.
 */
int pbWriteByte(PbPacket* pkt, uint8_t b);

/**
 * Read a byte from packet buffer.
 */
uint8_t pbReadByte(PbPacket* pkt);

/**
 * Write integer to packet buffer.
 */
int pbWriteInt(PbPacket* pkt, int val);

/**
 * Read integer from packet buffer.
 */
int pbReadInt(PbPacket* pkt);

/**
 * Write string to packet buffer.
 */
int pbWriteString(PbPacket* pkt, const char* str);

/**
 * Read string from packet buffer. MQTT packets use
 * pascal-style strings (with length field), but this
 * function returns null-terminated C string.
 */
const char* pbReadString(PbPacket* pkt);

/**
 * Read packet header.
 */
int pbReadHeader(PbPacket* pkt, int* flags);

/**
 * Write packet header.
 */
void pbWriteHeader(PbPacket* pkt, int packetType, int packetFlags, int len);

/**
 * Write publish packet.
 */
int pbWritePublish(PbPacket* pkt, PbPublish* args);

/**
 * Read published data.
 */
void pbReadPublish(PbPacket* pkt, PbPublish* pub);

/**
 * Write subscription packet.
 */
int pbWriteSubscribe(PbPacket* pkt, PbSubscribe* args);

/**
 * Read subscription ack.
 */
void pbReadSubAck(PbPacket* pkt, PbSubAck* ack);

/**
 * Write connect packet.
 */
int pbWriteConnect(PbPacket* pkt, PbConnect* args);

/**
 * Read connect ack.
 */
void pbReadConnectAck(PbPacket* pkt, PbConnectAck* ack);

/**
 * Write ping packet.
 */
int pbWritePing(PbPacket* pkt);

/**
 * Write disconnect packet.
 */
int pbWriteDisconnect(PbPacket* pkt);

/**
 * Get length of packet.
 */
int pbLength(PbPacket* pkt);

/**
 * Initialize packet. Must be called before writing/reading.
 */
void pbInitPacket(PbPacket* pkt);

/** @} */

#endif /* _POTATO_BUS_H */
