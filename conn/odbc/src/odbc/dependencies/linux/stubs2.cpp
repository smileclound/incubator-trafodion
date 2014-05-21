// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2010-2014 Hewlett-Packard Development Company, L.P.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// @@@ END COPYRIGHT @@@
#include <assert.h>

// As a temporary measure "assert" whenever one of the following
// functions is invoked.
void deleteSegment(int segmentnumber)
{
    assert(0);
}

void addSegment(int segmentnumber)
{
    assert(0);
}

void updateSegment_CPUDOWN(int segmentnumber, short cpu)
{
    assert(0);
}

void updateSegment_CPUUP(int segmentnumber, short cpu)
{
    assert(0);
}

void updateSegment_REMOTECPUDOWN(int segmentnumber, short cpu)
{
    assert(0);
}

void updateSegment_REMOTECPUUP(int segmentnumber, short cpu)
{
    assert(0);
}
