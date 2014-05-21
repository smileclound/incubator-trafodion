#ifndef STMTDDLALTERTABLEATTRIBUTE_H
#define STMTDDLALTERTABLEATTRIBUTE_H
/* -*-C++-*-
 *****************************************************************************
 *
 * File:         StmtDDLAlterTableAttribute.h
 * Description:  class for Alter Table <table-name> Attribute(s)
 *               DDL statements
 *
 *               The methods in this class are defined either in this
 *               header file or in the source file StmtDDLAlter.C.
 *
 *               
 * Created:      1/15/96
 * Language:     C++
 *
 *
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 1996-2014 Hewlett-Packard Development Company, L.P.
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
 *
 *
 *****************************************************************************
 */


#include "ParDDLFileAttrsAlterTable.h"
#include "StmtDDLAlterTable.h"

// -----------------------------------------------------------------------
// contents of this file
// -----------------------------------------------------------------------
class StmtDDLAlterTableAttribute;

// -----------------------------------------------------------------------
// forward references
// -----------------------------------------------------------------------
// None.

// -----------------------------------------------------------------------
// definition of class StmtDDLAlterTableAttribute
// -----------------------------------------------------------------------
class StmtDDLAlterTableAttribute : public StmtDDLAlterTable
{

public:

  // constructor
  StmtDDLAlterTableAttribute(ElemDDLNode *pFileAttrNode);

  // virtual destructor
  virtual ~StmtDDLAlterTableAttribute();

  // accessor
  inline       ParDDLFileAttrsAlterTable & getFileAttributes();
  inline const ParDDLFileAttrsAlterTable & getFileAttributes() const;

  // cast
  virtual StmtDDLAlterTableAttribute * castToStmtDDLAlterTableAttribute();

  // method for tracing
  virtual NATraceList getDetailInfo() const;
  virtual const NAString getText() const;

private:

  // ---------------------------------------------------------------------
  // private methods
  // ---------------------------------------------------------------------

  void setFileAttributes(ElemDDLFileAttrClause * pFileAttrClause);

        // Copies the information in the specified file
        // attribute clause (pointed to by pFileAttrClause)
        // to data member fileAttributes_ in this object.
        // 
        // This method can only be invoked during the
        // construction of this object when the (file)
        // attributes clause appears.

  // ---------------------------------------------------------------------
  // private data members
  // ---------------------------------------------------------------------

  ParDDLFileAttrsAlterTable fileAttributes_;

}; // class StmtDDLAlterTableAttribute

// -----------------------------------------------------------------------
// definitions of inline methods for class StmtDDLAlterTableAttribute
// -----------------------------------------------------------------------

//
// accessors
//

inline const ParDDLFileAttrsAlterTable &
StmtDDLAlterTableAttribute::getFileAttributes() const
{
  return fileAttributes_;
}

inline ParDDLFileAttrsAlterTable &
StmtDDLAlterTableAttribute::getFileAttributes()
{
  return fileAttributes_;
}

#endif // STMTDDLALTERTABLEATTRIBUTE_H
