/*
 * Copyright (c) 2002-2017 "Neo Technology,"
 * Network Engine for Objects in Lund AB [http://neotechnology.com]
 *
 * This file is part of Neo4j.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <assert.h>
#include "string.h"
#include "values.h"
#include "mem.h"


void BoltValue_to_Num8(struct BoltValue* value, uint8_t x)
{
    _format(value, BOLT_NUM8, 1, NULL, 0);
    value->data.as_uint8[0] = x;
}

void BoltValue_to_Num16(struct BoltValue* value, uint16_t x)
{
    _format(value, BOLT_NUM16, 1, NULL, 0);
    value->data.as_uint16[0] = x;
}

void BoltValue_to_Num32(struct BoltValue* value, uint32_t x)
{
    _format(value, BOLT_NUM32, 1, NULL, 0);
    value->data.as_uint32[0] = x;
}

void BoltValue_to_Num64(struct BoltValue* value, uint64_t x)
{
    _format(value, BOLT_NUM64, 1, NULL, 0);
    value->data.as_uint64[0] = x;
}

void BoltValue_to_Num8Array(struct BoltValue* value, uint8_t* array, int32_t size)
{
    if (size <= sizeof(value->data) / sizeof(uint8_t))
    {
        _format(value, BOLT_NUM8_ARRAY, size, NULL, 0);
        memcpy(value->data.as_uint8, array, (size_t)(size));
    }
    else
    {
        _format(value, BOLT_NUM8_ARRAY, size, array, sizeof_n(uint8_t, size));
    }
}

void BoltValue_to_Num16Array(struct BoltValue* value, uint16_t* array, int32_t size)
{
    if (size <= sizeof(value->data) / sizeof(uint16_t))
    {
        _format(value, BOLT_NUM16_ARRAY, size, NULL, 0);
        memcpy(value->data.as_uint16, array, (size_t)(size));
    }
    else
    {
        _format(value, BOLT_NUM16_ARRAY, size, array, sizeof_n(uint16_t, size));
    }
}

void BoltValue_to_Num32Array(struct BoltValue* value, uint32_t* array, int32_t size)
{
    if (size <= sizeof(value->data) / sizeof(uint32_t))
    {
        _format(value, BOLT_NUM32_ARRAY, size, NULL, 0);
        memcpy(value->data.as_uint32, array, (size_t)(size));
    }
    else
    {
        _format(value, BOLT_NUM32_ARRAY, size, array, sizeof_n(uint32_t, size));
    }
}

void BoltValue_to_Num64Array(struct BoltValue* value, uint64_t* array, int32_t size)
{
    if (size <= sizeof(value->data) / sizeof(uint64_t))
    {
        _format(value, BOLT_NUM64_ARRAY, size, NULL, 0);
        memcpy(value->data.as_uint64, array, (size_t)(size));
    }
    else
    {
        _format(value, BOLT_NUM64_ARRAY, size, array, sizeof_n(uint64_t, size));
    }
}

uint8_t BoltNum8_get(const struct BoltValue* value)
{
    return value->data.as_uint8[0];
}

uint16_t BoltNum16_get(const struct BoltValue* value)
{
    return value->data.as_uint16[0];
}

uint32_t BoltNum32_get(const struct BoltValue* value)
{
    return value->data.as_uint32[0];
}

uint64_t BoltNum64_get(const struct BoltValue* value)
{
    return value->data.as_uint64[0];
}

uint8_t BoltNum8Array_get(const struct BoltValue* value, int32_t index)
{
    const uint8_t* data = value->size <= sizeof(value->data) / sizeof(uint8_t) ?
                          value->data.as_uint8 : value->data.extended.as_uint8;
    return data[index];
}

uint16_t BoltNum16Array_get(const struct BoltValue* value, int32_t index)
{
    const uint16_t* data = value->size <= sizeof(value->data) / sizeof(uint16_t) ?
                           value->data.as_uint16 : value->data.extended.as_uint16;
    return data[index];
}

uint32_t BoltNum32Array_get(const struct BoltValue* value, int32_t index)
{
    const uint32_t* data = value->size <= sizeof(value->data) / sizeof(uint32_t) ?
                           value->data.as_uint32 : value->data.extended.as_uint32;
    return data[index];
}

uint64_t BoltNum64Array_get(const struct BoltValue* value, int32_t index)
{
    const uint64_t* data = value->size <= sizeof(value->data) / sizeof(uint64_t) ?
                           value->data.as_uint64 : value->data.extended.as_uint64;
    return data[index];
}

int BoltNum8_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM8);
    fprintf(file, "n8(%u)", BoltNum8_get(value));
    return 0;
}

int BoltNum16_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM16);
    fprintf(file, "n16(%u)", BoltNum16_get(value));
    return 0;
}

int BoltNum32_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM32);
    fprintf(file, "n32(%u)", BoltNum32_get(value));
    return 0;
}

int BoltNum64_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM64);
    fprintf(file, "n64(%lu)", BoltNum64_get(value));
    return 0;
}

int BoltNum8Array_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM8_ARRAY);
    fprintf(file, "n8[");
    for (int i = 0; i < value->size; i++)
    {
        fprintf(file, i == 0 ? "%u" : ", %u", BoltNum8Array_get(value, i));
    }
    fprintf(file, "]");
    return 0;
}

int BoltNum16Array_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM16_ARRAY);
    fprintf(file, "n16[");
    for (int i = 0; i < value->size; i++)
    {
        fprintf(file, i == 0 ? "%u" : ", %u", BoltNum16Array_get(value, i));
    }
    fprintf(file, "]");
    return 0;
}

int BoltNum32Array_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM32_ARRAY);
    fprintf(file, "n32[");
    for (int i = 0; i < value->size; i++)
    {
        fprintf(file, i == 0 ? "%u" : ", %u", BoltNum32Array_get(value, i));
    }
    fprintf(file, "]");
    return 0;
}

int BoltNum64Array_write(struct BoltValue * value, FILE * file)
{
    assert(BoltValue_type(value) == BOLT_NUM64_ARRAY);
    fprintf(file, "n64[");
    for (int i = 0; i < value->size; i++)
    {
        fprintf(file, i == 0 ? "%lu" : ", %lu", BoltNum64Array_get(value, i));
    }
    fprintf(file, "]");
    return 0;
}
