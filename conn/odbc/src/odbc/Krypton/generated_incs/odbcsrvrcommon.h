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
#ifndef ODBCSRVRCOMMON_H_
#define ODBCSRVRCOMMON_H_
/*
 * Translation unit: ODBCSRVRCOMMON
 * Generated by CNPGEN(TANTAU CNPGEN TANTAU_AG_PC8 20001120.103031) on Mon Jan 31 11:14:08 2011
 * C++ constructs used
 * Header file for use with the CEE
 * Client functionality included
 * Server functionality included
 */
#include <stdarg.h>
#include <cee.h>
#if CEE_H_VERSION != 19991123
#error Version mismatch CEE_H_VERSION != 19991123
#endif
#include <idltype.h>
#if IDL_TYPE_H_VERSION != 19971225
#error Version mismatch IDL_TYPE_H_VERSION != 19971225
#endif
typedef IDL_enum CfgSvc_REQ_t;
#define UNKNOWN_NM ((IDL_enum) 0)
#define ASSOC_NM ((IDL_enum) 1)
#define DATASOURCE_NM ((IDL_enum) 2)
#define USER_NM ((IDL_enum) 3)
#define PROFILE_NM ((IDL_enum) 4)
typedef CfgSvc_REQ_t CfgSvc_REQ_def;
#define CfgSvc_REQ_def_cin_ ((char *) "h4+")
#define CfgSvc_REQ_def_csz_ ((IDL_unsigned_long) 3)
typedef IDL_enum DS_AUTOMATION_t;
#define AUTOMATIC ((IDL_enum) 0)
#define MANUAL ((IDL_enum) 1)
typedef DS_AUTOMATION_t DS_AUTOMATION_def;
#define DS_AUTOMATION_def_cin_ ((char *) "h1+")
#define DS_AUTOMATION_def_csz_ ((IDL_unsigned_long) 3)
typedef struct DATASOURCE_LIST_def_seq_ {
  IDL_unsigned_long _length;
  char pad_to_offset_8_[4];
  SQL_IDENTIFIER_def *_buffer;
  IDL_PTR_PAD(_buffer, 1)
} DATASOURCE_LIST_def;
#define DATASOURCE_LIST_def_cin_ ((char *) "c0+d512+")
#define DATASOURCE_LIST_def_csz_ ((IDL_unsigned_long) 8)
struct DATASOURCE_DESC_t {
  SQL_IDENTIFIER_def DSNm;
  char pad_to_offset_516_[3];
  IDL_unsigned_long DSNStatus;
  TIMESTAMP_STR_def LastStatusChange;
  char pad_to_offset_552_[1];
  IDL_long MaxSrvrCnt;
  IDL_long AvailSrvrCnt;
  IDL_long InitSrvrCnt;
  IDL_long StartAheadCnt;
  INTERVAL_NUM_def SrvrIdleTimeout;
  INTERVAL_NUM_def ConnIdleTimeout;
  INTERVAL_NUM_def RefreshRate;
  TIMESTAMP_STR_def LastUpdated;
  char pad_to_size_624_[1];
};
typedef DATASOURCE_DESC_t DATASOURCE_DESC_def;
#define DATASOURCE_DESC_def_cin_ ((char *) "b11+d512+Kd30+FFFFGGGd30+")
#define DATASOURCE_DESC_def_csz_ ((IDL_unsigned_long) 25)
struct RES_HIT_DESC_t {
  SQL_IDENTIFIER_def AttrNm;
  char pad_to_offset_520_[7];
  IDL_long_long Limit;
  IDL_char Action[129];
  char pad_to_offset_664_[7];
  IDL_long_long ActualValue;
};
typedef RES_HIT_DESC_t RES_HIT_DESC_def;
#define RES_HIT_DESC_def_cin_ ((char *) "b4+d512+Gd128+G")
#define RES_HIT_DESC_def_csz_ ((IDL_unsigned_long) 15)
typedef struct DEFINE_DESC_LIST_def_seq_ {
  IDL_unsigned_long _length;
  char pad_to_offset_8_[4];
  IDL_octet *_buffer;
  IDL_PTR_PAD(_buffer, 1)
} DEFINE_DESC_LIST_def;
#define DEFINE_DESC_LIST_def_cin_ ((char *) "c0+H")
#define DEFINE_DESC_LIST_def_csz_ ((IDL_unsigned_long) 4)
typedef IDL_short NSK_PROCESS_HANDLE[10];
#define NSK_PROCESS_HANDLE_cin_ ((char *) "a1+10+I")
#define NSK_PROCESS_HANDLE_csz_ ((IDL_unsigned_long) 7)
struct PROCESS_ID_t {
  char pad_to_offset_4_[2];
  //new phandle changes
#ifdef USE_NEW_PHANDLE
    SB_Phandle_Type pHandle;
    int nodeId;
    int processId;
#else
  NSK_PROCESS_HANDLE pHandle;
  IDL_short nodeId;
  IDL_long processId;
#endif
};
typedef PROCESS_ID_t PROCESS_ID_def;
#define PROCESS_ID_def_cin_ ((char *) "b3+IFa1+10+I")
#define PROCESS_ID_def_csz_ ((IDL_unsigned_long) 12)
struct DATASOURCE_CFG_t {
  SQL_IDENTIFIER_def DSName;
  char pad_to_offset_516_[3];
  IDL_long DSId;
  IDL_short defaultDS;
  char pad_to_offset_524_[2];
  DS_AUTOMATION_def DSAutomation;
  IDL_long MaxSrvrCnt;
  IDL_long AvailSrvrCnt;
  IDL_long InitSrvrCnt;
  IDL_long StartAheadCnt;
  INTERVAL_NUM_def SrvrIdleTimeout;
  INTERVAL_NUM_def ConnIdleTimeout;
  INTERVAL_NUM_def RefreshRate;
  IDL_short Status;
  char pad_to_offset_572_[2];
  IDL_long StatusChangeTime;
  RES_DESC_LIST_def ResDescList;
  ENV_DESC_LIST_def EnvDescList;
  DEFINE_DESC_LIST_def DefineDescList;
};
typedef DATASOURCE_CFG_t DATASOURCE_CFG_def;
#define DATASOURCE_CFG_def_cin_ ((char *) \
    "b16+d512+FIh1+FFFFGGGIFc0+b4+d512+Gd0+Fc0+b3+FFd0+c0+H")
#define DATASOURCE_CFG_def_csz_ ((IDL_unsigned_long) 54)
typedef struct DATASOURCE_CFG_LIST_def_seq_ {
  IDL_unsigned_long _length;
  char pad_to_offset_8_[4];
  DATASOURCE_CFG_def *_buffer;
  IDL_PTR_PAD(_buffer, 1)
} DATASOURCE_CFG_LIST_def;
#define DATASOURCE_CFG_LIST_def_cin_ ((char *) \
    "c0+b16+d512+FIh1+FFFFGGGIFc0+b4+d512+Gd0+Fc0+b3+FFd0+c0+H")
#define DATASOURCE_CFG_LIST_def_csz_ ((IDL_unsigned_long) 57)
/*
 * End translation unit: ODBCSRVRCOMMON
 */
#endif /* ODBCSRVRCOMMON_H_ */
