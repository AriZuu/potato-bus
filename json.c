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
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#include "potato-json.h"

static JsonNode* deeper(JsonNode* parent)
{
  JsonContext* context = parent->context;
  int depth = parent - context->nodes;

  if (depth + 1 == POTATO_JSON_MAX_DEPTH) {

    parent->context->error = true;
    return NULL;
  }

  JsonNode* out = context->nodes + depth + 1;

  out->context = context;
  return out;
}

static int depth(JsonNode* node)
{
  JsonContext* context = node->context;

  return node - context->nodes;
}

static void restoreBuffer(JsonNode* n)
{
  JsonContext* ctx = n->context;

  if (ctx->keyEnd != NULL) {

    *ctx->keyEnd = ctx->restoreKey;
    ctx->keyEnd = NULL;
  }

  if (ctx->valueEnd != NULL) {

    *ctx->valueEnd = ctx->restoreValue;
    ctx->valueEnd = NULL;
  }
}

static void nullTerminate(JsonNode* n)
{
  JsonContext* ctx = n->context;

  if (n->key) {

    ctx->keyEnd = n->key + n->keyLen;
    ctx->restoreKey = *ctx->keyEnd;
    *ctx->keyEnd = '\0';
  }

  if (n->base) {

    ctx->valueEnd = n->base + n->len;
    ctx->restoreValue = *ctx->valueEnd;
    *ctx->valueEnd = '\0';
  }
}

static int jsonScan(char** bufPtr, char** tokenData)
{
  char* ptr = *bufPtr;

  while (*ptr != '\0' && isspace(*ptr))
    ++ptr;

  if (*ptr == ',' ||
      *ptr == ':' ||
      *ptr == '{' ||
      *ptr == '}' ||
      *ptr == '[' ||
      *ptr == ']') {

    *tokenData = ptr;
    *bufPtr = ptr + 1;
    return 1;
  } 

  int len;

  if (*ptr == '"') {

    *tokenData = ptr;
    ++ptr;
    if (*ptr == '\0')
      return -1;

    len = 0;
    while (*ptr != '"' && *ptr != '\0') {

      len++;
      ptr++;
    }

    if (*ptr == '\0')
      return -1;

    ++ptr;
    *bufPtr = ptr;
    return len + 2;
  }

  *tokenData = ptr;
  len = 0;

  while (*ptr != ' ' && 
         *ptr != '}' &&
         *ptr != ']' &&
         *ptr != ',' &&
         *ptr != '\0') {

    len++;
    ptr++;
  }

  *bufPtr = ptr;
  return len;
}

JsonNode* jsonParse(JsonContext* ctx, char* json)
{
  JsonNode* root;

  ctx->keyEnd = NULL;
  ctx->valueEnd = NULL;

  while (*json && isspace(*json))
    json++;

// Toplevel must be either object or array.

  if (*json == '{') {

    root = ctx->nodes;
   
    root->context = ctx;
    root->base = json;
    root->pos = root->base;
    root->token = JsonObject;
    root->keyLen = 0;
    root->key = NULL;
    root->len = strlen(json);

    return root;
  }

  if (*json == '[') {

    root = ctx->nodes;

    root->context = ctx;
    root->base = json;
    root->pos = root->base;
    root = ctx->nodes;
    root->token = JsonArray;
    root->keyLen = 0;
    root->key = NULL;
    root->len = strlen(json);

    return root;
  }

  return NULL;
}

static int scanFor(JsonNode* node, int forCh)
{
  int total = 0;
  int len;
  char* str;
  char* start = node->pos;

  do {

    len = jsonScan(&node->pos, &str);
    if (len == 1 && *str == '{')
      len += scanFor(node, '}');
    else if (len == 1 && *str == '[')
      len += scanFor(node, ']');

    total += len;
  } while (len != 1 || *str != forCh);

  return node->pos - start + 1;
}

static void simpleValue(JsonNode* node)
{
  if (node->len >= 2 && *node->base == '"') {

    node->token = JsonString;
    node->base++;
    node->pos++;
    node->len -= 2;
    return;
  }

  if (node->len == 4 && !strncmp(node->base, "null", node->len)) {
 
    node->base = NULL;
    node->pos = NULL;
    node->token = JsonNull;
  }
  else
    node->token = JsonNumber;
}

void jsonReset(JsonNode* node)
{
  restoreBuffer(node);
  node->pos = node->base;
}

JsonNode* jsonNext(JsonNode* parent)
{
  JsonNode* node;

  restoreBuffer(parent);
  node = deeper(parent);

  if (node == NULL)
    return NULL;

  node->key = NULL;
  node->keyLen = 0;

  if (parent->token == JsonObject) {

    int len;
    char* str;

    // read key
    len = jsonScan(&parent->pos, &str);
    if (len == 1 && (*str == '{'  || *str == ',')) // XXX: skip begin
      len = jsonScan(&parent->pos, &str);
    
    if (len == 1 && *str == '}')
      return NULL;

    if (*str != '"')
      return NULL;

    node->key = str + 1;
    node->keyLen = len - 2;

    len = jsonScan(&parent->pos, &str);
    if (len != 1 && *str != ':')
      return NULL;
   
    len = jsonScan(&parent->pos, &str);
    node->base = str;
    node->pos  = str;

    if (len == 1 && *str == '{') {

      node->len = scanFor(parent, '}');
      node->token = JsonObject;
      nullTerminate(node);
      return node;
    }

    if (len == 1 && *str == '[') {

      node->len = scanFor(parent, ']');
      node->token = JsonArray;
      nullTerminate(node);
      return node;
    }

    node->len = len;
    simpleValue(node);
    nullTerminate(node);

    return node;
  }

  if (parent->token == JsonArray) {

    int len;
    char* str;

    // read value
    len = jsonScan(&parent->pos, &str);
    if (len == 1 && (*str == '['  || *str == ',')) // XXX: skip begin
      len = jsonScan(&parent->pos, &str);
    
    if (len == 1 && *str == ']')
      return NULL;

    node->base = str;
    node->pos  = str;

    if (len == 1 && *str == '{') {

      node->len = scanFor(parent, '}');
      node->token = JsonObject;
      nullTerminate(node);
      return node;
    }

    if (len == 1 && *str == '[') {

      node->len = scanFor(parent, ']');
      node->token = JsonArray;
      nullTerminate(node);
      return node;
    }

    node->len = len;
    simpleValue(node);
    nullTerminate(node);
    return node;
  }

  return NULL;
}

