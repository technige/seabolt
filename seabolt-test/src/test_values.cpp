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

#include <math.h>
#include <assert.h>
#include <memory.h>
#include "test_values.hpp"

extern "C" {
    #include "mem.h"
    #include "values.h"
}

#include <catch.hpp>

using namespace std;


SCENARIO("Test Null")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        THEN("the initial type should be Null")
        {
            REQUIRE(BoltValue_type(value) == BOLT_NULL);
        }
        WHEN("the type is explicitly set to Null")
        {
            BoltValue_to_Null(value);
            THEN("the type should still be Null")
            {
                REQUIRE(BoltValue_type(value) == BOLT_NULL);
            }
        }
        BoltValue_destroy(value);
    }
}

SCENARIO("Test Bit values")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        WHEN("the value is mutated to a Bit")
        {
            for (int i = 0; i <= 1; i++)
            {
                BoltValue_to_Bit(value, (char)(i));
                THEN("the type should be Bit")
                {
                    REQUIRE(BoltValue_type(value) == BOLT_BIT);
                }
                THEN("the inner value should be set to " + to_string(i))
                {
                    REQUIRE(BoltBit_get(value) == (char)(i));
                }
            }
        }
        BoltValue_destroy(value);
    }
}

SCENARIO("Test BitArray values")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        WHEN("the value is mutated to a BitArray")
        {
            int32_t size = 2;
            char array[] = {0, 1};
            BoltValue_to_BitArray(value, array, size);
            THEN("the type should be BitArray")
            {
                REQUIRE(BoltValue_type(value) == BOLT_BIT_ARRAY);
            }
            THEN("the inner values should be set correctly")
            {
                REQUIRE(BoltBitArray_get(value, 0) == 0);
                REQUIRE(BoltBitArray_get(value, 1) == 1);
            }
        }
        BoltValue_destroy(value);
    }
}

SCENARIO("Test Byte values")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        WHEN("the value is mutated to a Byte")
        {
            for (int i = 0x00; i <= 0xFF; i++)
            {
                BoltValue_to_Byte(value, (char)(i));
                THEN("the type should be Byte")
                {
                    REQUIRE(BoltValue_type(value) == BOLT_BYTE);
                }
                THEN("the inner value should be set to " + to_string(i))
                {
                    REQUIRE(BoltByte_get(value) == (char)(i));
                }
            }
        }
        BoltValue_destroy(value);
    }
}

