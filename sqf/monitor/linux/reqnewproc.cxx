///////////////////////////////////////////////////////////////////////////////
//
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2012-2014 Hewlett-Packard Development Company, L.P.
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

#include <stdio.h>
#include "reqqueue.h"
#include "montrace.h"
#include "monsonar.h"
#include "monlogging.h"
#include "device.h"
#include "replicate.h"

extern CMonStats *MonStats;
extern CNode *MyNode;
extern CNodeContainer *Nodes;
extern CReplicate Replicator;
extern CDeviceContainer *Devices;

CExtNewProcReq::CExtNewProcReq (reqQueueMsg_t msgType, int pid,
                                struct message_def *msg )
    : CExternalReq(msgType, pid, msg)
{
    // Add eyecatcher sequence as a debugging aid
    memcpy(&eyecatcher_, "RQEI", 4);
}

CExtNewProcReq::~CExtNewProcReq()
{
    // Alter eyecatcher sequence as a debugging aid to identify deleted object
    memcpy(&eyecatcher_, "rqei", 4);
}

void CExtNewProcReq::populateRequestString( void )
{
    char strBuf[MON_STRING_BUF_SIZE/2] = { 0 };

    snprintf( strBuf, sizeof(strBuf), 
              "ExtReq(%s) req #=%ld requester(pid=%d) (name=%s/nid=%d)"
              , CReqQueue::svcReqType[reqType_], getId(), pid_,
              msg_->u.request.u.new_process.process_name,
              msg_->u.request.u.new_process.nid );
    requestString_.assign( strBuf );
}