JsonNode* jsonFind(JsonNode* parent, const char* key)
{
  JsonNode* n;

  parent->pos = parent->base;

  do {

    n = jsonNext(parent);
    if (n->keyLen && n->keyLen == strlen(key) && n->key != NULL && !strncmp(n->key, key, n->keyLen))
      return n;
  } while (n);

  return NULL;
}

bool jsonIsObject(JsonNode* node)
{
  return node->token == JsonObject;
}

bool jsonIsArray(JsonNode* node)
{
  return node->token == JsonArray;
}

bool jsonIsNull(JsonNode* node)
{
  return node->token == JsonNull;
}

bool jsonIsString(JsonNode* node)
{
  return node->token == JsonString;
}

bool jsonIsNumber(JsonNode* node)
{
  return node->token == JsonNumber;
}

const char* jsonReadKey(JsonNode* node)
{
  return node->key;
}

const char* jsonReadString(JsonNode* node)
{
  return node->base;
}

int jsonReadInteger(JsonNode* node)
{
  return strtol(node->base, (char**)NULL, 10);
}

double jsonReadDouble(JsonNode* node)
{
  return strtod(node->base, (char**)NULL);
}

static int append(JsonContext* ctx, const char* fmt, ...)
{
    va_list ap;
    int len;
    
    if (ctx->left <= 0) {

      ctx->error = true;
      return -1;
    }

    va_start(ap, fmt);
    len = vsnprintf(ctx->pos, ctx->left, fmt, ap);
    va_end(ap);

    if (len > ctx->left) {

      ctx->error = true;
      return -1;
    }

    ctx->pos = ctx->pos + len;
    ctx->left -= len;
    return 0;
}

JsonNode* jsonGenerate(JsonContext* ctx, char* outputBuf, int outputBufSizeof)
{
  ctx->pos = outputBuf;
  ctx->left = outputBufSizeof - 1;
  ctx->depth = -1;
  
  JsonNode* n;

  n = ctx->nodes;
  n->context = ctx;
  
  return n;
}

static void endCheck(JsonNode* node)
{
  JsonContext* ctx;

  ctx = node->context;
  while (depth(node) < ctx->depth) {

    append(ctx, ctx->nodes[ctx->depth].token == JsonObject ? "}" : "]");
    ctx->depth--;
  }
}

void jsonGenerateFlush(JsonNode* node)
{
  JsonContext* ctx;

  ctx = node->context;
  endCheck(node);
  append(ctx, node->token == JsonObject ? "}" : "]");
}

JsonNode* jsonStartObject(JsonNode* node)
{
  JsonNode* out;
  JsonContext* ctx;

  ctx = node->context;
  if (ctx->depth == -1) {

// bootstrap
    ctx->depth = 0;
    out = node;
  }
  else {

    node->values++;
    out = deeper(node);
    if (out == NULL)
      return NULL;

    ctx->depth++;
  }
   
  out->token = JsonObject;
  out->values = 0;
  append(ctx, "{");

  return out;
}

JsonNode* jsonStartArray(JsonNode* node)
{
  JsonContext* ctx;
  JsonNode* out;

  ctx = node->context;
  if (ctx->depth == -1) {

// bootstrap
    ctx->depth = 0;
    out = node;
  }
  else {

    node->values++;
    out = deeper(node);
    if (out == NULL)
      return NULL;

    ctx->depth++;
  }
   
  out->token = JsonArray;
  out->values = 0;
  append(ctx, "[");

  return out;
}

void jsonWriteKey(JsonNode* node, const char* key)
{
  JsonContext* ctx;

  ctx = node->context;
  endCheck(node);

  if (node->values > 0)
    if (append(ctx, ",") == -1)
      return;

  if (append(ctx, "\"%s\":", key) == -1)
    return;

  node->values = 0;
}

void jsonWriteNull(JsonNode* node)
{
  JsonContext* ctx;

  ctx = node->context;
  endCheck(node);

  node->values++;
  if (append(ctx, "null") == -1)
    return;
}

void jsonWriteInteger(JsonNode* node, int value)
{
  JsonContext* ctx;

  ctx = node->context;
  endCheck(node);

  if (node->values > 0)
    if (append(ctx, ",") == -1)
      return;

  node->values++;
  if (append(ctx, "%d", value) == -1)
    return;
}

void jsonWriteDouble(JsonNode* node, double value)
{
  JsonContext* ctx;

  ctx = node->context;
  endCheck(node);

  if (node->values > 0)
    if (append(ctx, ",") == -1)
      return;

  node->values++;
  if (append(ctx, "%lf", value) == -1)
    return;
}

void jsonWriteString(JsonNode* node, const char* value)
{
  JsonContext* ctx;

  ctx = node->context;
  endCheck(node);

  if (node->values > 0)
    if (append(ctx, ",") == -1)
      return;

  node->values++;
  if (append(ctx, "\"%s\"", value) == -1)
    return;
}

bool jsonFailed(JsonContext* context)
{
  return context->error;
}

