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

/** @defgroup json potato-bus JSON API
 * Potato-bus includes a simple mallocless JSON parser & generator.
 * @{
 */

#define MAX_DEPTH 4

/**
 * JSON token types.
 */
typedef enum jsonToken {

  JsonObject,
  JsonArray,
  JsonKey,
  JsonString,
  JsonNumber,
  JsonNull,
  
} JsonToken;

/**
 * Single node (object,  array or simple key / value) in json data. 
 */
typedef struct jsonNode {

  JsonToken token;
  char*     base;     // pointer to start of current node in json buf
  size_t    len;      // length of current token
  char*     pos;      // parser position inside token
  char*     key;      // pointer to "key" in "key":value -pair
  size_t    keyLen;   // length of "key"
  int       values;   // number of values in current node

  struct jsonContext*   context;

} JsonNode;

/**
 * JSON processing context. Contains storage
 * for parser node stack and parser / generator
 * state information.
 */
typedef struct jsonContext  {

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

/**
 * Start json parsing. Function returns pointer
 * to root token, which can be used as argument
 * for other parser calls.
 * @param ctx    Pointer to JsonContext object.
 * @param json   Character string containing json to be parsed.
 *               Note that parser modifies the string when it
 *               temporarily null-terminates values.
 * 
 * @sa jsonNext, jsonReset, jsonFind
 */
JsonNode* jsonParse(JsonContext* ctx, char* json);

/**
 * Get next json node from input string.
 * 
 * @sa jsonReset
 */
JsonNode* jsonNext(JsonNode* parent);

/**
 * Reset json node iterator back to beginning.
 *
 * @sa jsonNext
 */
void jsonReset(JsonNode* node);

/**
 * Find json node with given attribute name.
 */
JsonNode* jsonFind(JsonNode* parent, const char* key);

/**
 * Check if current node is json object.
 */
bool jsonIsObject(JsonNode* node);

/**
 * Check if current node is json array.
 */
bool jsonIsArray(JsonNode* node);

/**
 * Check if current node is a null value.
 */
bool jsonIsNull(JsonNode* node);

/**
 * Check if current node is a string value.
 */
bool jsonIsString(JsonNode* node);

/**
 * Check if current node is numeric value.
 */
bool jsonIsNumber(JsonNode* node);

/**
 * Read json attribute key.
 */
const char* jsonReadKey(JsonNode* node);

/**
 * Read json string attribute value.
 */
const char* jsonReadString(JsonNode* node);

/**
 * Read json integer attribute value.
 */
int jsonReadInteger(JsonNode* node);

/**
 * Read json float/double attribute value.
 */
double jsonReadDouble(JsonNode* node);

/**
 * Initialize json generator.
 * @param ctx             Pointer to JsonContext object.
 * @param outputBuf       Output character buffer.
 * @param outputBufSizeof Sizeof output buffer.
 */
JsonNode* jsonGenerate(JsonContext* ctx, char* outputBuf, int outputBufSizeof);

/**
 * Flush all pending json objects to output.
 * Must be called at least once.
 */
void jsonGenerateFlush(JsonNode* node);

/**
 * Start writing new json object.
 */
JsonNode* jsonStartObject(JsonNode* node);

/**
 * Start writing new json array.
 */
JsonNode* jsonStartArray(JsonNode* node);

/**
 * Write json attribute key.
 */
void jsonWriteKey(JsonNode* node, const char* key);

/**
 * Write json null value.
 */
void jsonWriteNull(JsonNode* node);

/**
 * Write json integer value.
 */
void jsonWriteInteger(JsonNode* node, int value);

/**
 * Write json double/float value.
 */
void jsonWriteDouble(JsonNode* node, double value);

/**
 * Write json string value.
 */
void jsonWriteString(JsonNode* node, const char* value);

/**
 * Check if there have been any errors during parsing or
 * generation.
 */
bool jsonFailed(JsonContext* ctx);

/** @} */

#endif /* _POTATO_BUS_JSON_H */
