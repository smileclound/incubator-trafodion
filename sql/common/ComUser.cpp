/* -*-C++-*-
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2014 Hewlett-Packard Development Company, L.P.
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

 *****************************************************************************
 *
 * File:         ComUser
 * Description:  Implements methods for user management
 *
 * Contains methods for classes:
 *   ComUser
 *   ComUserVerifyObj
 *   ComUserVerifyAuth
 *
 *****************************************************************************
 */

#define   SQLPARSERGLOBALS_FLAGS	// must precede all #include's
#define   SQLPARSERGLOBALS_NADEFAULTS

#include "ComUser.h"
#include "CmpSeabaseDDL.h"

#include "sqlcli.h"
#include "SQLCLIdev.h"
#include "ExExeUtilCli.h"

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Class ComUser methods
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ----------------------------------------------------------------------------
// Constructors
// ----------------------------------------------------------------------------
ComUser::ComUser()
{ }

// ----------------------------------------------------------------------------
// method:  getCurrentUser
//
// Returns currentUser from the Cli context
// ----------------------------------------------------------------------------
Int32 ComUser::getCurrentUser(void)
{
  Int32 dbUserID = 0;
  Int32 rc = 0;

  SQL_EXEC_ClearDiagnostics(NULL);

  rc = SQL_EXEC_GetSessionAttr(SESSION_DATABASE_USER_ID,
                               &dbUserID,
                               NULL, 0, NULL);

  assert(rc >= 0);
  assert(dbUserID >= SUPER_USER)

  return dbUserID;
}

// ----------------------------------------------------------------------------
// method:  getSessionUser
//
// Returns sessionUser from the Cli context
// ----------------------------------------------------------------------------
Int32 ComUser::getSessionUser(void)
{
  Int32 dbUserID = 0;
  Int32 rc = 0;

  SQL_EXEC_ClearDiagnostics(NULL);

  rc = SQL_EXEC_GetSessionAttr(SESSION_DATABASE_USER_ID,
                               &dbUserID,
                               NULL, 0, NULL);

  assert(rc >= 0);
  assert(dbUserID >= SUPER_USER);

  return dbUserID;
}

// ----------------------------------------------------------------------------
// method: getEffectiveUser
//
// This method returns the effective user for the session.
// If this if the first time, the effectiveUser_ member is also set.
// In most cases the effective user is the same as the session user
// If the user is the system owner (DB__ROOT) or has system owner privileges,
// and this is a create operation, then the effective user is the schema
// owner.
//
// We may need to change this when definer and invoker rights are enabled
//
// input:  VerifyAction - describes what operation the user desires
// ----------------------------------------------------------------------------
Int32 ComUser::getEffectiveUser(VerifyAction act)
{
  return (getSessionUser() == SUPER_USER && belongsToCreateGroup(act))
             ? getSchemaOwner() : getSessionUser();
}

// ----------------------------------------------------------------------------
// method:  isRootUserID
//
// Returns true if passed in userID is the super user
// ----------------------------------------------------------------------------
bool ComUser::isRootUserID(Int32 userID)
{
  return (userID == SUPER_USER) ? true : false;
}

// ----------------------------------------------------------------------------
// method: getAuthType
//
// returns the type of authorization ID given the passed in authID
// ----------------------------------------------------------------------------
char ComUser::getAuthType(Int32 authID)
{

   if (authID == PUBLIC_USER)
      return AUTH_PUBLIC;

   if (authID == SYSTEM_USER)
      return AUTH_SYSTEM;

   if (authID > SUPER_USER && authID < MAX_USERID)
      return AUTH_USER;

   if (authID >= MAX_USERID && authID <= MAX_ROLEID)
      return AUTH_ROLE;
   return AUTH_UNKNOWN;
}

