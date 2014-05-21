///////////////////////////////////////////////////////////////////////////////
//
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2008-2014 Hewlett-Packard Development Company, L.P.
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
//
///////////////////////////////////////////////////////////////////////////////

#ifndef __MONLOGGING_H
#define __MONLOGGING_H

#include <string>

using namespace std;

#include "sqevlog/evl_sqlog_writer.h"
#include "common/evl_sqlog_eventnum.h"
#include "lock.h"

#define MON_STRING_BUF_SIZE 256
#define MON_EVENT_BUF_SIZE 4096

#define MEM_LOG_NUM_ENTRIES (64 * 1024) // in-memory log, 64K entries
#define MEM_LOG_KEY( monpid ) ( (0x1234 << 16) + (monpid & 0xFFFF) )
#define MEM_LOG_SIZE( entries ) ( sizeof(memLogHeader_t) + ( sizeof(memLogEntry_t) * entries ) )

// Used by monitor process
int mon_log_write(int event_type, posix_sqlog_severity_t severity, char *evl_buf);

void mem_log_write(int eventType, int value1 = 0, int value2 = 0);

// Used by watchdog process (does not use Sealog by intention )
int wdt_log_write(int event_type, posix_sqlog_severity_t severity, char *evl_buf);

// header of the memory log buffer
typedef struct memLogHeader
{
    int entries_;           // total number of entries. 
    int currEntry_;         // current available entry 
    int cycles_;            // number of times buffer got full
} memLogHeader_t;

// an entry in the memory log buffer. 
typedef struct memLogEntry
{
    struct timespec ts_;    // timestamp when the entry is logged
    int eventType_;         // event identifier 
    int value1_;            // optional trace value 
    int value2_;            // optional trace value
} memLogEntry_t;

class CMonLog
{
public:
    CMonLog( const char *logFilePrefix );
    ~CMonLog();

    int write(int eventType, posix_sqlog_severity_t severity, char *msg);

    void memLogWrite(int event_type, int value1 = 0, int value2 = 0);

    void writeAltLog(int event_type, posix_sqlog_severity_t severity, char *msg);

    int getLogFileNum();

    void setLogFileNum(int value);

    void setLogFileType(int value);

    void setUseAltLog(bool value);

    void setupInMemoryLog();

    enum MemLogEvents
    {
        MON_REINTEGRATE_1,
        MON_REINTEGRATE_2,
        MON_REINTEGRATE_3,
        MON_REINTEGRATE_4,
        MON_REINTEGRATE_5,
        MON_REINTEGRATE_6,
        MON_REINTEGRATE_7,
        MON_REINTEGRATE_8,
        MON_CONNTONEWMON_1,
        MON_CONNTONEWMON_2,
        MON_CONNTONEWMON_3,
        MON_CONNTONEWMON_4,
        MON_CONNTONEWMON_5,
        MON_CONNTONEWMON_6,
        MON_CONNTONEWMON_7,
        MON_UPDATE_CLUSTER_1,
        MON_UPDATE_CLUSTER_2,
        MON_NODE_1,
        MON_UPDATE_CLUSTER_3,
        MON_UPDATE_CLUSTER_4,
        MON_REINTEGRATE_9,
        MON_UPDATE_CLUSTER_5,
        MON_UPDATE_CLUSTER_6,
    };

private:

    // Monitor's start time. This is set when monitor starts.
    struct timeval startTime_;

    // formatted start time -      
    char startTimeFmt_[20];
 
    // File number to be appended to the file name. 
    // This helps in starting a new log file in real time.
    int logFileNum_;

    // Log file name prefix
    string logFileNamePrefix_;

    // Type of log file - just log, log with persist option etc. 
    int logFileType_;

    // flag to turn on/off the alternate logging feature.
    bool useAltLog_;
  
    // shared memory id for in-memory log data.
    int memLogID_;
    
    // pointer to the header of the in-memory log data.
    memLogHeader_t *memLogHeader_;

    // pointer to the first entry of the in-memory log data
    memLogEntry_t  *memLogBase_;

    // lock for use when writing to in-memory log
    CLock memLogLock_;
};

#endif
