/**********************************************************************
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 1998-2014 Hewlett-Packard Development Company, L.P.
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
/* -*-C++-*-
****************************************************************************
*
* File:         SequenceGeneratorAttributes.h        
* Description:  The attributes of the sequence generator
* Created:      4/22/08
* Language:     C++
*
****************************************************************************/
 
#ifndef SEQUENCEGENERATORATTRIBUTES_H
#define SEQUENCEGENERATORATTRIBUTES_H

#include "ComSmallDefs.h"

// -----------------------------------------------------------------------
// contents of this file
// -----------------------------------------------------------------------
class SequenceGeneratorAttributes;

// ***********************************************************************
// SequenceGeneratorAttributes contains all the attributes of a
// sequence generator
// ***********************************************************************
#pragma nowarn(1506)   // warning elimination 
class SequenceGeneratorAttributes : public NABasicObject
{
public:
  // ---------------------------------------------------------------------
  // Constructor functions
  // ---------------------------------------------------------------------
  SequenceGeneratorAttributes(const Int64                     psgStartValue,
                              const Int64                     psgIncrement,
                              const Int64                     psgMaxValue,
                              const Int64                     psgMinValue,                    
                              const ComSequenceGeneratorType  psgSGType,
                              const ComSQLDataType            psgSQLDataType,
                              const ComFSDataType             psgFSDataType,
                              const NABoolean                 psgCycleOption,
			      const ComUID                    psgObjectUID,
                              CollHeap * h=0)
              : sgStartValue_(psgStartValue),
	        sgIncrement_(psgIncrement),
                sgMaxValue_(psgMaxValue),
                sgMinValue_(psgMinValue),
                sgSGType_(psgSGType),
                sgSQLDataType_(psgSQLDataType),
                sgFSDataType_(psgFSDataType),
                sgCycleOption_(psgCycleOption),
		sgObjectUID_(psgObjectUID)
  {}

      
  SequenceGeneratorAttributes(CollHeap * h=0)
              : sgStartValue_(0),
                sgIncrement_(0),
                sgMaxValue_(0),
                sgMinValue_(0),
                sgSGType_(COM_UNKNOWN_SG),
                sgSQLDataType_(COM_UNKNOWN_SDT),
                sgFSDataType_(COM_UNKNOWN_FSDT),
                sgCycleOption_(FALSE),
		sgObjectUID_(0)
  {}

  // copy ctor
  SequenceGeneratorAttributes (const SequenceGeneratorAttributes & sga, CollHeap * h=0) 
       : 
    sgStartValue_(sga.sgStartValue_),
    sgIncrement_(sga.sgIncrement_),
    sgMaxValue_(sga.sgMaxValue_),
    sgMinValue_(sga.sgMinValue_),
    sgSGType_(sga.sgSGType_),
    sgSQLDataType_(sga.sgSQLDataType_),
    sgFSDataType_(sga.sgFSDataType_),
    sgCycleOption_(sga.sgCycleOption_),
    sgObjectUID_(sga.sgObjectUID_)
  {}

  // ---------------------------------------------------------------------
  // Sequence generator functions
  // ---------------------------------------------------------------------

  const Int64                      &getSGStartValue()   const     { return sgStartValue_; }
  const Int64                      &getSGIncrement()    const     { return sgIncrement_; }
  const Int64                      &getSGMaxValue()     const     { return sgMaxValue_; }
  const Int64                      &getSGMinValue()     const     { return sgMinValue_; }
  const ComSequenceGeneratorType   &getSGSGType()       const     { return sgSGType_; }
  const ComSQLDataType             &getSGSQLDataType()  const     { return sgSQLDataType_; }
  const ComFSDataType              &getSGFSDataType()   const     { return sgFSDataType_; }
  const NABoolean                   getSGCycleOption()  const	  { return sgCycleOption_; }
  const ComUID                     &getSGObjectUID()    const     { return sgObjectUID_; }
  
  void setSGStartValue(const Int64 psgStartValue)
  { sgStartValue_= psgStartValue; }

  void setSGIncrement(const Int64 psgIncrement)
  { sgIncrement_ = psgIncrement; }

  void setSGMaxValue(const Int64 psgMaxValue)
  { sgMaxValue_ = psgMaxValue; }

  void setSGMinValue(const Int64 psgMinValue)
  { sgMinValue_ = psgMinValue; }

  void setSGSGType(const ComSequenceGeneratorType psgSGType)
  { sgSGType_ = psgSGType; }

  void setSGSQLDataType(const ComSQLDataType psgSQLDataType)
  { sgSQLDataType_ = psgSQLDataType; }

  void setSGFSDataType(const ComFSDataType psgFSDataType)
  { sgFSDataType_ = psgFSDataType; }

 void  setSGCycleOption(const NABoolean psgCycleOption)
  { sgCycleOption_ = psgCycleOption; }

 void setSGObjectUID(const ComUID psgObjectUID)
  { sgObjectUID_ = psgObjectUID; }
  
private:

  // Sequence generator

  Int64                     sgStartValue_;
  Int64                     sgIncrement_;
  Int64                     sgMaxValue_;
  Int64                     sgMinValue_;
  ComSequenceGeneratorType  sgSGType_;
  ComSQLDataType            sgSQLDataType_;
  ComFSDataType             sgFSDataType_;
  NABoolean                 sgCycleOption_;
  ComUID                    sgObjectUID_;

}; // class SequenceGeneratorAttributes
#pragma warn(1506)  // warning elimination 

#endif  /* SEQUENCEGENERATORATTRIBUTES_H */
