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

#include <picoos.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "potato-bus.h"

void potatoStart(void);

static PbClient client;
static bool sendData = false;

static void potatoTask(void* arg)
{
  while (true) {

//
// Connect to MQTT broker.
//
    PbConnect cd = {};
    cd.clientId = "test";
    cd.keepAlive= 60;
  
    if (pbConnect(&client, "mqtt.server.example.com", "1883", &cd) < 0) {

      printf("potato: connect failed.\n");
      posTaskSleep(MS(10000));
      continue;
    }

    printf("potato: connected.\n");
    PbPublish pub = {};
  
//
// Subscribe some topics.
//
    PbSubscribe sub = {};
    sub.topic = "sensors/storage";
    pbSubscribe(&client, &sub);

    sub.topic = "sensors/power-meter";
    pbSubscribe(&client, &sub);

    int type;
  
    while((type = pbEvent(&client))) {

      if (type == PB_TIMEOUT) {

//
// Timeout means that application should send a keepalive ping.
// This is also a good place to publish data to mqtt broker.
//

        if (sendData) {

          pub.message = (uint8_t*)"publish example data";
          pub.len = strlen((char*)pub.message);
          pub.topic = "example";
          if (pbPublish(&client, &pub) < 0)
            break;
        }
        else
          if (pbPing(&client) < 0)
            break;

        continue;
      }

//
// If packet is too big for our buffer, break out from the loop,
// disconnect and connect again.
//
      if (type == PB_TOOBIG) {
   
        printf ("potato: too big packet\n");
        break;
      }
  
      if (type < 0)
        break;
  
//
// If we got a published message from broker,
// show information about it.
//
      if (type == PB_MQ_PUBLISH) {
  
        pbReadPublish(&client.packet, &pub);
  
        printf("Topic: %s len %d \n", pub.topic, pub.len);
        pub.message[pub.len] = '\0';
        printf("msg: %s\n", pub.message);
      }
    }
  
//
// Disconnect, wait for a while and try to connect again.
//
    pbDisconnect(&client);
    printf("potato: disconnected.\n");
    posTaskSleep(MS(10000));
  }
}

void potatoStart()
{
  nosTaskCreate(potatoTask, NULL, 2, 4000, "PotatoBus");
}
