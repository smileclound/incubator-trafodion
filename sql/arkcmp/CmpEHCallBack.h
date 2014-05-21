/**********************************************************************
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 1995-2014 Hewlett-Packard Development Company, L.P.
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
**********************************************************************/
#ifndef _CMP_EH_CALL_BACK_H_
#define _CMP_EH_CALL_BACK_H_
/* -*-C++-*-
 *****************************************************************************
 *
 * File:         CmpEHCallBack.cpp
 * Description:  Compiler Call back functions for exception handling
 *               
 *               
 * Created:      5/4/02
 * Language:     C++
 *
 *
 *
 *
 *****************************************************************************
 */
#include "Platform.h"
#include "EHCallBack.h"

class CmpEHCallBack : public EHCallBack {
public:
  virtual void doFFDC();
  virtual void dumpDiags();
};


void makeTFDSCall(const char *msg, const char *file, UInt32 line);


void printSignalHandlers();

#endif