// -----------------------------------------------------------------------
// getUserNameFromUserID
//
// Reads the USERS table to get the user name associated with the passed
// in userID
//
//   <userID>                        Int32                           In
//      is the numeric ID to be mapped to a name.
//
//   <userName>                      char *                          In
//      passes back the name that the numeric ID mapped to.  If the ID does
//      not map to a name, the ASCII equivalent of the ID is passed back.
//
//   <maxLen>                        Int32                           In
//      is the size of <authName>.
//
//   <actualLen>                     Int32 &                         Out
//      is the size of the auth name in the table.  If larger than <maxLen>,
//      caller needs to supply a larger buffer.
//
// Returns:
//   FEOK          -- Found. User name written to userName. Length
//                    returned in actualLen.
//   FENOTFOUND    -- Not found
//   FEBUFTOOSMALL -- Found but output buffer is too small. Required
//                    length returned in actualLen.
//   Other         -- Unexpected error
// -----------------------------------------------------------------------
Int16 ComUser::getUserNameFromUserID(Int32 userID,
                                     char *userName,
                                     Int32 maxLen,
                                     Int32 &actualLen)
{
  Int16 result = FEOK;
  actualLen = 0;

  Int16 retcode = SQL_EXEC_GetDatabaseUserName_Internal(userID,
                                                        userName,
                                                        maxLen,
                                                        &actualLen);

  if (retcode < 0)
  {
    if (actualLen > 0)
      result = FEBUFTOOSMALL;
    else
      result = FENOTFOUND;
  }

  if (retcode != 0)
    SQL_EXEC_ClearDiagnostics(NULL);

  // On success, CLI does not return the length of the string in
  // actualLen. This function however is supposed to return a value in
  // actualLen even when the mapping is successful. The value returned
  // should not account for a null terminator.
  if (result == FEOK)
    actualLen = strlen(userName);
  return result;
}

// ----------------------------------------------------------------------------
// method: getUserIDFromUserName
//
// Reads the AUTHS table to get the userID associated with the passed
// in user name
//
//  Returns:  FEOK -- found
//            FENOTFOUND -- user not defined
//            other -- unexpected error
// ----------------------------------------------------------------------------
Int16 ComUser::getUserIDFromUserName(const char *userName, Int32 &userID)
{
  Int16 result = FEOK;

  Int16 retcode = SQL_EXEC_GetDatabaseUserID_Internal((char *) userName,
                                                      &userID);

  if (retcode < 0)
    result = FENOTFOUND;

  if (retcode != 0)
    SQL_EXEC_ClearDiagnostics(NULL);

  return result;
}


// ----------------------------------------------------------------------------
//  method:      getAuthNameFromAuthID
//
//  Maps an integer authentication ID to a name.  If the number cannot be
//  mapped to a name, the numeric ID is converted to a string.
//
//   <authID>                        Int32                           In
//      is the numeric ID to be mapped to a name.
//
//   <authName>                      char *                          In
//      passes back the name that the numeric ID mapped to.  If the ID does
//      not map to a name, the ASCII equivalent of the ID is passed back.
//
//   <maxLen>                        Int32                           In
//      is the size of <authName>.
//
//   <actualLen>                     Int32 &                         Out
//      is the size of the auth name in the table.  If larger than <maxLen>,
//      caller needs to supply a larger buffer.
//
//   Returns: Int16
//
//     FEOK          -- Found. User name written to userName. Length
//                      returned in actualLen.
//     FENOTFOUND    -- Not found
//     FEBUFTOOSMALL -- Found but output buffer is too small. Required
//                      length returned in actualLen.
//     Other         -- Unexpected error
// ----------------------------------------------------------------------------
Int16 ComUser::getAuthNameFromAuthID(Int32   authID,
                                     char  * authName,
                                     Int32   maxLen,
                                     Int32 & actualLen)

{
  actualLen = 0;
  int retcode = SQL_EXEC_GetAuthName_Internal(authID,authName,maxLen,actualLen);

  if (retcode < 0)  // ERROR == -1
  {
    if (actualLen > 0)
      return FEBUFTOOSMALL;

    return FENOTFOUND; //Should not happen
  }

  if (retcode != 0)
    SQL_EXEC_ClearDiagnostics(NULL);

  return FEOK;
}

// ----------------------------------------------------------------------------
// method: userHasPriv
//
// virtual method - always returns false
// ----------------------------------------------------------------------------
bool ComUser::userHasPriv(VerifyAction act)
{
  return false;
}

// ----------------------------------------------------------------------------
// method: isAuthorized
//
// virtual method - always returns false
// ----------------------------------------------------------------------------
bool ComUser::isAuthorized(VerifyAction act,
                           bool trustedCaller)
{
  return false;
}


// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Class ComUserVerifyObj methods
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ----------------------------------------------------------------------------
// Constructors
// ----------------------------------------------------------------------------
ComUserVerifyObj::ComUserVerifyObj()
: objType_(UNKNOWN_OBJ_TYPE)
, ComUser()
{}

ComUserVerifyObj::ComUserVerifyObj(const ComObjectName &objName,
                                   ComUserVerifyObj::ObjType objType)
: objType_ (objType)
, objName_ (objName)
, ComUser()
{}

