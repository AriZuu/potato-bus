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

#ifndef _POTATO_BUS_JSON_H
#define _POTATO_BUS_JSON_H

#define MAX_DEPTH 4

typedef enum _jsonToken {

  JsonObject,
  JsonArray,
  JsonKey,
  JsonString,
  JsonNumber,
  JsonNull,
  
} JsonToken;

typedef struct _jsonNode {

  JsonToken token;
  char*     base;     // pointer to start of current node in json buf
  size_t    len;      // length of current token
  char*     pos;      // parser position inside token
  char*     key;      // pointer to "key" in "key":value -pair
  size_t    keyLen;   // length of "key"
  int       values;   // number of values in current node

  struct _jsonContext*   context;

} JsonNode;

typedef struct _jsonContext  {

  bool     error;
  JsonNode nodes[MAX_DEPTH]; 

  union {

    // parser fields
    struct {
    
      char*       keyEnd;
      char*       valueEnd;
      char        restoreKey;
      char        restoreValue;
    };

    // generator fields
    struct {
    
      char*       pos;
      int         left;
      int         depth;
    };
  };

} JsonContext;

JsonNode* jsonParse(JsonContext* ctx, char* json);
void jsonReset(JsonNode* node);
JsonNode* jsonNext(JsonNode* parent);
JsonNode* jsonFind(JsonNode* parent, const char* key);
bool jsonIsObject(JsonNode* node);
bool jsonIsArray(JsonNode* node);
bool jsonIsNull(JsonNode* node);
bool jsonIsString(JsonNode* node);
bool jsonIsNumber(JsonNode* node);
const char* jsonReadKey(JsonNode* node);
const char* jsonReadString(JsonNode* node);
int jsonReadInteger(JsonNode* node);
double jsonReadDouble(JsonNode* node);

JsonNode* jsonGenerate(JsonContext* ctx, char* outputBuf, int outputBufSizeof);
void jsonGenerateFlush(JsonNode* node);
JsonNode* jsonStartObject(JsonNode* node);
JsonNode* jsonStartArray(JsonNode* node);
void jsonWriteKey(JsonNode* node, const char* key);
void jsonWriteNull(JsonNode* node);
void jsonWriteInteger(JsonNode* node, int value);
void jsonWriteDouble(JsonNode* node, double value);
void jsonWriteString(JsonNode* node, const char* value);

bool jsonFailed(JsonContext* ctx);
#endif /* _POTATO_BUS_JSON_H */
