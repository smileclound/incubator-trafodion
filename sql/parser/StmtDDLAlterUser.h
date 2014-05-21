#ifndef STMTDDLALTERUSER_H
#define STMTDDLALTERUSER_H
//******************************************************************************
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2011-2014 Hewlett-Packard Development Company, L.P.
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
//******************************************************************************
/* -*-C++-*-
 *****************************************************************************
 *
 * File:         StmtDDLAlterUser.h
 * Description:  class for parse nodes representing alter user statements
 *                 
 *
 * Created:      May 27, 2011     
 * Language:     C++
 *
 *****************************************************************************
 */

#include "ComLocationNames.h"
#include "ElemDDLLocation.h"
#include "ComSmallDefs.h"
#include "StmtDDLNode.h"

// -----------------------------------------------------------------------
// contents of this file
// -----------------------------------------------------------------------
class StmtDDLRegisterUser;

// -----------------------------------------------------------------------
// forward references
// -----------------------------------------------------------------------
// None

// -----------------------------------------------------------------------
// Alter user statement 
// -----------------------------------------------------------------------
class StmtDDLAlterUser : public StmtDDLNode
{

public:

enum AlterUserCmdSubType { 
   SET_LOGON_ROLE = 2,
   SET_EXTERNAL_NAME,
   SET_IS_VALID_USER,
   SET_AUTH_TYPE,
   SET_IMMUTABLE,
   RESET_IMMUTABLE
};


   StmtDDLAlterUser(
      const NAString        & databaseUsername,
      AlterUserCmdSubType     cmdSubType,
      const NAString        * pExternalName,
      const NAString        * pLogonRole,
      NABoolean               isValidUser,
      ComAuthenticationType   authType,
      CollHeap              * heap);

   virtual ~StmtDDLAlterUser();

   virtual StmtDDLAlterUser * castToStmtDDLAlterUser();

// for binding
   ExprNode * bindNode(BindWA *bindWAPtr);

// accessors

   inline const NAString & getExternalUsername() const;
   inline const NAString & getDatabaseUsername() const;
   inline const NAString & getLogonRole() const;
   inline NABoolean isValidUser() const;
   inline AlterUserCmdSubType getAlterUserCmdSubType() const;
   inline ComAuthenticationType getAuthenticationType() const;

// for tracing

  virtual const NAString displayLabel1() const;
  virtual const NAString displayLabel2() const;
  virtual const NAString getText() const;
  
private:

// ---------------------------------------------------------------------
// private data members
// ---------------------------------------------------------------------

NAString externalUserName_;
NAString databaseUserName_;
NAString logonRole_;
AlterUserCmdSubType alterUserCmdSubType_;
NABoolean isValidUser_;
ComAuthenticationType authenticationType_;  
}; // class StmtDDLAlterUser

// -----------------------------------------------------------------------
// definitions of inline methods for class StmtDDLAlterUser
// -----------------------------------------------------------------------

//
// accessors
//

inline const NAString &
StmtDDLAlterUser::getExternalUsername() const
{
  return externalUserName_;
}

inline const NAString &
StmtDDLAlterUser::getDatabaseUsername() const
{
  return databaseUserName_;
}
inline const NAString &
StmtDDLAlterUser::getLogonRole() const
{
  return logonRole_;
}

inline NABoolean
StmtDDLAlterUser::isValidUser() const
{
  return isValidUser_;
}
 
inline StmtDDLAlterUser::AlterUserCmdSubType 
StmtDDLAlterUser::getAlterUserCmdSubType() const
{
  return alterUserCmdSubType_;
}

inline ComAuthenticationType
StmtDDLAlterUser::getAuthenticationType() const
{
  return authenticationType_;
}

#endif // STMTDDLALTERUSER_H
