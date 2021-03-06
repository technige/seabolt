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


#include <stdarg.h>
#include <malloc.h>

#include "logging.h"


static FILE* __bolt_log_file;


void BoltLog_set_file(FILE* log_file)
{
    __bolt_log_file = log_file;
}

void BoltLog_info(const char* message, ...)
{
    if (__bolt_log_file == NULL) return;
    va_list args;
    va_start(args, message);
    vfprintf(__bolt_log_file, message, args);
    va_end(args);
    fprintf(__bolt_log_file, "\n");
}

void BoltLog_error(const char* message, ...)
{
    if (__bolt_log_file == NULL) return;
    va_list args;
    va_start(args, message);
    vfprintf(__bolt_log_file, message, args);
    va_end(args);
    fprintf(__bolt_log_file, "\n");
}