void CExtNewProcReq::performRequest()
{
    const char method_name[] = "CExtNewProcReq::performRequest";
    TRACE_ENTRY;

    int zone;
    int requester_nid;
    int requester_pid;
    CProcess *process = NULL;
    CNode *node = NULL;
    CLNode *lnode = NULL;
    CLNode *zone_lnode = NULL;
    char la_buf[MON_STRING_BUF_SIZE];
    int result;
    CProcess *requester;
    CProcess *parent;

    // Record statistics (sonar counters)
       MonStats->req_type_newprocess_Incr();

    // Trace info about request
    if (trace_settings &  (TRACE_REQUEST | TRACE_PROCESS))
    {
        char * prog = strrchr(msg_->u.request.u.new_process.program, '/');
        if (prog == NULL)
            prog = msg_->u.request.u.new_process.program;
        else
            ++prog;
        trace_printf("%s@%d request #%ld: create new process on nid=%d, "
                     "name=%s, program=%s, nowait=%d\n",
                     method_name, __LINE__, id_,
                     msg_->u.request.u.new_process.nid,
                     msg_->u.request.u.new_process.process_name,
                     prog, msg_->u.request.u.new_process.nowait);
    }

    requester = MyNode->GetProcess( pid_ );
    if ( requester )
    {
        if ( msg_->u.request.u.new_process.type == ProcessType_SSMP ) 
        {
            if (( msg_->u.request.u.new_process.nid < 0  ||
                  msg_->u.request.u.new_process.nid >= Nodes->NumberLNodes )   )
            {
                // Nid must be specified
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
                // Send reply to requester
                lioreply(msg_, pid_);

                sprintf(la_buf, "[%s], Invalid Node ID (%d).\n", method_name,
                        msg_->u.request.u.new_process.nid);
                mon_log_write(MON_MONITOR_STARTPROCESS_1, SQ_LOG_ERR, la_buf);
                return;
            }
            process = Nodes->GetLNode(msg_->u.request.u.new_process.nid)->GetSSMProc();
            if ( process )
            {
                // 
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
                // Send reply to requester
                lioreply(msg_, pid_);

                sprintf(la_buf, "[%s], SSMP process already exists in logical "
                        "node %d.\n", method_name,
                        msg_->u.request.u.new_process.nid);
                mon_log_write(MON_MONITOR_STARTPROCESS_2, SQ_LOG_ERR, la_buf);
                return;
            }
        }
        if ( msg_->u.request.u.new_process.type == ProcessType_SPX ) 
        {
            if (( msg_->u.request.u.new_process.nid < 0  ||
                  msg_->u.request.u.new_process.nid >= Nodes->NumberLNodes )   )
            {
                // Nid must be specified
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
                // Send reply to requester
                lioreply(msg_, pid_);

                sprintf(la_buf, "[%s], Invalid Node ID (%d).\n", method_name,
                        msg_->u.request.u.new_process.nid);
                mon_log_write(MON_MONITOR_STARTPROCESS_3, SQ_LOG_ERR, la_buf);
                return;
            }
            node = Nodes->GetLNode(msg_->u.request.u.new_process.nid)->GetNode();
            process = node->GetProcessByType(ProcessType_SPX);
            if ( process )
            {
                // 
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
                // Send reply to requester
                lioreply(msg_, pid_);

                sprintf(la_buf, "[%s], SPX process already exists in physical"
                        " node %d.\n", method_name,
                        msg_->u.request.u.new_process.nid);
                mon_log_write(MON_MONITOR_STARTPROCESS_4, SQ_LOG_ERR, la_buf);
                return;
            }
        }
        if (( msg_->u.request.u.new_process.type == ProcessType_TSE ||
              msg_->u.request.u.new_process.type == ProcessType_ASE ) &&
            ( msg_->u.request.u.new_process.backup                  )   )
        {
            parent = requester->GetParent();
            
            if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
            {
                trace_printf( "%s@%d" " - SE Backup parent=%p, pair parent(%d,%d)\n"
                            , method_name, __LINE__, parent
                            , requester->GetPairParentNid()
                            , requester->GetPairParentPid());
            }

            if ( parent &&
                 (requester->GetPairParentNid() != -1 && 
                  requester->GetPairParentPid() != -1) )
            {
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
                // Send reply to requester
                lioreply(msg_, pid_);

                sprintf(la_buf, "[%s], TSE already has a backup process.\n",
                        method_name);
                mon_log_write(MON_MONITOR_STARTPROCESS_14, SQ_LOG_ERR, la_buf);

                return;
            }

            CLogicalDevice *ldev;
            ldev = Devices->GetLogicalDevice( requester->GetName() );
            if (ldev)
            {
                // see if we need to start in the primary or backup fault zone
                // The requester's (current primary process) zone is always the local node.
                zone = MyNode->GetZone();
                if (zone == ldev->GetPrimaryZone())
                {  
                    zone = ldev->GetBackupZone();
                }
                else
                {
                    zone = ldev->GetPrimaryZone();
                }
                if ( zone != -1 )
                {
                    node = Nodes->GetZoneNode( zone );
                    zone_lnode = node->AssignLNode();
                }
                lnode = Nodes->AssignLNode(requester,
                                           msg_->u.request.u.new_process.type,
                                           (zone != -1)?zone_lnode->GetNid():-1,
                                           requester->GetNid());

                if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                {
                    if (lnode)
                    {
                        trace_printf("%s@%d" " - Assigned configured backup node id "  "%d" " for TSE %s" "\n", method_name, __LINE__, lnode->Nid, requester->GetName());
                    }
                    else
                    {
                        trace_printf("%s@%d" " - No node found for TSE "  "%s" "\n", method_name, __LINE__, requester->GetName());
                    }
                }
            }
            else
            {
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
                // Send reply to requester
                lioreply(msg_, pid_);

                sprintf(la_buf, "[%s], Undefined TSE device (%s).\n",
                        method_name, requester->GetName());
                mon_log_write(MON_MONITOR_STARTPROCESS_5, SQ_LOG_ERR, la_buf);

                return;
            }
        }
        else if (msg_->u.request.u.new_process.nid == -1)
        {
            if ( msg_->u.request.u.new_process.type == ProcessType_Backout ||
                 msg_->u.request.u.new_process.type == ProcessType_VolumeRecovery )
            {
                // Use the parent's nid as the preferred nid, 
                // to maintain aligment in the same node
                lnode = Nodes->AssignLNode(requester, msg_->u.request.u.new_process.type,requester->GetNid(),-1);
                if (lnode)
                {
                    if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                        trace_printf("%s@%d" " - Auto assigned node id "  "%d" "\n", method_name, __LINE__, lnode->Nid);
                }
                else
                {
                    if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                        trace_printf("%s@%d" " - No node found" "\n", method_name, __LINE__);
                }
            }
            else if( msg_->u.request.u.new_process.backup )
            {
                lnode = Nodes->AssignLNode(requester, msg_->u.request.u.new_process.type,-1,requester->GetNid());
                if (lnode)
                {
                    if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                        trace_printf("%s@%d" " - Auto assigned backup node id "  "%d" "\n", method_name, __LINE__, lnode->Nid);
                }
                else
                {
                    if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                        trace_printf("%s@%d" " - No node found (backup)" "\n", method_name, __LINE__);
                }
            }
            else
            {
                lnode = Nodes->AssignLNode( requester, msg_->u.request.u.new_process.type,-1,-1 );
                if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                {
                    if (lnode)
                    {
                        trace_printf("%s@%d" " - Auto assigned primary node id "  "%d" "\n", method_name, __LINE__, lnode->Nid);
                    }
                    else
                    {
                        trace_printf("%s@%d" " - No node found  (primary)" "\n", method_name, __LINE__);
                    }
                }
            }
        }
        else if (( msg_->u.request.u.new_process.type == ProcessType_DTM         ) &&
                 (( msg_->u.request.u.new_process.nid < 0                    ) ||
                  ( msg_->u.request.u.new_process.nid >= Nodes->NumberLNodes )   )   )
        {
            msg_->u.reply.type = ReplyType_NewProcess;
            msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
            // Send reply to requester
            lioreply(msg_, pid_);

            sprintf(la_buf, "[%s], Invalid Node ID (%d).\n", method_name,
                    msg_->u.request.u.new_process.nid);
            mon_log_write(MON_MONITOR_STARTPROCESS_6, SQ_LOG_ERR, la_buf);
    
            return;
        }
        else if (( msg_->u.request.u.new_process.type != ProcessType_DTM         ) &&
                 (( msg_->u.request.u.new_process.nid < 0                    ) ||
                  ( msg_->u.request.u.new_process.nid >= Nodes->NumberLNodes )   )   )
        {
            msg_->u.reply.type = ReplyType_NewProcess;
            msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
            // Send reply to requester
            lioreply(msg_, pid_);

            sprintf(la_buf, "[%s], Invalid Node ID (%d).\n", method_name,
                    msg_->u.request.u.new_process.nid);
            mon_log_write(MON_MONITOR_STARTPROCESS_7, SQ_LOG_ERR, la_buf);

            return;
        }
        else
        {
            if( msg_->u.request.u.new_process.backup )
            {
                lnode = Nodes->AssignLNode(requester, msg_->u.request.u.new_process.type, 
                                           msg_->u.request.u.new_process.nid,requester->GetNid());
            }
            else
            {
                lnode = Nodes->AssignLNode(requester, msg_->u.request.u.new_process.type, 
                                           msg_->u.request.u.new_process.nid,-1);
            }
            if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
            {
                if (lnode)
                {
                    trace_printf("%s@%d" " - fault zone balanced, zid in="  "%d" ", nid out=" "%d" "\n", method_name, __LINE__, msg_->u.request.u.new_process.nid, lnode->Nid);
                }
                else
                {
                    trace_printf("%s@%d" " - No node found " "\n", method_name, __LINE__);
                }
            }
        }

        // Check if argc is within limits
        if (msg_->u.request.u.new_process.argc > MAX_ARGS)
        {
            msg_->u.reply.type = ReplyType_NewProcess;
            msg_->u.reply.u.new_process.return_code = MPI_ERR_ARG;
            // Send reply to requester
            lioreply(msg_, pid_);

            sprintf(la_buf, "[%s], Invalid number of arguments (%d).\n",
                    method_name, msg_->u.request.u.new_process.argc);
            mon_log_write(MON_MONITOR_STARTPROCESS_9, SQ_LOG_ERR, la_buf);

            return;
        }

        int programLen = strlen(msg_->u.request.u.new_process.program);
        if ( programLen < 0 || programLen  >= MAX_PROCESS_PATH )
        {
            msg_->u.reply.type = ReplyType_NewProcess;
            msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
            // Send reply to requester
            lioreply(msg_, pid_);

            sprintf(la_buf, "[%s], Invalid Program file, len=%d\n",
                    method_name, programLen);
            mon_log_write(MON_MONITOR_STARTPROCESS_10, SQ_LOG_ERR, la_buf);

            return;
        }
    
        // Check if node is available and within limits
        if ( !lnode || lnode->GetState() != State_Up)
        {
            msg_->u.reply.type = ReplyType_NewProcess;
            msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
            // Send reply to requester
            lioreply(msg_, pid_);
    
            sprintf(la_buf, "[%s], Unsuccessful, node is not up (%d)\n",
                    method_name, (lnode ? lnode->GetNid(): -1));
            mon_log_write(MON_MONITOR_STARTPROCESS_11, SQ_LOG_ERR, la_buf);

            return;
        }
    
        if (lnode->GetNumProcs() < MAX_PROCESSES)
        {
            strId_t pathStrId = MyNode->GetStringId ( msg_->u.request.u.new_process.path );
            strId_t ldpathStrId = MyNode->GetStringId (msg_->u.request.u.new_process.ldpath );
            strId_t programStrId = MyNode->GetStringId ( msg_->u.request.u.new_process.program );

            if (MyNode->IsMyNode(lnode->Nid))
            {
                node = MyNode;
                process = node->CreateProcess ( requester,
                                                lnode->GetNid(),
                                                msg_->u.request.u.new_process.type,
                                                msg_->u.request.u.new_process.debug,
                                                msg_->u.request.u.new_process.priority,
                                                msg_->u.request.u.new_process.backup,
                                                msg_->u.request.u.new_process.unhooked,
                                                msg_->u.request.u.new_process.process_name,
                                                pathStrId,
                                                ldpathStrId,
                                                programStrId,
                                                msg_->u.request.u.new_process.infile,
                                                msg_->u.request.u.new_process.outfile
                                                , result
                                                );
                if ( process )
                {
                    process->userArgs (  msg_->u.request.u.new_process.argc,
                                         msg_->u.request.u.new_process.argv );
                }
                if ( process && process->Create(process->GetParent(), result))
                {
                    MyNode->AddToNameMap(process);
                    MyNode->AddToPidMap(process->GetPid(), process);
                }
            }
            else
            {
                if (requester)
                {
                    requester_nid = requester->GetNid();
                    requester_pid = requester->GetPid();
                    if (msg_->u.request.u.new_process.backup && 
                        (requester->GetPairParentNid() == -1 && 
                         requester->GetPairParentPid() == -1))
                    {
                        // preserve the true parent of a process pair in the first 
                        // primary only
                        requester->SetPairParentNid ( requester->GetParentNid() );
                        requester->SetPairParentPid ( requester->GetParentPid() );
                    }
                }
                else
                {
                    requester_nid = requester_pid = -1;
                }
                node = lnode->GetNode();
                process = node->CloneProcess (lnode->GetNid(), 
                                              msg_->u.request.u.new_process.type,
                                              msg_->u.request.u.new_process.priority,
                                              msg_->u.request.u.new_process.backup,
                                              msg_->u.request.u.new_process.unhooked,
                                              msg_->u.request.u.new_process.process_name, 
                                              (char *) "", 
                                              -1, 
                                              requester_nid,
                                              requester_pid, 
                                              false,
                                              false,
                                              pathStrId,
                                              ldpathStrId,
                                              programStrId,
                                              msg_->u.request.u.new_process.infile,
                                              msg_->u.request.u.new_process.outfile,
                                              NULL);
                if (process)
                {
                    process->userArgs (  msg_->u.request.u.new_process.argc,
                                         msg_->u.request.u.new_process.argv );
                    // Replicate the process to other nodes
                    CReplProcess *repl = new CReplProcess(process);
                    Replicator.addItem(repl);
                }
            }
            if (process)
            {
                // Save the caller's reply tag so we can reply later.
                process->SetReplyTag ( msg_->reply_tag );
                process->SetNowait ( msg_->u.request.u.new_process.nowait );
                process->SetTag ( msg_->u.request.u.new_process.tag );
                if ( process->IsNowait() )
                {
                    // we will reply here, but when the process has be created, replicated and completed
                    // we will send a notice to the parent indicating that the startup has completed.
                    msg_->u.reply.type = ReplyType_NewProcess;
                    msg_->u.reply.u.new_process.nid = process->GetNid();
                    msg_->u.reply.u.new_process.pid = process->GetPid();
                    strcpy(msg_->u.reply.u.new_process.process_name,process->GetName());
                    msg_->u.reply.u.new_process.return_code = MPI_SUCCESS;
                }
#ifdef QUICK_WAITED_NEWPROCESS_REPLY
                else if (process->GetPid() != -1)
                {   // Process was created locally, reply now.  The process
                    // was created but the process startup message has not yet
                    // arrived.
                    msg_->u.reply.type = ReplyType_NewProcess;
                    msg_->u.reply.u.new_process.nid = process->GetNid();
                    msg_->u.reply.u.new_process.pid = process->GetPid();
                    strcpy(msg_->u.reply.u.new_process.process_name,process->GetName());
                    msg_->u.reply.u.new_process.return_code = MPI_SUCCESS;
                }
#endif
                else
                {
                    // we will not reply at this time ... but wait for the child process to 
                    // startup and sent its completion message.  Then we will reply to the parent.

                    // Retain reference to parent's request buffer so can
                    // send completion message.
                    process->parentContext( msg_ );

                    msg_->noreply = true;
                }
                if (trace_settings & (TRACE_REQUEST | TRACE_PROCESS))
                    trace_printf("%s@%d" " - Successful" "\n", method_name, __LINE__);
            }
            else
            {
                msg_->u.reply.type = ReplyType_NewProcess;
                msg_->u.reply.u.new_process.return_code = MPI_ERR_SPAWN;
           
                sprintf(la_buf, "[%s], Unsuccessful!\n", method_name);
                mon_log_write(MON_MONITOR_STARTPROCESS_12, SQ_LOG_CRIT, la_buf);
             
            }
        }
        else
        {
            msg_->u.reply.type = ReplyType_NewProcess;
            msg_->u.reply.u.new_process.return_code = MPI_ERR_NO_MEM;
           
            sprintf(la_buf, "[%s], Unsuccessful, MAX_PROCESSES limit!\n",
                    method_name);
            mon_log_write(MON_MONITOR_STARTPROCESS_13, SQ_LOG_ERR, la_buf);
           
        }

        // Send reply to requester
        lioreply(msg_, pid_);
    }
    else
    {   // Reply to requester so it can release the buffer.  
        // We don't know about this process.
        errorReply( MPI_ERR_EXITED );
    }

    TRACE_EXIT;
}
