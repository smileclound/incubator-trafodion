// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2015 Hewlett-Packard Development Company, L.P.
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

#ifndef _CMP_SEABASE_DDL_INCLUDES_H_
#define _CMP_SEABASE_DDL_INCLUDES_H_

// declaration of the yacc parser and its result
#ifndef   SQLPARSERGLOBALS_CONTEXT_AND_DIAGS
#define   SQLPARSERGLOBALS_CONTEXT_AND_DIAGS
#endif
#ifndef   SQLPARSERGLOBALS_LEX_AND_PARSE
#define   SQLPARSERGLOBALS_LEX_AND_PARSE
#endif

#define   SQLPARSERGLOBALS_FLAGS
#define   SQLPARSERGLOBALS_NADEFAULTS_SET
#include "SqlParserGlobalsCmn.h"

#include "ComObjectName.h"
#include "ComUser.h"

#include "StmtDDLCreateTable.h"
#include "StmtDDLDropTable.h"
#include "StmtDDLAlterTableRename.h"
#include "StmtDDLCreateIndex.h"
#include "StmtDDLPopulateIndex.h"
#include "StmtDDLDropIndex.h"
#include "StmtDDLAlterTableAddColumn.h"
#include "StmtDDLAlterTableDropColumn.h"
#include "StmtDDLAlterTableAlterColumnSetSGOption.h"
#include "StmtDDLAddConstraintPK.h"
#include "StmtDDLAddConstraintRIArray.h"
#include "StmtDDLAddConstraintUniqueArray.h"
#include "StmtDDLGrant.h"
#include "StmtDDLRevoke.h"
#include "StmtDDLDropSchema.h"
#include "StmtDDLRegisterUser.h"
#include "StmtDDLRegisterComponent.h"
#include "StmtDDLCreateView.h"
#include "StmtDDLAlterTableDisableIndex.h"
#include "StmtDDLAlterTableEnableIndex.h"
#include "StmtDDLCreateDropSequence.h"
#include "StmtDDLCreateComponentPrivilege.h"
#include "StmtDDLDropComponentPrivilege.h"
#include "StmtDDLGrantComponentPrivilege.h"
#include "StmtDDLRevokeComponentPrivilege.h"
#include "StmtDDLRegisterComponent.h"
#include "StmtDDLCleanupObjects.h"

#include "ElemDDLHbaseOptions.h"
#include "ElemDDLParamDefArray.h"
#include "ElemDDLParamDef.h"
#include "ElemDDLConstraintPK.h"
#include "StmtDDLDropConstraint.h"
#include "ElemDDLSGOptions.h"

#include "CmpDDLCatErrorCodes.h"

#include "SchemaDB.h"
#include "CmpSeabaseDDL.h"
#include "CmpSeabaseDDLupgrade.h"
#include "CmpDescribe.h"

#include "ExpHbaseInterface.h"

#include "ExExeUtilCli.h"
#include "Generator.h"
#include "desc.h"

#include "ComCextdecs.h"

#endif