SCENARIO("Test ByteArray values", "[ByteArray]")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        WHEN("the value is mutated to a ByteArray")
        {
            for (int32_t size = 0; size <= 0x100; size += 8)
            {
                char array[] = ("\x00\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b\x0c\r\x0e\x0f"
                                "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
                                " !\"#$%&'()*+,-./0123456789:;<=>?"
                                "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                                "`abcdefghijklmnopqrstuvwxyz{|}~\x7f"
                                "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
                                "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
                                "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
                                "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
                                "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
                                "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
                                "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
                                "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff");
                BoltValue_to_ByteArray(value, array, size);
                THEN("the type should be ByteArray (size = " + to_string(size) + ")")
                {
                    REQUIRE(BoltValue_type(value) == BOLT_BYTE_ARRAY);
                }
                THEN("the inner values should be set correctly (size = " + to_string(size) + ")")
                {
                    for (int i = 0; i < size; i++)
                    {
                        REQUIRE(BoltByteArray_get(value, i) == (char)(i));
                    }
                }
            }
        }
        BoltValue_destroy(value);
    }
}

SCENARIO("Test String8 values")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        WHEN("set to an empty string")
        {
            const char* text = "";
            int32_t text_size = 0;
            BoltValue_to_String8(value, text, text_size);
            THEN("the type should be String8")
            {
                REQUIRE(BoltValue_type(value) == BOLT_STRING8);
            }
            THEN("the size should be correct")
            {
                REQUIRE(value->size == text_size);
            }
            THEN("the inner value should be correct")
            {
                const char* stored_text = BoltString8_get(value);
                REQUIRE(strncmp(text, stored_text, text_size) == 0);
            }
        }
        WHEN("set to a simple string")
        {
            const char* text = "hello, world";
            int32_t text_size = 12;
            BoltValue_to_String8(value, text, text_size);
            THEN("the type should be String8")
            {
                REQUIRE(BoltValue_type(value) == BOLT_STRING8);
            }
            THEN("the size should be correct")
            {
                REQUIRE(value->size == text_size);
            }
            THEN("the inner value should be correct")
            {
                const char* stored_text = BoltString8_get(value);
                REQUIRE(strncmp(text, stored_text, text_size) == 0);
            }
        }
        WHEN("set to a long string containing a zero byte")
        {
            const char* text = "there is a null character -> \x00 <- in the middle of this string";
            int32_t text_size = 62;
            BoltValue_to_String8(value, text, text_size);
            THEN("the type should be String8")
            {
                REQUIRE(BoltValue_type(value) == BOLT_STRING8);
            }
            THEN("the size should be correct")
            {
                REQUIRE(value->size == text_size);
            }
            THEN("the inner value should be correct")
            {
                const char* stored_text = BoltString8_get(value);
                REQUIRE(strncmp(text, stored_text, text_size) == 0);
            }
        }
        WHEN("set back to a simple string")
        {
            const char* text = "back to a short one";
            int32_t text_size = 19;
            BoltValue_to_String8(value, text, text_size);
            THEN("the type should be String8")
            {
                REQUIRE(BoltValue_type(value) == BOLT_STRING8);
            }
            THEN("the size should be correct")
            {
                REQUIRE(value->size == text_size);
            }
            THEN("the inner value should be correct")
            {
                const char* stored_text = BoltString8_get(value);
                REQUIRE(strncmp(text, stored_text, text_size) == 0);
            }
        }
        BoltValue_destroy(value);
    }
}

SCENARIO("Test String8Array values")
{
    GIVEN("a new value")
    {
        struct BoltValue* value = BoltValue_create();
        BoltValue_to_String8Array(value, 5);
        BoltString8Array_put(value, 0, "hello", 5);
        BoltString8Array_put(value, 1, "world", 5);
        BoltString8Array_put(value, 2, "here is a very very very very very very very very long string", 61);
        BoltString8Array_put(value, 3, "", 0);
        BoltString8Array_put(value, 4, "that last one was empty!!", 25);
        THEN("the type should be String8Array")
        {
            REQUIRE(BoltValue_type(value) == BOLT_STRING8_ARRAY);
        }
        THEN("the size should be correct")
        {
            REQUIRE(value->size == 5);
        }
        THEN("the values should be correct")
        {
            char* text;
            int32_t size;

            text = BoltString8Array_get(value, 0);
            size = BoltString8Array_get_size(value, 0);
            REQUIRE(strncmp(text, "hello", (size_t)(size)) == 0);

            text = BoltString8Array_get(value, 1);
            size = BoltString8Array_get_size(value, 1);
            REQUIRE(strncmp(text, "world", (size_t)(size)) == 0);

            text = BoltString8Array_get(value, 2);
            size = BoltString8Array_get_size(value, 2);
            REQUIRE(strncmp(text, "here is a very very very very very very very very long string", (size_t)(size)) == 0);

            text = BoltString8Array_get(value, 3);
            size = BoltString8Array_get_size(value, 3);
            REQUIRE(strncmp(text, "", (size_t)(size)) == 0);

            text = BoltString8Array_get(value, 4);
            size = BoltString8Array_get_size(value, 4);
            REQUIRE(strncmp(text, "that last one was empty!!", (size_t)(size)) == 0);
        }
        BoltValue_destroy(value);
    }
}





void _dump(struct BoltValue* value)
{
    BoltValue_write(value, stdout, 0);
    fprintf(stdout, "\n");
}

void _test_list()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_List(value, 6);
    BoltValue_to_Int32(BoltList_value(value, 0), 1234567);
    BoltValue_to_Int32(BoltList_value(value, 1), 2345678);
    BoltValue_to_Int32(BoltList_value(value, 2), 3456789);
    BoltValue_to_String8(BoltList_value(value, 3), "hello", 5);
    BoltValue_to_List(BoltList_value(value, 5), 3);
    BoltValue_to_Num8(BoltList_value(BoltList_value(value, 5), 0), 77);
    BoltValue_to_Num8(BoltList_value(BoltList_value(value, 5), 1), 88);
    BoltValue_to_Byte(BoltList_value(BoltList_value(value, 5), 2), 99);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_LIST);
    assert(value->size == 6);
    BoltValue_destroy(value);
}

void _test_empty_list()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_List(value, 0);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_LIST);
    assert(value->size == 0);
    BoltValue_destroy(value);
}

void _test_list_growth()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_List(value, 0);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_LIST);
    assert(value->size == 0);
    for (int i = 0; i < 3; i++)
    {
        int size = i + 1;
        BoltList_resize(value, size);
        BoltValue_to_Int8(BoltList_value(value, i), (int8_t)(size));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_LIST);
        assert(value->size == size);
    }
    BoltValue_destroy(value);
}

void _test_list_shrinkage()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_List(value, 3);
    BoltValue_to_Int8(BoltList_value(value, 0), 1);
    BoltValue_to_Int8(BoltList_value(value, 1), 2);
    BoltValue_to_Int8(BoltList_value(value, 2), 3);
    assert(BoltValue_type(value) == BOLT_LIST);
    assert(value->size == 3);
    for (int size = 3; size >= 0; size--)
    {
        BoltList_resize(value, size);
        _dump(value);
        assert(BoltValue_type(value) == BOLT_LIST);
        assert(value->size == size);
    }
    BoltValue_destroy(value);
}

void test_list()
{
    _test_list();
    _test_empty_list();
    _test_list_growth();
    _test_list_shrinkage();
}

void _test_utf8_dictionary()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Dictionary8(value, 4);
    BoltDictionary8_set_key(value, 0, "a", 1);
    BoltDictionary8_set_key(value, 1, "b", 1);
    BoltDictionary8_set_key(value, 2, "c", 1);
    BoltValue_to_Int8(BoltDictionary8_value(value, 0), 1);
    BoltValue_to_Int8(BoltDictionary8_value(value, 1), 2);
    BoltValue_to_Int8(BoltDictionary8_value(value, 2), 3);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_DICTIONARY8);
    assert(value->size == 4);
    BoltValue_destroy(value);
}

void _test_empty_utf8_dictionary()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Dictionary8(value, 0);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_DICTIONARY8);
    assert(value->size == 0);
    BoltValue_destroy(value);
}

void _test_single_entry_utf8_dictionary()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Dictionary8(value, 1);
    BoltDictionary8_set_key(value, 0, "hello", 5);
    BoltValue_to_String8(BoltDictionary8_value(value, 0), "world", 5);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_DICTIONARY8);
    assert(value->size == 1);
    BoltValue_destroy(value);
}

void _test_utf8_dictionary_growth()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Dictionary8(value, 0);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_DICTIONARY8);
    assert(value->size == 0);
    for (int i = 0; i < 3; i++)
    {
        int size = i + 1;
        BoltValue_to_Dictionary8(value, size);
        BoltDictionary8_set_key(value, i, "key", 3);
        BoltValue_to_Int8(BoltDictionary8_value(value, i), (int8_t)(size));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_DICTIONARY8);
        assert(value->size == size);
    }
    BoltValue_destroy(value);
}

void _test_utf8_dictionary_shrinkage()
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Dictionary8(value, 3);
    BoltDictionary8_set_key(value, 0, "a", 1);
    BoltDictionary8_set_key(value, 1, "b", 1);
    BoltDictionary8_set_key(value, 2, "c", 1);
    BoltValue_to_Int8(BoltDictionary8_value(value, 0), 1);
    BoltValue_to_Int8(BoltDictionary8_value(value, 1), 2);
    BoltValue_to_Int8(BoltDictionary8_value(value, 2), 3);
    assert(BoltValue_type(value) == BOLT_DICTIONARY8);
    assert(value->size == 3);
    for (int size = 3; size >= 0; size--)
    {
        BoltValue_to_Dictionary8(value, size);
        _dump(value);
        assert(BoltValue_type(value) == BOLT_DICTIONARY8);
        assert(value->size == size);
    }
    BoltValue_destroy(value);
}

void test_utf8_dictionary()
{
    _test_utf8_dictionary();
    _test_empty_utf8_dictionary();
    _test_single_entry_utf8_dictionary();
    _test_utf8_dictionary_growth();
    _test_utf8_dictionary_shrinkage();
}

int test_num8()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x100)
    {
        BoltValue_to_Num8(value, (uint8_t)(x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_NUM8);
        assert(BoltNum8_get(value) == x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_num8_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    uint8_t array[size];
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x100)
    {
        array[n] = (uint8_t)(x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_to_Num8Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_NUM8_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltNum8Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_num16()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x10000)
    {
        BoltValue_to_Num16(value, (uint16_t)(x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_NUM16);
        assert(BoltNum16_get(value) == x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_num16_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    uint16_t array[size];
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x10000)
    {
        array[n] = (uint16_t)(x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_to_Num16Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_NUM16_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltNum16Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_num32()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x100000000)
    {
        BoltValue_to_Num32(value, (uint32_t)(x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_NUM32);
        assert(BoltNum32_get(value) == x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_num32_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    uint32_t array[size];
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x100000000)
    {
        array[n] = (uint32_t)(x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_to_Num32Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_NUM32_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltNum32Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_num64()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0xA000000000000000L)
    {
        BoltValue_to_Num64(value, (uint64_t)(x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_NUM64);
        assert(BoltNum64_get(value) == x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_num64_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    uint64_t array[size];
    int n = 0;
    unsigned long long x = 0, y = 1, z;
    while (x < 0xA000000000000000L)
    {
        array[n] = (uint64_t)(x);
        n += 1, z = x + y, x = y, y = z;
    }
    BoltValue_to_Num64Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_NUM64_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltNum64Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_int8()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x80)
    {
        BoltValue_to_Int8(value, (int8_t)(s * x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_INT8);
        assert(BoltInt8_get(value) == s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_int8_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    int8_t array[size];
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x80)
    {
        array[n] = (int8_t)(s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_to_Int8Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_INT8_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltInt8Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_int16()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x8000)
    {
        BoltValue_to_Int16(value, (int16_t)(s * x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_INT16);
        assert(BoltInt16_get(value) == s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_int16_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    int16_t array[size];
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x8000)
    {
        array[n] = (int16_t)(s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_to_Int16Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_INT16_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltInt16Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_int32()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x80000000)
    {
        BoltValue_to_Int32(value, (int32_t)(s * x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_INT32);
        assert(BoltInt32_get(value) == s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_int32_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    int32_t array[size];
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x80000000)
    {
        array[n] = (int32_t)(s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_to_Int32Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_INT32_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltInt32Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

int test_int64()
{
    struct BoltValue* value = BoltValue_create();
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x8000000000000000)
    {
        BoltValue_to_Int64(value, (int64_t)(s * x));
        _dump(value);
        assert(BoltValue_type(value) == BOLT_INT64);
        assert(BoltInt64_get(value) == s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_destroy(value);
    return n;
}

void test_int64_array(int size)
{
    struct BoltValue* value = BoltValue_create();
    int64_t array[size];
    int n = 0, s = 1;
    unsigned long long x = 0, y = 1, z;
    while (x < 0x8000000000000000)
    {
        array[n] = (int64_t)(s * x);
        n += 1, s = -s, z = x + y, x = y, y = z;
    }
    BoltValue_to_Int64Array(value, array, size);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_INT64_ARRAY);
    for (int i = 0; i < size; i++)
    {
        assert(BoltInt64Array_get(value, i) == array[i]);
    }
    BoltValue_destroy(value);
}

void _test_float32(float x)
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Float32(value, x);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_FLOAT32);
    assert(BoltFloat32_get(value) == x ||
           (isnanf(BoltFloat32_get(value)) && isnanf(x)));
    BoltValue_destroy(value);
}

void test_float32()
{
    _test_float32(0.0F);
    _test_float32(0.375F);
    _test_float32(1.0F);
    _test_float32(-1.0F);
    _test_float32(3.14159F);
    _test_float32(-3.14159F);
    _test_float32(6.02214086e23F);
    _test_float32(3.402823e38F);
    _test_float32(INFINITY);
    _test_float32(-INFINITY);
    _test_float32(NAN);
}

void test_float32_array()
{
    struct BoltValue* value = BoltValue_create();
    float array[11] = {0.0F, 0.375F, 1.0F, -1.0F, 3.14159F, -3.14159F,
                       6.02214086e23F, 3.402823e38F, INFINITY, -INFINITY, NAN};
    BoltValue_to_Float32Array(value, array, 11);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_FLOAT32_ARRAY);
    assert(value->size == 11);
    assert(BoltFloat32Array_get(value, 0) == 0.0F);
    assert(BoltFloat32Array_get(value, 1) == 0.375F);
    assert(BoltFloat32Array_get(value, 2) == 1.0F);
    assert(BoltFloat32Array_get(value, 3) == -1.0F);
    assert(BoltFloat32Array_get(value, 4) == 3.14159F);
    assert(BoltFloat32Array_get(value, 5) == -3.14159F);
    assert(BoltFloat32Array_get(value, 6) == 6.02214086e23F);
    assert(BoltFloat32Array_get(value, 7) == 3.402823e38F);
    assert(BoltFloat32Array_get(value, 8) == INFINITY);
    assert(BoltFloat32Array_get(value, 9) == -INFINITY);
    assert(isnanf(BoltFloat32Array_get(value, 10)));
    BoltValue_destroy(value);
}

void _test_float64(double x)
{
    struct BoltValue* value = BoltValue_create();
    BoltValue_to_Float64(value, x);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_FLOAT64);
    assert(BoltFloat64_get(value) == x ||
           (isnan(BoltFloat64_get(value)) && isnan(x)));
    BoltValue_destroy(value);
}

void test_float64()
{
    _test_float64(0.0);
    _test_float64(0.375);
    _test_float64(1.0);
    _test_float64(-1.0);
    _test_float64(3.14159);
    _test_float64(-3.14159);
    _test_float64(6.02214086e23);
    _test_float64(3.402823e38);
    _test_float64(INFINITY);
    _test_float64(-INFINITY);
    _test_float64(NAN);
}

void test_structure()
{
    struct BoltValue* value = BoltValue_create();
    const int NODE = 0xA0;
    BoltValue_to_Structure(value, NODE, 3);
    struct BoltValue* id = BoltStructure_value(value, 0);
    struct BoltValue* labels = BoltStructure_value(value, 1);
    struct BoltValue* properties = BoltStructure_value(value, 2);
    BoltValue_to_Int64(id, 123);
    BoltValue_to_String8Array(labels, 2);
    BoltString8Array_put(labels, 0, "Person", 6);
    BoltString8Array_put(labels, 1, "Employee", 8);
    BoltValue_to_Dictionary8(properties, 2);
    BoltDictionary8_set_key(properties, 0, "name", 4);
    BoltDictionary8_set_key(properties, 1, "age", 3);
    BoltValue_to_String8(BoltDictionary8_value(properties, 0), "Alice", 5);
    BoltValue_to_Num8(BoltDictionary8_value(properties, 1), 33);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_STRUCTURE && BoltStructure_code(value) == NODE);
    assert(value->size == 3);
    BoltValue_destroy(value);
}

void test_structure_array()
{
    struct BoltValue* value = BoltValue_create();
    const int NODE = 0xA0;
    BoltValue_to_StructureArray(value, NODE, 2);
    for (int i = 0; i < 2; i++)
    {
        BoltStructureArray_set_size(value, i, 3);
        struct BoltValue* id = BoltStructureArray_at(value, i, 0);
        struct BoltValue* labels = BoltStructureArray_at(value, i, 1);
        struct BoltValue* properties = BoltStructureArray_at(value, i, 2);
        BoltValue_to_Int64(id, 123 + 1);
        BoltValue_to_String8Array(labels, 2);
        BoltString8Array_put(labels, 0, "Person", 6);
        BoltString8Array_put(labels, 1, "Employee", 8);
        BoltValue_to_Dictionary8(properties, 2);
        BoltDictionary8_set_key(properties, 0, "name", 4);
        BoltDictionary8_set_key(properties, 1, "age", 3);
        BoltValue_to_String8(BoltDictionary8_value(properties, 0), i == 0 ? "Alice" : "Bob", i == 0 ? 5 : 3);
        BoltValue_to_Num8(BoltDictionary8_value(properties, 1), (uint8_t)(i == 0 ? 33 : 44));
    }
    _dump(value);
    assert(BoltValue_type(value) == BOLT_STRUCTURE_ARRAY && BoltStructure_code(value) == NODE);
    assert(value->size == 2);
    BoltValue_destroy(value);
}

void test_request()
{
    struct BoltValue* value = BoltValue_create();
    const int RUN = 0xA0;
    BoltValue_to_Request(value, RUN, 2);
    struct BoltValue* statement = BoltRequest_value(value, 0);
    struct BoltValue* parameters = BoltRequest_value(value, 1);
    BoltValue_to_String8(statement, "RETURN $x", 9);
    BoltValue_to_Dictionary8(parameters, 1);
    BoltDictionary8_set_key(parameters, 0, "x", 1);
    BoltValue_to_Int64(BoltDictionary8_value(parameters, 0), 1);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_REQUEST && BoltRequest_code(value) == RUN);
    assert(value->size == 2);
    BoltValue_destroy(value);
}

void test_summary()
{
    struct BoltValue* value = BoltValue_create();
    const int SUCCESS = 0xA0;
    BoltValue_to_Summary(value, SUCCESS, 1);
    struct BoltValue* metadata = BoltSummary_value(value, 0);
    BoltValue_to_Dictionary8(metadata, 2);
    BoltDictionary8_set_key(metadata, 0, "results", 7);
    BoltDictionary8_set_key(metadata, 1, "time", 4);
    BoltValue_to_Int64(BoltDictionary8_value(metadata, 0), 100);
    BoltValue_to_Int64(BoltDictionary8_value(metadata, 1), 123456789);
    _dump(value);
    assert(BoltValue_type(value) == BOLT_SUMMARY && BoltSummary_code(value) == SUCCESS);
    assert(value->size == 1);
    BoltValue_destroy(value);
}

int test_types()
{
    test_list();
    test_utf8_dictionary();
    test_num8_array(test_num8());
    test_num16_array(test_num16());
    test_num32_array(test_num32());
    test_num64_array(test_num64());
    test_int8_array(test_int8());
    test_int16_array(test_int16());
    test_int32_array(test_int32());
    test_int64_array(test_int64());
    test_float32();
    test_float32_array();
    test_float64();
    // TODO: test_float64_array();
    test_structure();
    test_structure_array();
    test_request();
    test_summary();
    assert(BoltMem_current_allocation() == 0);
    printf("*******\nMemory activity: %lld\n*******\n", BoltMem_allocation_events());
}