// ----------------------------------------------------------------------------
// method:  isAuthorized
//
// This method returns true if the current session user can perform requested
// operation, false otherwise
//
// input:  VerifyAction  - describes what operation the user desires
//         trustedCaller - indicates that this call is made by the system,
// ----------------------------------------------------------------------------
bool ComUserVerifyObj::isAuthorized(ComUser::VerifyAction act,
                                    bool trustedCaller)
{
  // If the request is for a metadata table and the call
  // is from a trusted caller, return true
  const NAString catName = getObjName().getCatalogNamePartAsAnsiString();
  const NAString schName = getObjName().getSchemaNamePartAsAnsiString(true);
  const NAString objName = getObjName().getObjectNamePartAsAnsiString(true);

  if (CmpSeabaseDDL::isSeabaseMD(catName, schName, objName))
    return trustedCaller ? true : false;

  // For now, just return true
  return true;
#if 0
  // Get the owner of the object
  Int32 objOwner = getEffectiveUser(act);

  // For create operations, the object does not exist so there is no owner
  // otherwise, get the owner from the OBJECTS table
  if (!belongsToCreateGroup(act))
  {
    // TBD: check to see if the object exists in the NATable structure, if so
    // get the objectOwner from there instead of doing a separate I/O.
    ExeCliInterface cliInterface(STMTHEAP);
    CmpSeabaseDDL ddlClass(STMTHEAP);
    Int32 retcode = ddlClass.getObjectOwner(&cliInterface,
                                            catName.data(),
                                            schName.data(),
                                            objName.data(),
                                            &objOwner);

    // -1 is returned if object owner could not be extracted, 0 otherwise
    if (retcode < 0)
      return false;
  }

  // If user owns object via the object, schema, or other; return true
  if (userOwnsObject(act, objOwner))
    return true;

  // If user has been granted the privilege, return true
  return (userHasPriv(act));
#endif
}

// ----------------------------------------------------------------------------
// method: userOwnsObject
//
// returns true is the user is owner either because they own the object,
// they own the schema, or they own the system (DB__ROOT).
//
// input:  VerifyAction  - describes what operation the user desires
//         objOwner - specifies the object owner ID
// ----------------------------------------------------------------------------
bool ComUserVerifyObj::userOwnsObject(VerifyAction act,
                                      Int32 objOwner)
{
  if (getSessionUser() == SUPER_USER )
    return true;
  if (getSessionUser() == getSchemaOwner())
    return true;
  if (objOwner == getSessionUser())
      return true;
  return false;
}

// ----------------------------------------------------------------------------
// method: userHasPriv
//
// input:  VerifyAction - describes what operation the user desires
//
// Until privileges are added, just return false
// ---------------------------------------------------------------------------
bool ComUserVerifyObj::userHasPriv(VerifyAction act)
{
  return false;
}

// ----------------------------------------------------------------------------
// method: belongsToCreateGroup
//
// This method returns true if the VerifyAction is CREATE, false otherwise
//
// input:  VerifyAction - describes what operation the user desires
// ----------------------------------------------------------------------------
bool ComUser::belongsToCreateGroup(ComUser::VerifyAction act)
{
  return (   (act == ComUser::CREATE)
          || (act == ComUser::CREATE_TABLE)
          || (act == ComUser::CREATE_VIEW)
          || (act == ComUser::CREATE_LIBRARY)
          || (act == ComUser::CREATE_TRIGGER)
          || (act == ComUser::CREATE_PROCEDURE)
          || (act == ComUser::CREATE_ROUTINE)
          || (act == ComUser::CREATE_ROUTINE_ACTION)
          || (act == ComUser::CREATE_SYNONYM)
         );
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Class ComUserVerifyAuth methods
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ----------------------------------------------------------------------------
// Constructors
// ----------------------------------------------------------------------------
ComUserVerifyAuth::ComUserVerifyAuth()
: authType_(UNKNOWN_AUTH_TYPE)
, ComUser()
{}

ComUserVerifyAuth::ComUserVerifyAuth(const NAString &authName,
                                     ComUserVerifyAuth::AuthType authType)
: authType_ (authType)
, authName_ (authName)
, ComUser()
{}

// ----------------------------------------------------------------------------
// method: isAuthorized
//
// This method returns true if the current session user can perform requested
// operation, false otherwise
//
// input:  VerifyAction  - describes what operation the user desires
//         trustedCaller - indicates that this call is made by the system,
//                         it is similar to isCatman from SeaQuest
// ---------------------------------------------------------------------------
bool ComUserVerifyAuth::isAuthorized(ComUser::VerifyAction act,
                                     bool trustedCaller)
{
  // just return true for now
  return true;
}

// ----------------------------------------------------------------------------
// method: userHasPriv
//
// input:  VerifyAction - describes what operation the user desires
//
// Until privileges are added, just return false
// ---------------------------------------------------------------------------
bool ComUserVerifyAuth::userHasPriv(VerifyAction act)
{
  return false;
}
