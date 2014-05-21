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

#include <iostream>

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

#include "localio.h"
#include "mlio.h"
#include "monlogging.h"
#include "monsonar.h"
#include "montrace.h"
#include "redirector.h"
#include "healthcheck.h"
#include "config.h"
#include "device.h"
#include "cluster.h"
#include "monitor.h"

#include "replicate.h"

#include "clusterconf.h"
#include "lnode.h"
#include "pnode.h"
#include "reqqueue.h"

extern bool IAmIntegrating;
extern bool IAmIntegrated;
extern bool IsRealCluster;
extern char IntegratingMonitorPort[MPI_MAX_PORT_NAME];
extern char MyPort[MPI_MAX_PORT_NAME];
extern bool SMSIntegrating;
extern int CreatorShellPid;

extern int MyPNID;
#include <netdb.h>
#include <sys/epoll.h>
static int *socks_ = 0;
static int *sockPorts_ = 0;
static int serverSock = -1;
static int serverSockPort = -1;
static int epollFD = -1;
static void InitClusterSocks(int worldSize, int myRank, char *nodeNames);
static int mksrvsock(int *pport);
static int mkcltsock(unsigned char srcip[4], unsigned char dstip[4], int port);
static int acceptsock(int sock);

extern CReqQueue ReqQueue;
extern char Node_name[MPI_MAX_PROCESSOR_NAME];

extern CMonitor *Monitor;
extern CNodeContainer *Nodes;
extern CConfigContainer *Config;
extern CDeviceContainer *Devices;
extern CNode *MyNode;
extern CMonStats *MonStats;
extern CRedirector Redirector;
extern CMonLog *MonLog;
extern CHealthCheck HealthCheck;

extern long next_test_delay;
extern CReplicate Replicator;

extern char *ErrorMsg (int error_code);

const char *JoiningPhaseString( JOINING_PHASE phase);
const char *StateString( STATE state);
const char *SyncStateString( SyncState state);

void CCluster::ActivateSpare( CNode *spareNode, CNode *downNode, bool checkHealth )
{
    const char method_name[] = "CCluster::ActivateSpare";
    TRACE_ENTRY;
    // if not checking health, assume the spare is healthy
    bool spareHealthy = checkHealth ? false : true; 
    int tmCount = 0;
    CNode *node;
    CLNode *lnode;

    if (trace_settings & TRACE_INIT)
    {
        trace_printf( "%s@%d - pnid=%d, name=%s (%s) is taking over pnid=%d, name=%s (%s), check health=%d, isIntegrating=%d , integrating pnid=%d\n"
                    , method_name, __LINE__
                    , spareNode->GetPNid(), spareNode->GetName(), StateString(spareNode->GetState())
                    , downNode->GetPNid(), downNode->GetName(), StateString(downNode->GetState())
                    , checkHealth, IsIntegrating(), integratingPNid_ );
    }

    if ( checkHealth )
    {
        // TODO: Execute physical node health check script here
        spareHealthy = true;
        if ( !spareHealthy )
        {
            // and tell the cluster the node is down, since the spare can't takeover
            CReplNodeDown *repl = new CReplNodeDown(downNode->GetPNid());
            Replicator.addItem(repl);
        }
    }
    
    if ( spareHealthy )
    {
        if ( downNode->GetPNid() != spareNode->GetPNid() )
        {
            // Move down node's logical nodes to spare node
            downNode->MoveLNodes( spareNode );
    
            spareNode->SetPhase( Phase_Activating );

            if ( !IsIntegrating() )
            {
                downNode->SetState( State_Down ); 
            
                // Send process death notices
                spareNode->KillAllDown();
    
                // Send node down notice
                lnode = spareNode->GetFirstLNode();
                for ( ; lnode; lnode = lnode->GetNext() )
                {
                    // Watchdog process clone was removed in KillAllDown
                    lnode->Down();
                }
            }
        }

        // Any DTMs running?
        for ( int i=0; !tmCount && i < Nodes->GetNodesCount(); i++ )
        {
            node = Nodes->GetNode( i );
            lnode = node->GetFirstLNode();
            for ( ; lnode; lnode = lnode->GetNext() )
            {
                CProcess *process = lnode->GetProcessLByType( ProcessType_DTM );
                if ( process  ) tmCount++;
            }
        }
        
        spareNode->ResetSpareNode();
    
        // Create Watchdog and PSD processes if this node is the activating spare
        if ( spareNode->GetPNid() == MyPNID )
        {
            Monitor->StartPrimitiveProcesses();
        }
        else
        {
            // Check for end of joining phase on node re-integration
            if ( spareNode->GetState() == State_Joining )
            {
                spareNode->SetState( State_Up );
            }
            if ( tmCount )
            {
                // Send node prepare notice to local DTM processes
                lnode = spareNode->GetFirstLNode();
                for ( ; lnode; lnode = lnode->GetNext() )
                {
                    lnode->PrepareForTransactions( downNode->GetPNid() != spareNode->GetPNid() );
                }
            }
        }

        if ( downNode->GetPNid() != spareNode->GetPNid() )
        {
            // we need to abort any active TmSync
            if (( MyNode->GetTmSyncState() == SyncState_Start    ) ||
                ( MyNode->GetTmSyncState() == SyncState_Continue ) ||
                ( MyNode->GetTmSyncState() == SyncState_Commit   )   )
            {
                MyNode->SetTmSyncState( SyncState_Abort );
                Monitor->SetAbortPendingTmSync();
                if (trace_settings & (TRACE_RECOVERY | TRACE_REQUEST | TRACE_SYNC | TRACE_TMSYNC))
                   trace_printf("%s@%d" " - Node "  "%d" " TmSyncState updated (" "%d" ")" "\n", method_name, __LINE__, MyPNID, MyNode->GetTmSyncState());
            }
        }
    
        if (trace_settings & TRACE_INIT)
        {
            trace_printf( "%s@%d - Spare node activating! pnid=%d, name=(%s)\n"
                        , method_name, __LINE__
                        , spareNode->GetPNid(), spareNode->GetName());
        }
    }
    
    if ( spareNode->GetPNid() == MyPNID && spareHealthy )
    {
        if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
           trace_printf( "%s@%d" " - Replicating activate spare node pnid=%d, name=%s (%s), spare=%d, down pnid=%d, name=%s (%s), DTM count=%d\n"
                       , method_name, __LINE__
                       , spareNode->GetPNid(), spareNode->GetName(), StateString(spareNode->GetState())
                       , spareNode->IsSpareNode()
                       , downNode->GetPNid(), downNode->GetName(), StateString(downNode->GetState())
                       , tmCount );
        // Let other monitors know is ok to activate this spare node
        CReplActivateSpare *repl = new CReplActivateSpare( MyPNID, downNode->GetPNid() );
        Replicator.addItem(repl);

        if ( !tmCount )
        {
            // No DTMs in environment so implicitly make ready for transactions
            lnode = MyNode->GetFirstLNode();
            for ( ; lnode; lnode = lnode->GetNext() )
            {
                ReqQueue.enqueueTmReadyReq( lnode->GetNid() );
            }
        }
    }

    TRACE_EXIT;
}

void CCluster::NodeTmReady( int nid )
{
    const char method_name[] = "CCluster::NodeTmReady";
    TRACE_ENTRY;

    if (trace_settings & TRACE_INIT)
    {
        trace_printf( "%s@%d - nid=%d\n", method_name, __LINE__, nid );
    }

    tmReadyCount_++;
    
    MyNode->StartPStartDPersistentDTM( nid );
    
    if ( MyNode->GetNumLNodes() == tmReadyCount_ )
    {
        char la_buf[MON_STRING_BUF_SIZE];
        sprintf(la_buf, "[%s], Node activated! pnid=%d, name=(%s) \n", method_name, MyNode->GetPNid(), MyNode->GetName());
        mon_log_write(MON_CLUSTER_SPARE_NODE_ACTIVE, SQ_LOG_INFO, la_buf); 

        // Let other monitors know the node is up
        CReplActivateSpare *repl = new CReplActivateSpare( MyPNID, -1 );
        Replicator.addItem(repl);
    }

    TRACE_EXIT;
}

void CCluster::NodeReady( CNode *spareNode )
{
    const char method_name[] = "CCluster::NodeReady";
    TRACE_ENTRY;

    if (trace_settings & TRACE_INIT)
    {
        trace_printf( "%s@%d - spare node %s pnid=%d\n"
                    , method_name, __LINE__, spareNode->GetName(), spareNode->GetPNid() );
    }

    assert( spareNode->GetState() == State_Up );

    // Send node up notice
    CLNode *lnode = spareNode->GetFirstLNode();
    for ( ; lnode; lnode = lnode->GetNext() )
    {
        lnode->Up();
    }

    if ( joinComm_ != MPI_COMM_NULL )
    {
        MPI_Comm_free( &joinComm_ );
        joinComm_ = MPI_COMM_NULL;
    }
    spareNode->SetActivatingSpare( false );
    integratingPNid_ = -1;
    if ( MyNode->IsCreator() )
    {
        MyNode->SetCreator( false, -1 );
    }

    TRACE_EXIT;
}


long CCluster::AssignTmSeqNumber( void )
{
    struct sync_def sync;
    
    const char method_name[] = "CCluster::AssignTmSeqNumber";
    TRACE_ENTRY;

    sync.type = SyncType_TmSeqNum;
    sync.length = 0;
    syncCycle_.lock();
    exchangeTmSyncData( &sync );
    syncCycle_.unlock();

    if (trace_settings & TRACE_ENTRY_EXIT)
        trace_printf("%s@%d" " - Exit - sequence number = "  "%d" "\n", method_name, __LINE__, TmSeqAssigned[MyPNID]);
    TRACE_EXIT;
    return TmSeqAssigned[MyPNID];
}


// Assigns a new TMLeader if given pnid is same as TmLeaderNid 
// TmLeader is a logical node num. 
// pnid has gone down, so if that node was previously the TM leader, a new one needs to be chosen.
void CCluster::AssignTmLeader(int pnid)
{
    const char method_name[] = "CCluster::AssignTmLeader";
    TRACE_ENTRY;

    int i = 0;
    CNode *node = NULL;

    int TmLeaderPNid = LNode[TmLeaderNid]->GetNode()->GetPNid();

    if (TmLeaderPNid != pnid) 
    {
        return;
    }

    node = Node[TmLeaderPNid];

    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY | TRACE_REQUEST | TRACE_SYNC | TRACE_TMSYNC))
    {
        trace_printf("%s@%d" " - Node "  "%d" " TmLeader failed." "\n", method_name, __LINE__, TmLeaderNid);
    }

    for (i=0; i<cfgPNodes_; i++)
    {
        TmLeaderPNid++;

        if (TmLeaderPNid == cfgPNodes_)
        {
            TmLeaderPNid = 0; // restart with nid 0
        }

        if (TmLeaderPNid == pnid)
        {
            continue; // this is the node that is going down, skip it
        }

        node = Node[TmLeaderPNid];

        if ( (node->GetState() != State_Up) || node->IsSpareNode() || (node->GetPhase() != Phase_Ready) )
        {
            continue; // skip this node for any of the above reasons 
        }  

        TmLeaderNid = node->GetFirstLNode()->GetNid();

        if (trace_settings & (TRACE_INIT | TRACE_RECOVERY | TRACE_REQUEST | TRACE_SYNC | TRACE_TMSYNC))
        {
            trace_printf("%s@%d" " - Node "  "%d" " is the new TmLeader." "\n", method_name, __LINE__, TmLeaderNid);
        }

        break;
    }

    TRACE_EXIT;
}


CCluster::CCluster (void)
    :
      NumRanks (-1),
      NumNodes (0),
      Node (NULL),
      LNode (NULL),
      TmSyncPNid (-1),
      CurNodes (0),
      CurProcs (0),
      cfgPNodes_ (-1),
      NodeMap (NULL),
      LastTmSeqNum (-1),
      TmLeaderNid (0), // nid 0 is the defacto leader. 
      tmReadyCount_(0),
      TmSeqAssigned (NULL ),
      minRecvCount_(4096),
      recvBuffer_(NULL),
      recvBuffer2_(NULL),
      swpRecCount_(0),
      barrierCount_(0),
      barrierCountSaved_(0),
      inBarrier_(false),
      monInitComplete_(false),
      monSyncResponsive_(true),
      integratingPNid_(-1),
      joinComm_(MPI_COMM_NULL),
      seqNum_(0),
      waitForWatchdogExit_(false)
      ,checkSeqNum_(false)
      ,validateNodeDown_(true)
      ,enqueuedDown_(false)
      ,upNodes_(0)
      ,nodeDownDeathNotices_(true)
{
    int i;
    char  buffer[32];
    char  fname[MAX_PROCESS_PATH];
    FILE *ini;
    
    const char method_name[] = "CCluster::CCluster";
    TRACE_ENTRY;


    MPI_Comm_set_errhandler(MPI_COMM_WORLD,MPI_ERRORS_RETURN);

    char *env = getenv("SQ_MON_CHECK_SEQNUM");
    if ( env )
    {
        int val;
        errno = 0;
        val = strtol(env, NULL, 10);
        if ( errno == 0) checkSeqNum_ = (val != 0);
    }

    if (trace_settings & TRACE_INIT)
       trace_printf("%s@%d Checking sync sequence numbers is %s\n",
                    method_name, __LINE__,
                    (checkSeqNum_ ? "enabled" : "disabled"));
    
    // Compute minimum "sync cycles" per second.   The minimum is 1/10
    // the expected number, assuming "next_test_delay" cycles per second (where
    // next_test_delay is in microseconds).
    syncMinPerSec_ = 1000000 / next_test_delay / 10;

    agMaxElapsed_.tv_sec = 0;
    agMaxElapsed_.tv_nsec = 0;
    agMinElapsed_.tv_sec = 10000;
    agMinElapsed_.tv_nsec = 0;

    tmSyncBuffer_ = Nodes->GetSyncBuffer();

    // Allocate structures for monitor point-to-point communications
    int cfgPNodes = Nodes->GetClusterConfig()->GetPNodesCount();
    comms_        = new MPI_Comm[cfgPNodes];
    otherMonRank_ = new int[cfgPNodes];
    socks_        = new int[cfgPNodes];
    sockPorts_    = new int[cfgPNodes];
    upNodes_      = 0;

    for (i=0; i<cfgPNodes; ++i)
    {
        comms_[i] = MPI_COMM_NULL;
        socks_[i] = -1;
    }

    env = getenv("SQ_MON_NODE_DOWN_VALIDATION");
    if ( env )
    {
        int val;
        errno = 0;
        val = strtol(env, NULL, 10);
        if ( errno == 0) validateNodeDown_ = (val != 0);
    }
    if ( cfgPNodes > 64 ) validateNodeDown_ = false;

    char buf[MON_STRING_BUF_SIZE];
    snprintf(buf, sizeof(buf), "[%s] Validation of node down is %s\n",
             method_name, (validateNodeDown_ ? "enabled" : "disabled"));
    mon_log_write(MON_CLUSTER_CLUSTER, SQ_LOG_INFO, buf);  

    InitializeConfigCluster();

    for (size_t j=0; j<(sizeof(agElapsed_)/sizeof(int)); ++j)
    {
        agElapsed_[j] = 0;
    }

    char *p = getenv("MON_MIN_RECV_COUNT");
    if ( p )
    {
        long int val = strtoul(p, NULL, 10);
        if (errno != ERANGE)
        {
            minRecvCount_ = val;
        }
    }

    p = getenv("SQ_MON_NODE_DOWN_DEATH_MESSAGES");
    if ( p && atoi(p) == 0)
    {
        nodeDownDeathNotices_ = false;
    }
    
    // build the node objects & Sync collision assignment arrays
    // these buffers will be used in ShareWithPeers in AllGather 
    // operation to get TMSync data as well as Replication data.
    recvBuffer_ = new struct sync_buffer_def[NumNodes];
    recvBuffer2_ = new struct sync_buffer_def[NumNodes];

    TmSeqAssigned = new int [NumNodes];



    // Read initialization file
    snprintf(fname, sizeof(fname), "%s/monitor.ini",getenv("MPI_TMPDIR"));
    ini = fopen(fname, "r");
    if( ini )
    {
        for (i=0; i<=MyPNID; i++)
        {
            fgets(buffer,32,ini);
            LastTmSeqNum = atol( &buffer[13] );
        }
        fclose(ini);
    }
    if( LastTmSeqNum == -1 )
    {
        LastTmSeqNum = 0;

        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[CCluster::CCluster], Error= No monitor.ini found, setting to defaults. \n");
        mon_log_write(MON_CLUSTER_CLUSTER, SQ_LOG_ERR, buf);  
    }
    if (trace_settings & TRACE_INIT)
       trace_printf("%s@%d" " - Initialized LastTmSeqNum="  "%d" "\n", method_name, __LINE__, LastTmSeqNum);
    
    TRACE_EXIT;
}

CCluster::~CCluster (void)
{
    int ini=-1;
    long pos=0;
    char buf[32];
    char fname[MAX_PROCESS_PATH];
    
    const char method_name[] = "CCluster::~CCluster";
    TRACE_ENTRY;

    delete [] comms_;
    delete[] otherMonRank_;
    delete [] socks_;
    delete [] sockPorts_;
    if (NodeMap)
    {
        delete [] NodeMap;
        NodeMap = NULL;
    }
    
    delete [] TmSeqAssigned;



    // Read initialization file
    snprintf(fname,sizeof(fname),"%s/monitor.ini",getenv("MPI_TMPDIR"));
    ini = open(fname, O_WRONLY | O_SYNC | O_CREAT, S_IRUSR | S_IWUSR );
    if( ini != -1 )
    {
        LOCKFILE(ini);
        if (ret == -1)
        {
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[CCluster::~CCluster], Error= Can't lock monitor.ini file. \n");
            mon_log_write(MON_CLUSTER_UCLUSTER, SQ_LOG_ERR, buf); 
        }
        pos = lseek(ini,(long)(24*MyPNID),SEEK_SET);
        if( pos != -1 )
        {
            snprintf (buf, sizeof(buf), "%3.3d:TmSeqNum:%10.10d\n", MyPNID, LastTmSeqNum);
            write(ini,buf,strlen(buf));
        }
        UNLOCKFILE(ini);
        close(ini);
    }

    delete [] recvBuffer2_;
    delete [] recvBuffer_;

    TRACE_EXIT;
}


void CCluster::CoordinateTmSeqNumber( int pnid )
{
    const char method_name[] = "CCluster::CoordinateTmSeqNumber";
    TRACE_ENTRY;

    LastTmSeqNum++;
    if( LastTmSeqNum >= MAX_SEQ_VALUE )
    {
        LastTmSeqNum = 1;
    }
    TmSeqAssigned[pnid] = LastTmSeqNum;

    TRACE_EXIT;
}

// For a reintegrated monitor node, following the first sync cycle, obtain the
// current sync cycle sequence number.   And verify that all nodes agree
// on the sequence number.
unsigned long long CCluster::EnsureAndGetSeqNum(cluster_state_def_t nodestate[])
{
    const char method_name[] = "CCluster::EnsureAndGetSeqNum";
    TRACE_ENTRY;

    unsigned long long seqNum = 0;

    for (int i = 0; i < cfgPNodes_; i++)
    {
        if (trace_settings & TRACE_RECOVERY)
        {
            trace_printf("%s@%d nodestate[%d].seq_num=%lld, seqNum=%lld\n", method_name, __LINE__, i, nodestate[i].seq_num, seqNum );
        }
        if (nodestate[i].seq_num > 0)
        {
            if (seqNum == 0) 
            {
                seqNum = nodestate[i].seq_num;
            }
            else
            {
                assert(nodestate[i].seq_num == seqNum);
            }
        }
    }

    TRACE_EXIT;
    return seqNum;
}


void CCluster::MarkDown (int pnid, bool communicate_state)
{
    char port_fname[MAX_PROCESS_PATH];
    char temp_fname[MAX_PROCESS_PATH];
    CNode  *node;
    CLNode *lnode;
    char    buf[MON_STRING_BUF_SIZE];
    
    const char method_name[] = "CCluster::MarkDown";
    TRACE_ENTRY;

    node = Nodes->GetNode(pnid);
    
    if (trace_settings & (TRACE_REQUEST | TRACE_INIT | TRACE_RECOVERY))
       trace_printf( "%s@%d - pnid=%d, comm_state=%d, state=%s, isInQuiesceState=%d,"
                     " (local pnid=%d, state=%s, isInQuiesceState=%d, "
                     "shutdown level=%d)\n", method_name, __LINE__,
                     pnid, communicate_state, StateString(node->GetState()),
                     node->isInQuiesceState(),
                     MyPNID, StateString(MyNode->GetState()),
                     MyNode->isInQuiesceState(), MyNode->GetShutdownLevel() );

    if (( MyPNID == pnid              ) &&
        ( MyNode->GetState() == State_Down || 
          MyNode->IsKillingNode() ) )
    {
        // we are coming down ... don't process it
        if ( !IsRealCluster && MyNode->isInQuiesceState())
        {
          // in virtual env, this would be called after node quiescing, 
          // so continue with mark down processing.
        }
        else
        {
          return;
        }
    }

    if ( (MyNode->GetShutdownLevel() != ShutdownLevel_Undefined) &&
         (pnid != MyPNID) ) // some other node went down while shutdown was in progress
    {
        snprintf(buf, sizeof(buf), "[%s], Node failure during shutdown, down nid = %d\n", method_name, pnid);
        mon_log_write(MON_CLUSTER_MARKDOWN_1, SQ_LOG_ERR, buf);

        if (!waitForWatchdogExit_) // if WDT is not exiting
        {
            // bring down this node because TSE backup processes may not exit
            // if the primary was on the node that went down.
            ReqQueue.enqueueDownReq(MyPNID);
        }
    }
   
    if ( communicate_state && pnid != MyPNID )
    {
        // just communicate the change and let the real node handle it.
        node->SetChangeState( true );
        return;
    }

    if ( !Emulate_Down )
    {
        if( !IsRealCluster )
        {
            snprintf(port_fname, sizeof(port_fname), "%s/monitor.%d.port.%s",getenv("MPI_TMPDIR"),pnid,node->GetName());
        }
        else
        {
            snprintf(port_fname, sizeof(port_fname), "%s/monitor.port.%s",getenv("MPI_TMPDIR"),node->GetName());
        }
        sprintf(temp_fname, "%s.bak", port_fname);
        remove(temp_fname);
        rename(port_fname, temp_fname);

    }

    if (node->GetState() != State_Down || node->isInQuiesceState())
    {
        snprintf(buf, sizeof(buf),
                 "[CCluster::MarkDown], Node %s (%d) is going down.\n",
                 node->GetName(), node->GetPNid());
        mon_log_write(MON_CLUSTER_MARKDOWN_1, SQ_LOG_ERR, buf); 

        node->SetKillingNode( true ); 
        
        if ( MyPNID == pnid && 
             (MyNode->GetState() == State_Up || MyNode->GetState() == State_Shutdown) &&
            !MyNode->isInQuiesceState() )
        { 
            STATE state = MyNode->GetState();
            switch ( state )
            {
            case State_Up:
            case State_Shutdown:
                // do node quiescing and let HealthCheck thread know that quiescing has started
                // setting internal state to 'quiesce' will prevent replicating process exits
                // and reject normal shutdown requests in all nodes while we are quiescing.
                if (!waitForWatchdogExit_) // if WDT is not exiting
                {
                    MyNode->setQuiesceState();
                    HealthCheck.setState(MON_NODE_QUIESCE);
                }
                break;
            default: // in all other states
                if ( ! Emulate_Down )
                {
                    // make sure no processes are alive if in the middle of re-integration
                    node->KillAllDown();
                    snprintf(buf, sizeof(buf),
                             "[CCluster::MarkDown], Node %s (%d)is down.\n",
                             node->GetName(), node->GetPNid());
                    mon_log_write(MON_CLUSTER_MARKDOWN_2, SQ_LOG_ERR, buf); 
                    // Don't generate a core file, abort is intentional
                    struct rlimit limit;
                    limit.rlim_cur = 0;
                    limit.rlim_max = 0;
                    setrlimit(RLIMIT_CORE, &limit);
                    MPI_Abort(MPI_COMM_SELF,99);
                }
            }
        }
        else
        {
            if ( node->GetPNid() == integratingPNid_ )
            {
                if ( joinComm_ != MPI_COMM_NULL )
                {
                    MPI_Comm_free( &joinComm_ );
                    joinComm_ = MPI_COMM_NULL;
                }
                integratingPNid_ = -1;
                if ( MyNode->IsCreator() )
                {
                    MyNode->SetCreator( false, -1 );
                }
            }
            node->KillAllDown();
            node->SetState( State_Down ); 
            lnode = node->GetFirstLNode();
            for ( ; lnode; lnode = lnode->GetNext() )
            {
                lnode->Down();
            }
        }
    }

    // we need to abort any active TmSync
    if (( MyNode->GetTmSyncState() == SyncState_Start    ) ||
        ( MyNode->GetTmSyncState() == SyncState_Continue ) ||
        ( MyNode->GetTmSyncState() == SyncState_Commit   )   )
    {
        MyNode->SetTmSyncState( SyncState_Abort );
        Monitor->SetAbortPendingTmSync();
        if (trace_settings & (TRACE_RECOVERY | TRACE_REQUEST | TRACE_SYNC | TRACE_TMSYNC))
           trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated (%d)(%s)\n", method_name, __LINE__, MyNode->GetName(), MyPNID, MyNode->GetTmSyncState(), SyncStateString( MyNode->GetTmSyncState() ));
    }

    if ( Emulate_Down )
    {
        IAmIntegrated = false;
        AssignTmLeader(pnid);
    }

    TRACE_EXIT;
}

bool CCluster::CheckSpareSet( int pnid )
{
    bool activatedSpare = false;
    bool done = false;
    unsigned int ii;
    unsigned int jj;
    CNode *newNode = Nodes->GetNode( pnid );
    
    const char method_name[] = "CCluster::CheckSpareSet";
    TRACE_ENTRY;

    // Build spare node set
    CNode *spareNode;
    NodesList spareSetList;
    NodesList *spareNodesConfigList = Nodes->GetSpareNodesConfigList();
    NodesList::iterator itSn;
    for ( itSn = spareNodesConfigList->begin(); 
          itSn != spareNodesConfigList->end() && !done ; itSn++ ) 
    {
        spareNode = *itSn;
        PNidVector sparePNids = spareNode->GetSparePNids();
        // if the new node is a spare node in the configuration
        if ( newNode->GetPNid() == spareNode->GetPNid() )
        {
            // Add the spare node and each node it is configured to spare to the set
            spareSetList.push_back( spareNode );

            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                trace_printf("%s@%d - pnid=%d, name=(%s) is a configured Spare\n", method_name, __LINE__, spareNode->GetPNid(), spareNode->GetName());

            for ( ii = 0; ii < sparePNids.size(); ii++ )
            {
                spareSetList.push_back( Nodes->GetNode(sparePNids[ii]) );

                if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                    trace_printf("%s@%d - pnid=%d, name=(%s) is in Spare set\n", method_name, __LINE__, Nodes->GetNode(sparePNids[ii])->GetPNid(), Nodes->GetNode(sparePNids[ii])->GetName());
            }
            done = true;
        }
        else
        {
            // Check each pnid it is configured to spare
            for ( jj = 0; jj < sparePNids.size(); jj++ )
            {
                // if the new node is in the spare set of a spare node
                if ( newNode->GetPNid() == sparePNids[jj] )
                {
                    // Add the spare node and each node it is configured to spare to the set
                    spareSetList.push_back( spareNode );

                    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                        trace_printf("%s@%d - pnid=%d, name=(%s) is a configured Spare\n", method_name, __LINE__, spareNode->GetPNid(), spareNode->GetName());

                    for ( ii = 0; ii < sparePNids.size(); ii++ )
                    {
                        spareSetList.push_back( Nodes->GetNode(sparePNids[ii]) );

                        if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                            trace_printf("%s@%d - pnid=%d, name=(%s) is in Spare set\n", method_name, __LINE__, Nodes->GetNode(sparePNids[ii])->GetPNid(), Nodes->GetNode(sparePNids[ii])->GetName());
                    }
                    done = true;
                }
            }
        }
    }

    CLNode  *lnode;
    CNode   *node;
    CNode   *downNode = NULL;
    spareNode = NULL;
    
    // Get the configured logical node
    CClusterConfig *clusterConfig = Nodes->GetClusterConfig() ;
    CPNodeConfig   *pnodeConfig = clusterConfig->GetPNodeConfig( pnid );
    if ( !pnodeConfig->IsSpareNode() )
    {
        CLNodeConfig   *lnodeConfig = pnodeConfig->GetFirstLNodeConfig();
        // Now get the current owning physical node of the configured logical node
        lnode = Nodes->GetLNode( lnodeConfig->GetNid() );
        node = lnode->GetNode();

        if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
        {
            trace_printf( "%s@%d - config pnid=%d, config name=(%s), config nid=%d\n"
                        , method_name, __LINE__
                        , pnodeConfig->GetPNid(), pnodeConfig->GetName(), lnodeConfig->GetNid());
            trace_printf( "%s@%d - owner pnid=%d, owner name=(%s), nid=%d\n"
                        , method_name, __LINE__
                        , node->GetPNid(), node->GetName(), lnode->GetNid());
        }

        // First check if configured logical node is still owned by 
        // the operational physical node
        if ( lnode->GetNode()->GetPNid() == pnid )
        {
            // assume implicit spare node activation 
            // (no need to move logical nodes to physical node)
            // since MarkUp() already set State_Up, 
            // just reset spare node flag and remove from available spare nodes
            newNode->ResetSpareNode();
            Nodes->RemoveFromSpareNodesList( newNode );
            ActivateSpare( newNode, newNode );
            activatedSpare = true;
            TRACE_EXIT;
            return( activatedSpare );
        }
    }

    // Now check the state of each configured logical node in the set for down state
    spareNode = newNode;  // new node (pnid) is the spare to activate
    NodesList::iterator itSs;
    for ( itSs = spareSetList.begin(); itSs != spareSetList.end(); itSs++ ) 
    {
        node = *itSs;
        if ( node->GetPNid() != pnid )
        {
            // Find the first down node
            if ( !downNode )
            {
                lnode = node->GetFirstLNode();
                if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                    trace_printf( "%s@%d - node nid=%d, pnid=%d(%s), state=%s\n"
                                , method_name, __LINE__, lnode->GetNid()
                                , node->GetPNid(), node->GetName()
                                , StateString( node->GetState() ) );
                if ( lnode && lnode->GetState() == State_Down )
                {
                    downNode = node;
                }
            }
        }
        if ( spareNode && downNode )
        {
            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                trace_printf( "%s@%d - spare node pnid=%d (%s), down node pnid=%d (%s) \n"
                            , method_name, __LINE__
                            , spareNode->GetPNid(), spareNode->GetName()
                            , downNode->GetPNid(), downNode->GetName());
            break;
        }
    }

    if ( spareNode && downNode )
    {
        Nodes->RemoveFromSpareNodesList( spareNode );
        spareNode->ResetSpareNode();
        if ( downNode->GetPNid() != pnid )
        { // the spare node does not own the down logical nodes so activate it
            ActivateSpare( spareNode, downNode );
        }
        activatedSpare = true;
    }

    TRACE_EXIT;
    return( activatedSpare );
}

const char *JoiningPhaseString( JOINING_PHASE phase )
{
    const char *str;
    
    switch( phase )
    {
        case JoiningPhase_Unknown:
            str = "JoiningPhase_Unknown";
            break;
        case JoiningPhase_1:
            str = "JoiningPhase_1";
            break;
        case JoiningPhase_2:
            str = "JoiningPhase_2";
            break;
        case JoiningPhase_3:
            str = "JoiningPhase_3";
            break;
        default:
            str = "JoiningPhase - Undefined";
            break;
    }

    return( str );
}

struct message_def *CCluster::JoinMessage( const char *node_name, int pnid, JOINING_PHASE phase )
{
    struct message_def *msg;

    const char method_name[] = "CCluster::JoinMessage";
    TRACE_ENTRY;
    
    // Record statistics (sonar counters)
       MonStats->notice_death_Incr();

    msg = new struct message_def;
    msg->type = MsgType_NodeJoining;
    msg->noreply = true;
    msg->u.request.type = ReqType_Notice;
    strcpy( msg->u.request.u.joining.node_name, node_name );
    msg->u.request.u.joining.pnid = pnid;
    msg->u.request.u.joining.phase = phase;

    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY | TRACE_REQUEST_DETAIL))
        trace_printf("%s@%d - Joining notice for node %s (pnid=%d, phase=%d)\n",
                     method_name, __LINE__, node_name, pnid, phase );
    TRACE_EXIT;

    return msg;
}

struct message_def *CCluster::SpareUpMessage( const char *node_name, int pnid )
{
    struct message_def *msg;

    const char method_name[] = "CCluster::SpareUpMessage";
    TRACE_ENTRY;
    
    // Record statistics (sonar counters)
       MonStats->notice_death_Incr();

    msg = new struct message_def;
    msg->type = MsgType_SpareUp;
    msg->noreply = true;
    msg->u.request.type = ReqType_Notice;
    strcpy( msg->u.request.u.spare_up.node_name, node_name );
    msg->u.request.u.spare_up.pnid = pnid;

    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY | TRACE_REQUEST_DETAIL))
        trace_printf("%s@%d - Spare node up notice for node %s nid=%d\n",
                     method_name, __LINE__, node_name, pnid );
    TRACE_EXIT;

    return msg;
}

struct message_def *CCluster::ReIntegErrorMessage( const char *msgText )
{
    struct message_def *msg;

    const char method_name[] = "CCluster::ReIntegErrorMessage";
    TRACE_ENTRY;
    
    msg = new struct message_def;
    msg->type = MsgType_ReintegrationError;
    msg->noreply = true;
    msg->u.request.type = ReqType_Notice;
    strncpy( msg->u.request.u.reintegrate.msg, msgText,
             sizeof(msg->u.request.u.reintegrate.msg) );

    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY | TRACE_REQUEST_DETAIL))
        trace_printf("%s@%d - Reintegrate notice %s\n",
                     method_name, __LINE__, msgText );

    TRACE_EXIT;

    return msg;
}

int CCluster::MarkUp( int pnid, char *node_name )
{
    bool    spareNodeActivated = false;
    int     rc = MPI_SUCCESS;
    int     tmCount = 0;
    CNode  *node;
    CLNode *lnode;
    STATE   nodeState;
    
    const char method_name[] = "CCluster::MarkUp";
    TRACE_ENTRY;

    if (trace_settings & (TRACE_REQUEST | TRACE_INIT | TRACE_RECOVERY))
       trace_printf( "%s@%d - pnid=%d, name=%s\n"
                   , method_name, __LINE__, pnid, node_name );

    if ( pnid == -1 )
    {
        node = Nodes->GetNode( node_name );
    }
    else
    {
        node = Nodes->GetNode( pnid );
    }
    
    if ( node == NULL )
    {
        if (trace_settings & (TRACE_REQUEST | TRACE_INIT | TRACE_RECOVERY))
           trace_printf( "%s@%d" " - Invalid node, pnid=%d, name=%s" "\n"
                       , method_name, __LINE__, pnid, node_name );
    
        return( MPI_ERR_NAME );
    }
    
    nodeState = node->GetState();
     
    if (trace_settings & (TRACE_REQUEST | TRACE_INIT | TRACE_RECOVERY))
       trace_printf( "%s@%d" " - Node state=%s" "\n"
                   , method_name, __LINE__, StateString( nodeState ) );
    if ( nodeState != State_Up )
    {
        if ( nodeState == State_Down )
        {
            node->SetKillingNode( false ); 
            if ( Emulate_Down )
            {
                // Any DTMs running?
                for ( int i=0; !tmCount && i < Nodes->GetNodesCount(); i++ )
                {
                    CNode  *tempNode = Nodes->GetNode( i );
                    lnode = tempNode->GetFirstLNode();
                    for ( ; lnode; lnode = lnode->GetNext() )
                    {
                        CProcess *process = lnode->GetProcessLByType( ProcessType_DTM );
                        if ( process  ) tmCount++;
                    }
                }
                if ( tmCount )
                {
                    IAmIntegrated = true;
                }
                // We need to remove any old process objects before we restart the node.
                node->CleanUpProcesses();
                node->SetState( State_Up ); 
                if ( MyPNID == pnid )
                {
                    MyNode->clearQuiesceState();
                    HealthCheck.initializeVars(); 
                    SMSIntegrating = true;
                    Monitor->StartPrimitiveProcesses();
                        // Let other monitors know this node is up
                        CReplNodeUp *repl = new CReplNodeUp(MyPNID);
                        Replicator.addItem(repl);
                }
                else
                {
                    if ( tmCount )
                    {
                        // Send node prepare notice to local DTM processes
                        lnode = node->GetFirstLNode();
                        for ( ; lnode; lnode = lnode->GetNext() )
                        {
                            lnode->PrepareForTransactions( true );
                        }
                    }
                    else
                    {
                        // Process logical node up
                        lnode = node->GetFirstLNode();
                        for ( ; lnode; lnode = lnode->GetNext() )
                        {
                            lnode->Up();
                        }
                    }
                }
            }
            else
            {
                if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                    trace_printf( "%s@%d - Unexpectedly executing MarkUp.  Expecting to do accept in commAccept thread\n",
                                  method_name, __LINE__ );

            }
        }
        else if ( nodeState == State_Merged )
        {
            node->SetKillingNode( false ); 
            node->SetState( State_Joining );
            
            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
               trace_printf( "%s@%d" " - New monitor %s, pnid=%d, state=%s" "\n"
                           , method_name, __LINE__, node->GetName(), node->GetPNid(), StateString( node->GetState() ) );
            if ( MyNode->IsCreator() )
            {
                SQ_theLocalIOToClient->putOnNoticeQueue( MyNode->GetCreatorPid()
                                                       , JoinMessage( node->GetName()
                                                                    , node->GetPNid()
                                                                    , JoiningPhase_1 )
                                                       , NULL);

                // save the current seq num in the snapshot request.
                // this sequence number will match the state of the cluster
                // when this request is processed. 
                ReqQueue.enqueueSnapshotReq(seqNum_); 
            }
            if ( MyPNID == pnid )
            {
                // request and process revive packet from the creator.
                // when complete, this will call MarkUp again.
                ReqQueue.enqueueReviveReq( ); 
            }
        }
        else if ( nodeState == State_Joining )
        {
            // The new monitor comes in here first and schedules a node up request on all nodes.  
            // All other monitors come here next, including the creator.
            // The new monitor will not come here again because 
            // CReplNodeUp is a noop for the one who schedules it.
            node->SetState( State_Up );

            if ( Nodes->GetSNodesCount() == 0 )
            { // Spare nodes not configured so bring up my logical nodes
                if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                   trace_printf( "%s@%d" " - No spare nodes configured node=%s, pnid=%d, state=%s\n"
                               , method_name, __LINE__, node->GetName(), node->GetPNid()
                               , StateString(node->GetState()) );
                if ( MyPNID == pnid )
                {
                    ActivateSpare( node, node );
                }
            }
            else
            {
                node->SetSpareNode();
                Nodes->AddToSpareNodesList( node->GetPNid() );
                if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                   trace_printf( "%s@%d" " - Adding to available spares node=%s, pnid=%d\n"
                               , method_name, __LINE__, node->GetName(), node->GetPNid() );
                // Check for a node down in spare set and activate down node if found
                spareNodeActivated = CheckSpareSet( node->GetPNid() );
                if ( spareNodeActivated )
                {
                    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                       trace_printf( "%s@%d" " - Activated spare node=%s, pnid=%d\n"
                                   , method_name, __LINE__, node->GetName(), node->GetPNid() );
                }
                else
                {
                    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                       trace_printf( "%s@%d" " - Available spare node=%s, pnid=%d\n"
                                   , method_name, __LINE__, node->GetName(), node->GetPNid() );

                    // Spare node not activated
                    if ( MyNode->IsCreator() )
                    {
                        if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                           trace_printf( "%s@%d" " - Sending spare up notice to creator shell(%d) spare node=%s, pnid=%d\n"
                                       , method_name, __LINE__, MyNode->GetCreatorPid(), node->GetName(), node->GetPNid() );
                        // Tell creator spare node is up
                        SQ_theLocalIOToClient->putOnNoticeQueue( MyNode->GetCreatorPid()
                                                               , SpareUpMessage( node->GetName()
                                                                               , node->GetPNid() )
                                                               , NULL);
                    }
                }
            }

            if ( MyPNID == pnid )
            {
                // Any DTMs running?
                for ( int i=0; !tmCount && i < Nodes->GetNodesCount(); i++ )
                {
                    CNode  *tempNode = Nodes->GetNode( i );
                    lnode = tempNode->GetFirstLNode();
                    for ( ; lnode; lnode = lnode->GetNext() )
                    {
                        CProcess *process = lnode->GetProcessLByType( ProcessType_DTM );
                        if ( process  ) tmCount++;
                    }
                }
                if ( !tmCount && !spareNodeActivated )
                {
                    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                       trace_printf( "%s@%d" " - Replicating node up %s, pnid=%d, state=%s, spare=%d, DTM count=%d\n"
                                   , method_name, __LINE__, node->GetName(), node->GetPNid()
                                   , StateString(node->GetState()), node->IsSpareNode(), tmCount );
                    // Let other monitors know this node is up
                    CReplNodeUp *repl = new CReplNodeUp(MyPNID);
                    Replicator.addItem(repl);
                }
            }

            if ( joinComm_ != MPI_COMM_NULL )
            {
                MPI_Comm_free( &joinComm_ );
                joinComm_ = MPI_COMM_NULL;
            }
            integratingPNid_ = -1;
            if ( MyNode->IsCreator() )
            {
                MyNode->SetCreator( false, -1 );
            }

            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
               trace_printf( "%s@%d" " - New monitor %s, pnid=%d, state=%s, spare=%d\n"
                           , method_name, __LINE__, node->GetName(), node->GetPNid()
                           , StateString(node->GetState()), node->IsSpareNode() );
        }
    }

    TRACE_EXIT;
    return( rc );
}




const char *StateString( STATE state)
{
    const char *str;
    
    switch( state )
    {
        case State_Unknown:
            str = "State_Unknown";
            break;
        case State_Up:
            str = "State_Up";
            break;
        case State_Down:
            str = "State_Down";
            break;
        case State_Stopped:
            str = "State_Stopped";
            break;
        case State_Shutdown:
            str = "State_Shutdown";
            break;
        case State_Unlinked:
            str = "State_Unlinked";
            break;
        case State_Merging:
            str = "State_Merging";
            break;
        case State_Merged:
            str = "State_Merged";
            break;
        case State_Joining:
            str = "State_Joining";
            break;
        case State_Initializing:
            str = "State_Initializing";
            break;
        default:
            str = "State - Undefined";
            break;
    }

    return( str );
}

const char *SyncStateString( SyncState state)
{
    const char *str;
    
    switch( state )
    {
        case SyncState_Null:
            str = "SyncState_Null";
            break;
        case SyncState_Start:
            str = "SyncState_Start";
            break;
        case SyncState_Continue:
            str = "SyncState_Continue";
            break;
        case SyncState_Abort:
            str = "SyncState_Abort";
            break;
        case SyncState_Commit:
            str = "SyncState_Commit";
            break;
        case SyncState_Suspended:
            str = "SyncState_Suspended";
            break;
        default:
            str = "SyncState - Undefined";
            break;
    }

    return( str );
}


void CCluster::AddTmsyncMsg (struct sync_def *sync,
                             struct internal_msg_def *msg)
{
    const char method_name[] = "CCluster::AddTmsyncMsg";
    TRACE_ENTRY;

    if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
        trace_printf("%s@%d - Requesting SyncType=%d\n", method_name,
                     __LINE__, sync->type);

    msg->type = InternalType_Sync;
    msg->u.sync.type = sync->type;
    msg->u.sync.pnid = sync->pnid;
    msg->u.sync.syncnid = sync->syncnid;
    msg->u.sync.tmleader = sync->tmleader;
    msg->u.sync.state = sync->state;
    msg->u.sync.count = sync->count;
    if ( sync->type == SyncType_TmData )
    {
        memmove (msg->u.sync.data, sync->data, sync->length);
    }
    msg->u.sync.length = sync->length;

    // We can have only have a single "InternalType_Sync" msg in our
    // SyncBuffer, else we cause a collision.

    int msgSize = (MSG_HDR_SIZE + sizeof(sync_def) - MAX_SYNC_DATA
                   + sync->length );

    // Insert the message size into the message header
    msg->replSize = msgSize;
    tmSyncBuffer_->msgInfo.msg_count = 1;
    tmSyncBuffer_->msgInfo.msg_offset += msgSize;

    // Set end-of-buffer marker
    msg = (struct internal_msg_def *)
        &tmSyncBuffer_->msg[tmSyncBuffer_->msgInfo.msg_offset];
    msg->type = InternalType_Null;

    TRACE_EXIT;
}


void CCluster::DoDeviceReq(char * ldevName)
{
    const char method_name[] = "CCluster::DoDeviceReq";
    TRACE_ENTRY;

    CProcess *process;

    if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
        trace_printf("%s@%d - Internal device request for ldev %s\n",
                     method_name, __LINE__, ldevName);
    Nodes->GetLNode(ldevName, &process);
    if (!process)
    {
        if (trace_settings & TRACE_SYNC)
            trace_printf("%s@%d - Device processing but can't find device %s\n",
                         method_name, __LINE__, ldevName);
    }
    else
    {
        CLogicalDevice *ldev;
        ldev = Devices->GetLogicalDevice( ldevName );
        if ( !ldev )
        {   // The device name is not known on this node
            // we need to clone the device
            ldev = Devices->CloneDevice( process );
        }
        if ( ldev )
        {
            bool rstate = false;
            if ( ldev->Mounted() )
            {
                rstate = ldev->UnMount( false );
                if (!rstate)
                {
                    if (trace_settings & TRACE_REQUEST)
                        trace_printf("%s@%d - Can't unmount device %s for "
                                     "process %s (%d, %d)\n", method_name,
                                     __LINE__, ldev->name(), process->GetName(),
                                     process->GetNid(), process->GetPid());
                }
            }
            if ( rstate )
            {
                rstate = ldev->Mount( process, false );
                if (!rstate)
                {
                    if (trace_settings & TRACE_REQUEST)
                        trace_printf("%s@%d - Can't mount device %s for "
                                     "process %s (%d, %d)\n", method_name,
                                     __LINE__, ldev->name(), process->GetName(),
                                     process->GetNid(), process->GetPid());
                }
                else
                {
                    if (trace_settings & TRACE_REQUEST)
                        trace_printf("%s@%d - Mounted device %s for process "
                                     "%s (%d, %d)\n", method_name, __LINE__,
                                     ldev->name(), process->GetName(),
                                     process->GetNid(), process->GetPid());
                }
            }
        }
        else
        {
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s], Can't find ldev %s.\n", method_name, 
                    ldevName);
            mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_2, SQ_LOG_ERR, buf);
        }
    }

    TRACE_EXIT;
}

void CCluster::SaveSchedData( struct internal_msg_def *recv_msg )
{
    const char method_name[] = "CCluster::SaveSchedData";
    TRACE_ENTRY;

    int nid = recv_msg->u.scheddata.PNid;
    Node[nid]->SetNumCores( recv_msg->u.scheddata.processors );
    Node[nid]->SetFreeMemory( recv_msg->u.scheddata.memory_free );
    Node[nid]->SetFreeSwap( recv_msg->u.scheddata.swap_free );
    Node[nid]->SetFreeCache( recv_msg->u.scheddata.cache_free );
    Node[nid]->SetMemTotal( recv_msg->u.scheddata.memory_total );
    Node[nid]->SetMemActive( recv_msg->u.scheddata.memory_active );
    Node[nid]->SetMemInactive( recv_msg->u.scheddata.memory_inactive );
    Node[nid]->SetMemDirty( recv_msg->u.scheddata.memory_dirty );
    Node[nid]->SetMemWriteback( recv_msg->u.scheddata.memory_writeback );
    Node[nid]->SetMemVMallocUsed( recv_msg->u.scheddata.memory_VMallocUsed );
    Node[nid]->SetBTime( recv_msg->u.scheddata.btime );

    CLNode *lnode;
    lnode = Node[nid]->GetFirstLNode();
    int i = 0;

    for ( ; lnode; lnode = lnode->GetNext() )
    {
        lnode->SetCpuUser(recv_msg->u.scheddata.proc_stats[i].cpu_user);
        lnode->SetCpuNice(recv_msg->u.scheddata.proc_stats[i].cpu_nice);
        lnode->SetCpuSystem(recv_msg->u.scheddata.proc_stats[i].cpu_system);
        lnode->SetCpuIdle(recv_msg->u.scheddata.proc_stats[i].cpu_idle);
        lnode->SetCpuIowait(recv_msg->u.scheddata.proc_stats[i].cpu_iowait);
        lnode->SetCpuIrq(recv_msg->u.scheddata.proc_stats[i].cpu_irq);
        lnode->SetCpuSoftIrq(recv_msg->u.scheddata.proc_stats[i].cpu_soft_irq);

        ++i;
    }

    TRACE_EXIT;
}

void CCluster::HandleOtherNodeMsg (struct internal_msg_def *recv_msg,
                                   int pnid)
{
    const char method_name[] = "CCluster::HandleOtherNodeMsg";
    TRACE_ENTRY;

    CNode *downNode;
    CNode *spareNode;
    CProcess *process;
    CLNode  *lnode;

    switch (recv_msg->type)
    {
    case InternalType_Null:
        if (trace_settings & TRACE_SYNC_DETAIL)
            trace_printf("%s@%d - Node n%d has nothing to "
                         "update. \n", method_name, __LINE__, pnid);
        break;
    
    case InternalType_ActivateSpare:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal activate spare request, spare pnid=%d, down pnid=%d\n"
                        , method_name, __LINE__
                        , recv_msg->u.activate_spare.spare_pnid
                        , recv_msg->u.activate_spare.down_pnid);

        downNode = NULL;
        if ( recv_msg->u.activate_spare.down_pnid != -1 )
        {
            downNode = Nodes->GetNode( recv_msg->u.activate_spare.down_pnid );
        }
        spareNode = Nodes->GetNode( recv_msg->u.activate_spare.spare_pnid );
        ReqQueue.enqueueActivateSpareReq( spareNode, downNode );
        break;

    case InternalType_Clone:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal clone request, process (%d, %d)"
                         " %s\n", method_name, __LINE__,
                         recv_msg->u.clone.nid, recv_msg->u.clone.os_pid,
                         (recv_msg->u.clone.backup?" Backup":""));

        ReqQueue.enqueueCloneReq( &recv_msg->u.clone );
        break;

    case InternalType_Close:
        break;

    case InternalType_Device:
        ReqQueue.enqueueDeviceReq(recv_msg->u.device.ldev_name);
        break;

    case InternalType_Shutdown:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal shutdown request for level=%d\n", method_name, __LINE__, recv_msg->u.shutdown.level);

        // Queue the shutdown request for processing by a worker thread.
        ReqQueue.enqueueShutdownReq( recv_msg->u.shutdown.level );
        break;

    case InternalType_Down:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal down node request for pnid=%d\n", method_name, __LINE__, recv_msg->u.down.pnid);

        // Queue the node down request for processing by a worker thread.
        ReqQueue.enqueueDownReq( recv_msg->u.down.pnid );
        break;

    case InternalType_Up:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal up node request for pnid=%d\n", method_name, __LINE__, recv_msg->u.up.pnid);

        // Queue the node up request for processing by a worker thread.
        ReqQueue.enqueueUpReq( recv_msg->u.up.pnid, NULL, -1 );
        break;

    case InternalType_Dump:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal dump request for nid=%d, pid=%d\n",
                         method_name, __LINE__,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
        lnode = Nodes->GetLNode( recv_msg->u.dump.nid );
        if ( lnode )
        {
            process = lnode->GetProcessL(recv_msg->u.dump.pid);

            if (process)
            {
                process->DumpBegin(recv_msg->u.dump.dumper_nid,
                                   recv_msg->u.dump.dumper_pid,
                                   recv_msg->u.dump.core_file);
            }
            else
            {
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s], Can't find process nid=%d, "
                         "pid=%d for dump target.\n", method_name,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
                mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_3, SQ_LOG_ERR, buf);
            }
        }

        break;

    case InternalType_DumpComplete:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal dump-complete request for nid=%d, pid=%d\n",
                         method_name, __LINE__,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
        lnode = Nodes->GetLNode( recv_msg->u.dump.nid );
        if ( lnode )
        {
            process = lnode->GetProcessL(recv_msg->u.dump.pid);

            if (process)
            {
                process->DumpEnd(recv_msg->u.dump.status, recv_msg->u.dump.core_file);
            }
            else
            {
                // Dump completion handled in CProcess::Exit()
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s], Can't find process nid=%d, "
                         "pid=%d for dump complete target.\n", method_name,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
                mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_4, SQ_LOG_ERR, buf);
            }
        }
        break;

    case InternalType_Exit:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal exit request for %s (%d, %d)\n", method_name, __LINE__, recv_msg->u.exit.name, recv_msg->u.exit.nid, recv_msg->u.exit.pid);
        ReqQueue.enqueueExitReq( &recv_msg->u.exit );
        break;

    case InternalType_Event:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal event request\n", method_name, __LINE__);
        if ( MyNode->IsMyNode(recv_msg->u.event.nid) )
        {    
            if (trace_settings & TRACE_SYNC)
                trace_printf("%s@%d - processing event for (%d, %d)\n", method_name, __LINE__, recv_msg->u.event.nid, recv_msg->u.event.pid);

            lnode = Nodes->GetLNode( recv_msg->u.event.nid );
            if ( lnode )
            {
                process = lnode->GetProcessL(recv_msg->u.event.pid);

                if (process)
                {
                    process->GenerateEvent (recv_msg->u.event.event_id,
                                            recv_msg->u.event.length,
                                            &recv_msg->u.event.data);
                }
                else
                {
                    char buf[MON_STRING_BUF_SIZE];
                    snprintf(buf, sizeof(buf), "[%s], Can't find process nid"
                             "=%d, pid=%d for processing event.\n",
                             method_name, 
                             recv_msg->u.event.nid, recv_msg->u.event.pid);
                    mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_6, SQ_LOG_ERR,
                                  buf);
                }
            }
        }
        break;

    case InternalType_IoData:
        if (trace_settings & (TRACE_SYNC_DETAIL | TRACE_REQUEST_DETAIL | TRACE_REDIRECTION))
            trace_printf("%s@%d - Internal IO data request\n", method_name, __LINE__);
        if ( MyNode->IsMyNode(recv_msg->u.iodata.nid) )
        {    
            if (trace_settings & (TRACE_SYNC | TRACE_REDIRECTION))
                trace_printf("%s@%d - processing IO Data for (%d, %d)\n", method_name, __LINE__, recv_msg->u.iodata.nid, recv_msg->u.iodata.pid);

            lnode = Nodes->GetLNode( recv_msg->u.iodata.nid );
            if ( lnode )
            {
                process = lnode->GetProcessL(recv_msg->u.iodata.pid);

                if (process)
                {
                    int fd;
                    if (recv_msg->u.iodata.ioType == STDIN_DATA)
                    {
                        fd = process->FdStdin();
                    }
                    else
                    {
                        fd = process->FdStdout();
                    }
                    Redirector.disposeIoData(fd,
                                             recv_msg->u.iodata.length,
                                             recv_msg->u.iodata.data);
                }
                else
                {
                    char buf[MON_STRING_BUF_SIZE];
                    snprintf(buf, sizeof(buf), "[%s], Can't find process nid"
                             "=%d, pid=%d for processing IO Data.\n",
                             method_name,
                             recv_msg->u.iodata.nid, recv_msg->u.iodata.pid);
                    mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_7, SQ_LOG_ERR,
                                  buf); 
                }
            }
        }
        break;

    case InternalType_StdinReq:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal STDIN request\n", method_name, __LINE__);
        if ( !MyNode->IsMyNode(recv_msg->u.stdin_req.supplier_nid) )
        {
            break;

        }

        if (trace_settings & (TRACE_SYNC | TRACE_REDIRECTION))
            trace_printf("%s@%d - stdin request from (%d,%d)"
                         ", type=%d, for supplier (%d, %d)\n",
                         method_name, __LINE__,
                         recv_msg->u.stdin_req.nid,
                         recv_msg->u.stdin_req.pid,
                         recv_msg->u.stdin_req.reqType,
                         recv_msg->u.stdin_req.supplier_nid,
                         recv_msg->u.stdin_req.supplier_pid);

        lnode = Nodes->GetLNode( recv_msg->u.stdin_req.nid );
        if ( lnode == NULL )
        {
            break;
        }
        process = lnode->GetProcessL(recv_msg->u.stdin_req.pid);

        if (process)
        {
            if (recv_msg->u.stdin_req.reqType == STDIN_REQ_DATA)
            {
                // Set up to forward stdin data to requester.
                // Save file descriptor associated with stdin
                // so can find the redirector object later.
                CProcess *supProcess;
                lnode = Nodes->GetLNode( recv_msg->u.stdin_req.supplier_nid );
                if ( lnode )
                {
                    supProcess = lnode->GetProcessL ( recv_msg->u.stdin_req.supplier_pid );
                    if (supProcess)
                    {
                        int fd;
                        fd = Redirector.stdinRemote(supProcess->infile(),
                                                    recv_msg->u.stdin_req.nid,
                                                    recv_msg->u.stdin_req.pid);
                        process->FdStdin(fd);
                    }
                    else
                    {
                        char buf[MON_STRING_BUF_SIZE];
                        snprintf(buf, sizeof(buf), "[%s], Can't find process "
                                 "nid=%d, pid=%d for stdin data request.\n",
                                 method_name,
                                 recv_msg->u.stdin_req.supplier_nid,
                                 recv_msg->u.stdin_req.supplier_pid);
                        mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_8,
                                      SQ_LOG_ERR, buf); 
                    }
                }
            }
            else if (recv_msg->u.stdin_req.reqType == STDIN_FLOW_OFF)
            {
                Redirector.stdinOff(process->FdStdin());
            }
            else if (recv_msg->u.stdin_req.reqType == STDIN_FLOW_ON)
            {
                Redirector.stdinOn(process->FdStdin());
            }
        }
        else
        {
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s], Can't find process nid=%d, "
                     "pid=%d for stdin data request.\n", method_name,
                     recv_msg->u.stdin_req.nid,
                     recv_msg->u.stdin_req.pid);
            mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_9, SQ_LOG_ERR, buf); 
        }
        break;

    case InternalType_Kill:
        // Queue the kill request for processing by a worker thread.
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal kill request for (%d, %d), abort =%d\n", method_name, __LINE__, recv_msg->u.kill.nid, recv_msg->u.kill.pid, recv_msg->u.kill.persistent_abort);

        ReqQueue.enqueueKillReq( recv_msg->u.kill.nid, recv_msg->u.kill.pid, recv_msg->u.kill.persistent_abort );
        break;
                    

    case InternalType_Process:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal process request\n", method_name, __LINE__);
        if ( MyNode->IsMyNode(recv_msg->u.process.nid) )
        {   // Need to create process on this node.
            // Queue process creation request for handling by worker thread
            ReqQueue.enqueueNewProcReq( &recv_msg->u.process );
        }
        break;

    case InternalType_ProcessInit:
        if ( MyNode->IsMyNode(recv_msg->u.processInit.origNid) )
        {  // New process request originated on this node
            ReqQueue.enqueueProcInitReq( &recv_msg->u.processInit );
        }
        break;

    case InternalType_Open:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal open request for (%d, %d), opened (%d, %d)\n", method_name, __LINE__, recv_msg->u.open.nid, recv_msg->u.open.pid, recv_msg->u.open.opened_nid, recv_msg->u.open.opened_pid);

        ReqQueue.enqueueOpenReq( &recv_msg->u.open );
        break;

    case InternalType_SchedData:
        SaveSchedData( recv_msg );
        break;

    case InternalType_Set:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal set request\n", method_name, __LINE__);
        ReqQueue.enqueueSetReq( &recv_msg->u.set );
        break;

    case InternalType_UniqStr:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal unique string request\n", method_name, __LINE__);
        ReqQueue.enqueueUniqStrReq( &recv_msg->u.uniqstr );
        break;

    case InternalType_Sync:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_TMSYNC))
            trace_printf("%s@%d - Internal sync request for"
                         " Node %s, pnid=%d, SyncType=%d\n",
                         method_name, __LINE__, Node[pnid]->GetName(), pnid,
                         recv_msg->u.sync.type);
        switch (recv_msg->u.sync.type )
        {
        case SyncType_TmSeqNum:
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d - TMSYNC(TmSeqNum) on Node %s (pnid=%d)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
            CoordinateTmSeqNumber(pnid);
            break;
        case SyncType_TmData:
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d - TMSYNC(TmData) on Node %s (pnid=%d)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
            if ( ! MyNode->IsSpareNode() && MyNode->GetPhase() != Phase_Ready )
            {
                MyNode->CheckActivationPhase();
            }
            if ( ! MyNode->IsSpareNode() && MyNode->GetPhase() == Phase_Ready )
            {
                if ( MyNode->GetTmSyncState() == SyncState_Null )
                {
                    // Begin a Slave Sync Start
                    if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                        trace_printf("%s@%d - Slave Sync Start on Node %s (pnid=%d)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
                    TmSyncPNid = pnid;
                    Node[pnid]->SetTmSyncState( recv_msg->u.sync.state );
                    if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                    {
                        trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated (%d)(%s)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid, Node[pnid]->GetTmSyncState(), SyncStateString( Node[pnid]->GetTmSyncState() ));
                    }
                    Monitor->CoordinateTmDataBlock( &recv_msg->u.sync );
                }
                else
                {
                    if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                        trace_printf("%s@%d - Sync State Collision! Node %s (pnid=%d) TmSyncState=(%d)(%s)\n", method_name, __LINE__, MyNode->GetName(), MyPNID, MyNode->GetTmSyncState(), SyncStateString( MyNode->GetTmSyncState()) );
                    if ( MyNode->GetTmSyncState() == SyncState_Continue )
                    {
                        if ( pnid > TmSyncPNid ) 
                            // highest node id will continue
                        {
                            // They take priority ... we abort
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Aborting Slave Sync Start on node %s (pnid=%d)\n", method_name, __LINE__, Node[Monitor->TmSyncPNid]->GetName(), Monitor->TmSyncPNid);
                            MyNode->SetTmSyncState( SyncState_Null );
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated (%d)(%s)\n", method_name, __LINE__, MyNode->GetName(), MyPNID, MyNode->GetTmSyncState(), SyncStateString( MyNode->GetTmSyncState() ) );
                            Monitor->ReQueue_TmSync (false);
                            // Continue with other node's Slave TmSync Start request
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Slave Sync Start on node %s (pnid=%d)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
                            TmSyncPNid = pnid;
                            Node[pnid]->SetTmSyncState( recv_msg->u.sync.state );
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                            {
                                trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated (%d)(%s)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid, Node[pnid]->GetTmSyncState(), SyncStateString( Node[pnid]->GetTmSyncState() ));
                            }
                            Monitor->CoordinateTmDataBlock (&recv_msg->u.sync);
                        }
                    }
                    else if ( MyNode->GetTmSyncState() == SyncState_Start )
                    {
                        // Check if they continue with Master Sync Start
                        if ( pnid > MyPNID )
                            // highest node id will continue
                        {
                            // They take priority ... we abort
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Aborted Master Sync Start\n", method_name, __LINE__);
                            MyNode->SetTmSyncState( SyncState_Null );
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated (%d)(%s)\n", method_name, __LINE__, MyNode->GetName(), MyPNID, MyNode->GetTmSyncState(), SyncStateString( MyNode->GetTmSyncState() ) );
                            // Continue with other node's Slave TmSync Start request
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Slave Sync Start on node %s (pnid=%d)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
                            TmSyncPNid = pnid;
                            Node[pnid]->SetTmSyncState( recv_msg->u.sync.state );
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                            {
                                trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated (%d)(%s)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid, Node[pnid]->GetTmSyncState(), SyncStateString( Node[pnid]->GetTmSyncState() ));
                            }
                            Monitor->CoordinateTmDataBlock (&recv_msg->u.sync);
                        }
                        else
                        {
                            // We continue and assume they abort
                            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                                trace_printf("%s@%d - Continuing with Master Sync Start\n", method_name, __LINE__);
                        }
                    }
                    else
                    {
                        if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                            trace_printf("%s@%d - Invalid TmSync_State\n", method_name, __LINE__);
                    }    
                }    
            }
            break;
        
        case SyncType_TmSyncState:
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d - TMSYNC(TmSyncState) on Node %s (pnid=%d)\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
            break; 
        
        default:
            {
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s], Unknown SyncType from pnid=%d.\n", method_name, pnid);
            mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_12, SQ_LOG_ERR, buf); 
            }
        }
        break;
    default:
        {
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s], Unknown Internal message received, Physical Node=%d.\n", method_name, pnid);
            mon_log_write(MON_CLUSTER_HANDLEOTHERNODE_13, SQ_LOG_ERR, buf); 
        }
    }

    TRACE_EXIT;
}

void CCluster::HandleMyNodeMsg (struct internal_msg_def *recv_msg,
                                int pnid)
{
    const char method_name[] = "CCluster::HandleMyNodeMsg";
    TRACE_ENTRY;

    CProcess *process;
    CLNode  *lnode;

    if (trace_settings & TRACE_SYNC_DETAIL)
        trace_printf("%s@%d - Marking object as replicated, msg type=%d\n",
                     method_name, __LINE__, recv_msg->type);
    switch (recv_msg->type)
    {

    case InternalType_Null:
        if (trace_settings & TRACE_SYNC_DETAIL)
            trace_printf("%s@%d - Physical Node pnid=n%d has nothing to "
                         "update. \n", method_name, __LINE__, pnid);
        break;

    case InternalType_ActivateSpare:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal activate spare request, spare pnid=%d, down pnid=%d\n"
                        , method_name, __LINE__
                        , recv_msg->u.activate_spare.spare_pnid
                        , recv_msg->u.activate_spare.down_pnid);
        break;

    case InternalType_Clone:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal clone request, completed replicating process (%d, %d) %s\n", method_name, __LINE__, recv_msg->u.clone.nid, recv_msg->u.clone.os_pid, (recv_msg->u.clone.backup?" Backup":""));
        break;

    case InternalType_Close:
        break;

    case InternalType_Device:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal device request, completed device processing for ldev %s\n", method_name, __LINE__, recv_msg->u.device.ldev_name);
        break;

    case InternalType_Shutdown:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal shutdown request for level=%d\n", method_name, __LINE__, recv_msg->u.shutdown.level);

        // Queue the shutdown request for processing by a worker thread.
        ReqQueue.enqueueShutdownReq( recv_msg->u.shutdown.level );
        break;

    case InternalType_Down:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal down node request for pnid=%d\n", method_name, __LINE__, recv_msg->u.down.pnid);
        break;

    case InternalType_Up:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal up node request for pnid=%d\n", method_name, __LINE__, recv_msg->u.up.pnid);
        break;

    case InternalType_Dump:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal dump request for nid=%d, pid=%d\n",
                         method_name, __LINE__,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);

        lnode = Nodes->GetLNode( recv_msg->u.dump.nid );
        if ( lnode )
        {
            process = lnode->GetProcessL(recv_msg->u.dump.pid);

            if (process)
            {
                process->DumpBegin(recv_msg->u.dump.dumper_nid,
                                   recv_msg->u.dump.dumper_pid,
                                   recv_msg->u.dump.core_file);
            }
            else
            {
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s], Can't find process nid=%d, "
                         "pid=%d for dump target.\n", method_name,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
                mon_log_write(MON_CLUSTER_HANDLEMYNODE_4, SQ_LOG_ERR, buf);
            }
        }
        break;

    case InternalType_DumpComplete:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal dump-complete request for nid=%d, pid=%d\n",
                         method_name, __LINE__,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
        lnode = Nodes->GetLNode( recv_msg->u.dump.nid );
        if ( lnode )
        {
            process = lnode->GetProcessL(recv_msg->u.dump.pid);

            if (process)
            {
                process->DumpEnd(recv_msg->u.dump.status, recv_msg->u.dump.core_file);
            }
            else
            {
                // Dump completion handled in CProcess::Exit()
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s], Can't find process nid=%d, "
                         "pid=%d for dump complete target.\n", method_name,
                         recv_msg->u.dump.nid, recv_msg->u.dump.pid);
                mon_log_write(MON_CLUSTER_HANDLEMYNODE_5, SQ_LOG_ERR, buf);
            }
        }
        break;

    case InternalType_Exit:
        // Final process exit logic is done in Process_Exit, not here
        // as in the past.
        break;

    case InternalType_Event:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal event request\n", method_name, __LINE__);
        break;
                    
    case InternalType_IoData:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal IO data request\n", method_name, __LINE__);
        break;

    case InternalType_StdinReq:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal STDIN request\n", method_name, __LINE__);
        break;

    case InternalType_Kill:
        // Queue the kill request for processing by a worker thread.
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal kill request for (%d, %d), abort =%d\n", method_name, __LINE__, recv_msg->u.kill.nid, recv_msg->u.kill.pid, recv_msg->u.kill.persistent_abort);

        ReqQueue.enqueueKillReq( recv_msg->u.kill.nid, recv_msg->u.kill.pid, recv_msg->u.kill.persistent_abort );
        break;

    case InternalType_Process:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal process request, completed process replication for (%d, %d) %s\n", method_name,  __LINE__, recv_msg->u.process.pid, recv_msg->u.process.nid, (recv_msg->u.process.backup?" Backup":""));
        break;

    case InternalType_ProcessInit:
        // No action needed
        break;

    case InternalType_Open:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal open request, completed open replication, "
                         "(%d, %d)  opened (%d, %d)\n",
                         method_name, __LINE__,
                         recv_msg->u.open.nid,
                         recv_msg->u.open.pid,
                         recv_msg->u.open.opened_nid,
                         recv_msg->u.open.opened_pid);
        break;

    case InternalType_SchedData:
        // No action needed
        break;

    case InternalType_Set:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal set request, completed replicating key %s::%s\n", method_name, __LINE__, recv_msg->u.set.group, recv_msg->u.set.key);
        break;

    case InternalType_UniqStr:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_PROCESS))
            trace_printf("%s@%d - Internal unique string request, completed replicating (%d, %d)\n", method_name, __LINE__, recv_msg->u.uniqstr.nid, recv_msg->u.uniqstr.id);
        break;

    case InternalType_Sync:
        if (trace_settings & (TRACE_SYNC | TRACE_REQUEST | TRACE_TMSYNC))
            trace_printf("%s@%d - Internal sync request for node %s, pnid=%d, SyncType=%d\n"
                         , method_name, __LINE__, Node[pnid]->GetName(), pnid, recv_msg->u.sync.type);
        switch (recv_msg->u.sync.type )
        {
        case SyncType_TmSeqNum:
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d    - TMSYNC(TmSeqNum) on Node %s (pnid=%d)\n", method_name, __LINE__, Node[MyPNID]->GetName(), MyPNID);
            CoordinateTmSeqNumber(pnid);
            break;
        case SyncType_TmData:
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d    - TMSYNC(TmData) on Node %s (pnid=%d)\n", method_name, __LINE__, Node[MyPNID]->GetName(), MyPNID);
            TmSyncPNid = MyPNID;
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d    - Sync communicated, TmSyncPNid=%d\n", method_name, __LINE__, TmSyncPNid);
            if ( ! MyNode->IsSpareNode() && MyNode->GetPhase() != Phase_Ready )
            {
                MyNode->CheckActivationPhase();
            }
            if ( MyNode->GetTmSyncState() == SyncState_Start &&
                 MyNode->GetPhase() == Phase_Ready &&
                 MyNode->GetNumLNodes() > 1 )
            {
                // Begin a Slave Sync Start to other 
                // logical nodes in my physical node
                if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                    trace_printf("%s@%d - Slave Sync Start on local node %s, pnid=%d\n", method_name, __LINE__, Node[pnid]->GetName(), pnid);
                Monitor->CoordinateTmDataBlock( &recv_msg->u.sync );
            }
            break;
        case SyncType_TmSyncState:
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                trace_printf("%s@%d    - TMSYNC(TmSyncState) on Node %s (pnid=%d)\n", method_name, __LINE__, Node[MyPNID]->GetName(), MyPNID);
            break;    
        default:
            {
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s], Unknown SyncType from node %s, pnid=%d during processing local SyncType.\n", method_name, Node[pnid]->GetName(), pnid);
                mon_log_write(MON_CLUSTER_HANDLEMYNODE_11, SQ_LOG_ERR, buf); 
            }
        }
        break;
                    
    default:
        {
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s], Unknown Internal message received during processing local SyncType for pnid=%d.\n", method_name, pnid);
            mon_log_write(MON_CLUSTER_HANDLEMYNODE_12, SQ_LOG_ERR, buf);   
        }

    }

    TRACE_EXIT;
}



bool CCluster::responsive()
{
    const char method_name[] = "CCluster::responsive";
    TRACE_ENTRY;

    int barrierDiff = barrierCount_ - barrierCountSaved_;

    // if no difference in barrier count, sync thread is not responsive
    if  ( !barrierDiff && isMonInitComplete() )
    {   
        // this proc is called every SYNC_MAX_RESPONSIVE+1 secs
        cumulativeDelaySec_ += CCluster::SYNC_MAX_RESPONSIVE + 1; 

        monSyncResponsive_ = false; // sync thread is no longer responsive

        // if sync thread is stuck in mpi call, one of the following checks will be true
        if ( inBarrier_ )
        {
            mem_log_write(MON_CLUSTER_RESPONSIVE_1, cumulativeDelaySec_, 
                          inBarrier_);
        }
        else // non-mpi took quite long
        {
            mem_log_write(MON_CLUSTER_RESPONSIVE_2, cumulativeDelaySec_); 
        }
    }
    else if (barrierDiff < syncMinPerSec_)
    {
        mem_log_write(MON_CLUSTER_RESPONSIVE_3, barrierDiff, syncMinPerSec_); 
        cumulativeDelaySec_ = 0;
        monSyncResponsive_ = true; // slow but responsive
    }
    else
    {
        cumulativeDelaySec_ = 0;
        monSyncResponsive_ = true; // truely responsive
    }

    barrierCountSaved_ = barrierCount_;

    TRACE_EXIT;

    return monSyncResponsive_;
}

void CCluster::InitializeConfigCluster( void )
{
    int rc;

    const char method_name[] = "CCluster::InitializeConfigCluster";
    TRACE_ENTRY;

    int worldSize;
    MPI_Comm_size (MPI_COMM_WORLD, &worldSize);    
    int rankToPnid[worldSize];
    CurNodes = worldSize;

    if ( IsRealCluster )
    {
        cfgPNodes_ = Nodes->GetClusterConfig()->GetPNodesCount();
    }
    else
    {
        // Set virtual cluster size to collective size
        MPI_Comm_size (MPI_COMM_WORLD, &cfgPNodes_); 

        // For virtual cluster set physical node id equal to rank
        for (int i=0; i<worldSize; ++i)
        {
            rankToPnid[i] = i;

            // Set bit indicating node is up
            upNodes_ |= (1ull << i);
        }
    }
    NumNodes = cfgPNodes_;

    // Build the monitor's configured view of the cluster
    if ( IsRealCluster )
    {   // Map node name to physical node id
        // (for virtual nodes physical node equals "rank" (previously set))
        CClusterConfig *clusterConfig = Nodes->GetClusterConfig();
        MyPNID = clusterConfig->GetPNid( Node_name );
    }
    Nodes->AddNodes( );

    MyNode = Nodes->GetNode(MyPNID);
    Nodes->SetupCluster( &Node, &LNode );

    // The new monitor in a real cluster initializes all 
    // existing nodes to a down state.
    // ReIntegrate() will set the state to up when communication is established.
    if ( IAmIntegrating )
    {
        for (int i=0; i<cfgPNodes_; i++)
        {
            CNode *node = Node[i];
            if ( node->GetPNid() != MyPNID )
            {
                node->SetState( State_Down );
            }
        }
    }
    else
    {
        char *nodeNames = 0;
        if ( IsRealCluster )
        {
            if (trace_settings & TRACE_INIT)
                trace_printf("%s@%d Collecting port numbers and node names "
                             "from all monitors\n", method_name, __LINE__);

            bool nodeStatus[cfgPNodes_];
            for (int i=0; i<cfgPNodes_; ++i)
            {
                nodeStatus[i] = false;
            }

            // Collect port info from other monitors
            char * portNums = new char[worldSize * MPI_MAX_PORT_NAME];

            rc = MPI_Allgather (MyPort, MPI_MAX_PORT_NAME, MPI_CHAR, portNums,
                                MPI_MAX_PORT_NAME, MPI_CHAR, MPI_COMM_WORLD);
            if (rc != MPI_SUCCESS)
            {
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s@%d] MPI_Allgather error=%s\n",
                         method_name, __LINE__, ErrorMsg(rc));
                mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);

                MPI_Abort(MPI_COMM_SELF,99);
            }

            // Exchange Node Names with collective
            nodeNames = new char[worldSize * MPI_MAX_PROCESSOR_NAME];
            rc = MPI_Allgather (Node_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
                                nodeNames, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
                                MPI_COMM_WORLD);
            if (rc != MPI_SUCCESS)
            { 
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s@%d] MPI_Allgather error=%s\n",
                         method_name, __LINE__, ErrorMsg(rc));
                mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);

                MPI_Abort(MPI_COMM_SELF,99);
            }

            // For each node name get corresponding CNode object and
            // store port number in it.
            char * nodeName;
            CNode * node;
            for (int i = 0; i < worldSize; i++)
            {
                nodeName = &nodeNames[ i * MPI_MAX_PROCESSOR_NAME ];
                node = Nodes->GetNode( nodeName );
                if ( node )
                {
                    node->SetPort( &portNums[ node->GetPNid() * MPI_MAX_PORT_NAME] );
                    rankToPnid[i] = node->GetPNid();
                    nodeStatus[rankToPnid[i]] = true;

                    if (trace_settings & TRACE_INIT)
                    {
                        trace_printf("%s@%d rankToPnid[%d]=%d (%s)\n",
                                     method_name, __LINE__, i, rankToPnid[i],
                                     node->GetName());
                    }
                }
                else
                {
                    rankToPnid[i] = -1;

                    // Unexpectedly could not map node name to CNode object
                    char buf[MON_STRING_BUF_SIZE];
                    snprintf(buf, sizeof(buf), "[%s@%d] Unable to find node "
                             "object for node %s\n", method_name, __LINE__,
                             nodeName );
                    mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                }
            }
            delete [] portNums;

            int TmLeaderPNid = LNode[TmLeaderNid]->GetNode()->GetPNid();

            // Any nodes not in the initial MPI_COMM_WORLD are down.
            for (int i=0; i<cfgPNodes_; ++i)
            {
                if ( nodeStatus[i] == false )
                {
                    node = Nodes->GetNode(i);
                    if ( node ) node->SetState( State_Down );

                    // assign new TmLeader if TMLeader node is dead.
                    if (TmLeaderPNid == i) 
                    {
                        AssignTmLeader(i);
                    }
                }
                else
                {   // Set bit indicating node is up
                    upNodes_ |= (1ull << i);
                }
            }
        }

        // Initialize communicators for point-to-point communications
        int myRank;
        MPI_Comm_rank( MPI_COMM_WORLD, &myRank );

        InitClusterComm(worldSize, myRank, rankToPnid);
        InitClusterSocks(worldSize, myRank, nodeNames);
        if (nodeNames) delete [] nodeNames;
    }

    if (trace_settings & TRACE_INIT)
    {
        trace_printf("%s@%d upNodes set: %llx\n",
                     method_name, __LINE__, upNodes_);
    }

    TRACE_EXIT;
}

void CCluster::InitClusterComm(int worldSize, int myRank, int * rankToPnid)
{
    const char method_name[] = "CCluster::InitClusterComm";
    TRACE_ENTRY;

    // Compute an array of "colors" for use with  MPI_Comm_split.
    int *splitColors;
    splitColors = new int[worldSize*worldSize*2];
    int *splitOtherNode;
    splitOtherNode = new int[worldSize*worldSize*2];
    int splitRows = 0;
    for ( int i=0; i<(worldSize*worldSize*2); ++i)
    {
        splitColors[i] = MPI_UNDEFINED;
        splitOtherNode[i] = -1;
    }

    int color = 1;
    bool placed;
    for (int i = 0; i < worldSize; i++)
    {
        for (int j = i+1; j < worldSize; j++)
        {
            // Find a free slot for rank "i" to rank "j"

            placed = false;
            for (int k=0; k<splitRows; ++k)
            {
                if (    splitColors[k*worldSize+i] == MPI_UNDEFINED 
                     && splitColors[k*worldSize+j] == MPI_UNDEFINED )
                {
                    splitColors[k*worldSize+i] = color;
                    splitColors[k*worldSize+j] = color;
                    placed = true;

                    if (myRank == i)
                        splitOtherNode[k] = j;
                    else if (myRank == j)
                        splitOtherNode[k] = i;
                    break;
                }
            }
            if (!placed)
            {   // Need to use a new row
                splitColors[splitRows*worldSize+i] = color;
                splitColors[splitRows*worldSize+j] = color;

                if (myRank == i)
                    splitOtherNode[splitRows] = j;
                else if (myRank == j)
                    splitOtherNode[splitRows] = i;

                ++splitRows;
            }
            
            ++color;
        }
    }

    if (trace_settings & TRACE_INIT)
    {
        trace_printf("%s@%d Created %d splitRows for worldSize=%d, myRank=%d\n",
                     method_name, __LINE__, splitRows, worldSize, myRank);
        string line;
        char fragment[50];
        for (int i=0; i<splitRows; ++i)
        {
            sprintf(fragment, "%s@%d splitColors[%d]=", method_name, __LINE__,
                    i);
            line = fragment;
            for (int j=0; j<worldSize; ++j)
            {
                sprintf(fragment, " %d,", splitColors[i*worldSize+j]);
                line += fragment;
            }
            line += "\n";
            trace_printf(line.c_str());

            trace_printf("%s@%d splitOtherNode[%d]=%d\n", method_name,
                         __LINE__, i, splitOtherNode[i]);
        }
    }

    // Create one communicator for each other rank in MPI_COMM_WORLD
    // This permits point-to-point communication with each rank.
    int myRankInComm;
    MPI_Comm ncomm;
    int nid;

    for (int nSplit=0; nSplit < splitRows; ++nSplit)
    {
        color = splitColors[nSplit*worldSize+myRank];
        MPI_Comm_split(MPI_COMM_WORLD, color, myRank, &ncomm);
        if (ncomm == MPI_COMM_NULL)
        {
            if (splitColors[nSplit*worldSize+myRank] != MPI_UNDEFINED)
            {
                if (trace_settings & TRACE_INIT)
                {
                    trace_printf("%s@%d Rank %d: Unexpected MPI_COMM_NULL from "
                                 "MPI_Comm_split, nSplit=%d\n",
                                 method_name, __LINE__,myRank, nSplit);
                }
            }
        }
        else
        {
            // Set comms_ (communicators) array element for the
            // physical node.
            nid = rankToPnid[splitOtherNode[nSplit]];
            comms_[nid] = ncomm;

            MPI_Comm_rank(ncomm, &myRankInComm);
            otherMonRank_[nid] = (myRankInComm == 0)? 1: 0;

            if (trace_settings & TRACE_INIT)
            {
                trace_printf("%s@%d Rank %d: MPI_Comm_split %d, color=%d, "
                             "comms_[%d] is orig rank #%d, "
                             "otherMonRank_=%d\n",
                             method_name, __LINE__,
                             myRank, nSplit, color,
                             nid, splitOtherNode[nSplit],
                             otherMonRank_[nid]);
            }
        }
    }

    delete [] splitColors;
    delete [] splitOtherNode;

    TRACE_EXIT;
}

void CCluster::HandleReintegrateError( int rc, int err,
                                       int pnid, nodeId_t *nodeInfo,
                                       bool abort )
{
    const char method_name[] = "CCluster::HandleReintegrateError";
    TRACE_ENTRY;

    char buf[MON_STRING_BUF_SIZE];

    switch ( err )
    {
    case Reintegrate_Err1:
        snprintf(buf, sizeof(buf), "[%s], can't to connect to creator monitor"
                 " port: %s - Error: %s.\n",
                 method_name, IntegratingMonitorPort, ErrorMsg(rc));
        break;

    case Reintegrate_Err2:
        snprintf(buf, sizeof(buf), "[%s], can't merge intercomm to existing "
                 "MPI collective - Error: %s.\n",
                 method_name, ErrorMsg(rc));

        break;

    case Reintegrate_Err3:
        snprintf(buf, sizeof(buf), "[%s], unable to obtain cluster info "
                 "from creator monitor: %s.\n", method_name, ErrorMsg(rc));
        break;

    case Reintegrate_Err4:
        snprintf(buf, sizeof(buf), "[%s], Failed to send name/port "
                 "to node %d (%s): %s.\n", method_name, pnid,
                 nodeInfo->nodeName, ErrorMsg(rc));
        break;

    case Reintegrate_Err5:
        snprintf(buf, sizeof(buf), "[%s], can't to connect to "
                 " node %d monitor, port: %s: %s.\n",
                 method_name, pnid, nodeInfo->port, ErrorMsg(rc));
        break;

    case Reintegrate_Err6:
        snprintf(buf, sizeof(buf), "[%s], can't merge intercomm "
                 "for node %d: %s.\n", method_name, pnid,
                 ErrorMsg(rc));
        break;
 
    case Reintegrate_Err7:
        snprintf(buf, sizeof(buf), "[%s], can't disconnect "
                 "intercomm for node %d: %s.\n", method_name, pnid,
                 ErrorMsg(rc));
        break;

    case Reintegrate_Err8:
        snprintf(buf, sizeof(buf), "[%s], Failed to send status to creator "
                 "monitor: %s\n", method_name, ErrorMsg(rc));
        break;

    case Reintegrate_Err9:
        snprintf(buf, sizeof(buf), "[%s], Failed to send name/port "
                 "to creator monitor: %s.\n", method_name, ErrorMsg(rc));
        break;

    case Reintegrate_Err10:
        snprintf(buf, sizeof(buf), "[%s], Monitor initialization failed (could"
                 " not write to port file).  Aborting.\n", method_name);
        break;

    case Reintegrate_Err11:
        snprintf(buf, sizeof(buf), "[%s], Monitor initialization failed (could"
                 " not open port file).  Aborting.\n", method_name);
        break;

    case Reintegrate_Err12:
        snprintf(buf, sizeof(buf), "[%s], Monitor initialization failed (could"
                 " not initialize local io).  Aborting.\n", method_name);
        break;

    case Reintegrate_Err13:
        snprintf(buf, sizeof(buf), "[%s], Monitor initialization failed (could"
                 " not initialize devices).  Aborting.\n", method_name);
        break;

    case Reintegrate_Err14:
        snprintf(buf, sizeof(buf), "[%s] Aborting.\n", method_name);
        break;

    case Reintegrate_Err15:
        snprintf(buf, sizeof(buf), "[%s], no connect acknowlegement "
                 "for node %d: %s.\n", method_name, pnid,
                 ErrorMsg(rc));
        break;

    default:
        snprintf(buf, sizeof(buf), "[%s], Reintegration error: %s\n",
                 method_name, ErrorMsg(rc));
    }

    mon_log_write(MON_CLUSTER_REINTEGRATE_1, SQ_LOG_ERR, buf);
    
    if ( abort )
        MPI_Abort(MPI_COMM_SELF,99);

    TRACE_EXIT;
}

void CCluster::SendReIntegrateStatus( STATE nodeState, int initErr )
{
    int rc;
    nodeStatus_t nodeStatus;
    nodeStatus.state = nodeState;
    nodeStatus.status = initErr;
    if ((rc = Monitor->Send((char *) &nodeStatus, sizeof(nodeStatus_t), 0,
                            MON_XCHNG_DATA, joinComm_)))
        HandleReintegrateError( rc, Reintegrate_Err8, -1, NULL, true );

    if ( nodeState != State_Up )
    {  // Initialization error, abort.

        mem_log_write(CMonLog::MON_REINTEGRATE_9, MyPNID, initErr);

        HandleReintegrateError( rc, initErr, -1, NULL, true );
    }
}

void CCluster::ReIntegrate( int initProblem )
{
    const char method_name[] = "CCluster::ReIntegrate";
    TRACE_ENTRY;

    int rc;
    bool haveCreatorComm = false;
    MPI_Comm interComm;
    MPI_Comm intraComm = MPI_COMM_NULL;
    MPI_Comm intraCommCreatorMon = MPI_COMM_NULL;

    nodeId_t myNodeInfo;
    strcpy(myNodeInfo.nodeName, MyNode->GetName());
    strcpy(myNodeInfo.port, MyPort);
    MyNode->SetPort ( MyPort );
    // Set bit indicating node is up
    upNodes_ = (1ull << MyPNID);

    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
       trace_printf("%s@%d - MPI_Comm_connect to creator monitor (port %s)\n",
                    method_name, __LINE__, IntegratingMonitorPort);

    mem_log_write(CMonLog::MON_REINTEGRATE_1, MyPNID);

    TEST_POINT( TP010_NODE_UP );

    // Connect with my creator monitor
    if ((rc = MPI_Comm_connect( IntegratingMonitorPort,
                                MPI_INFO_NULL, 0, MPI_COMM_SELF, &joinComm_ )))
        HandleReintegrateError( rc, Reintegrate_Err1, -1, NULL, true );

    MPI_Comm_set_errhandler( joinComm_, MPI_ERRORS_RETURN );

    mem_log_write(CMonLog::MON_REINTEGRATE_4, MyPNID);

    TEST_POINT( TP011_NODE_UP );

    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
    {
        trace_printf("%s@%d Connected to creator monitor, sending id\n",
                     method_name, __LINE__); 
    }

    // Send this node's name and port number so creator monitor 
    // knows who we are, and set flag to let creator monitor it is the CREATOR.
    myNodeInfo.creator = true;
    myNodeInfo.creatorShellPid = CreatorShellPid;
    if ((rc = Monitor->Send((char *) &myNodeInfo, sizeof(nodeId_t), 0,
                            MON_XCHNG_DATA, joinComm_)))
        HandleReintegrateError( rc, Reintegrate_Err9, -1, NULL,
                                true );

    TEST_POINT( TP012_NODE_UP );

    // Merge the inter-communicators obtained from the connect/accept
    // between this new monitor and the creator monitor.
    if ((rc = MPI_Intercomm_merge( joinComm_, 1, &intraCommCreatorMon )))
        HandleReintegrateError( rc, Reintegrate_Err2, -1, NULL, true );

    MPI_Comm_set_errhandler( intraCommCreatorMon, MPI_ERRORS_RETURN );

    nodeId_t *nodeInfo = new nodeId_t[cfgPNodes_];

    mem_log_write(CMonLog::MON_REINTEGRATE_3, MyPNID);

    // Obtain node names & port numbers of existing monitors from
    // the creator monitor.
    if ((rc = Monitor->Receive((char *)nodeInfo, sizeof(nodeId_t)*cfgPNodes_,
                               MPI_ANY_SOURCE, MON_XCHNG_DATA, joinComm_)))
        HandleReintegrateError( rc, Reintegrate_Err3, -1, NULL, true );

    if ( initProblem )
    {
        // The monitor encountered an initialization error.  Inform
        // the creator monitor that the node is down.  Then abort.
        SendReIntegrateStatus( State_Down, initProblem );
    }

    // Connect to each of the other existing monitors and let them know 
    // we are the NEW monitor and reset the creator flag so they know they are
    // not the creator monitor.
    myNodeInfo.creator = false;
    myNodeInfo.creatorShellPid = -1;
    for (int i=0; i<cfgPNodes_; i++)
    {
        if (strcmp(nodeInfo[i].port, IntegratingMonitorPort) == 0)
        {   // Already connected to creator monitor
            comms_[i] = intraCommCreatorMon;
            otherMonRank_[i] = 0;
            ++CurNodes;

            // Set bit indicating node is up
            upNodes_ |= (1ull << i);

            Node[i]->SetPort( IntegratingMonitorPort );
            Node[i]->SetState( State_Up );
            haveCreatorComm = true;
        }
        else if (nodeInfo[i].nodeName[0] != 0
                 && nodeInfo[i].port[0] != 0)
        {
            if ( haveCreatorComm && i >= cfgPNodes_/2)
                // Reintegration failure after connecting to half
                // of existing monitors.
                TEST_POINT( TP016_NODE_UP );

            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
            {
                trace_printf("%s@%d Attempting connection to node %d (%s), "
                             "port %s\n", method_name, __LINE__, i,
                             nodeInfo[i].nodeName, nodeInfo[i].port); 
            }

            mem_log_write(CMonLog::MON_REINTEGRATE_5, MyPNID, i);

            TEST_POINT( TP013_NODE_UP );

            // Connect to existing monitor
            if ((rc = MPI_Comm_connect( nodeInfo[i].port,
                                        MPI_INFO_NULL, 0, MPI_COMM_SELF,
                                        &interComm )))
            {
                HandleReintegrateError( rc, Reintegrate_Err5, i, &nodeInfo[i],
                                        false );
                SendReIntegrateStatus( State_Down, Reintegrate_Err14 );
            }

            MPI_Comm_set_errhandler( interComm, MPI_ERRORS_RETURN );

            TEST_POINT( TP014_NODE_UP );

            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
            {
                trace_printf("%s@%d Connected to node %d (%s), sending id\n",
                             method_name, __LINE__,i,nodeInfo[i].nodeName); 
            }

            // Send this nodes name and port number so other monitor
            // knows who we are.
            if ((rc = Monitor->Send((char *) &myNodeInfo, sizeof(nodeId_t), 0,
                                    MON_XCHNG_DATA, interComm)))
            {
                HandleReintegrateError( rc, Reintegrate_Err4, i, &nodeInfo[i],
                                        false );
                SendReIntegrateStatus( State_Down, Reintegrate_Err14 );
            }

            if ((rc = MPI_Intercomm_merge(interComm, 1, &intraComm)))
            {
                HandleReintegrateError( rc, Reintegrate_Err6, i, NULL, false );
                SendReIntegrateStatus( State_Down, Reintegrate_Err14 );
            }

            // Get acknowledgement that other monitor is ready to
            // integrate this node.  This is an interlock to avoid a
            // race condition where the creator monitor could signal
            // the monitors in the cluster to integrate the new node
            // before one or more was ready to do the integration.
            int readyFlag;
            if ((rc = Monitor->Receive((char *) &readyFlag, sizeof(readyFlag),
                                       MPI_ANY_SOURCE, MON_XCHNG_DATA,
                                       interComm)))
            {
                HandleReintegrateError( rc, Reintegrate_Err15, i, NULL,
                                        false );
                SendReIntegrateStatus( State_Down, Reintegrate_Err14 );
            }


            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
            {
                trace_printf( "%s@%d - Received ready-flag from node %d (%s)\n",
                              method_name, __LINE__, i,
                             nodeInfo[i].nodeName); 
            }

            if ((rc = MPI_Comm_disconnect(&interComm)))
                HandleReintegrateError( rc, Reintegrate_Err7, i, NULL, false );

            MPI_Comm_set_errhandler(intraComm, MPI_ERRORS_RETURN);

            comms_[i] = intraComm;
            otherMonRank_[i] = 0;
            ++CurNodes;
            Node[i]->SetPort( nodeInfo[i].port );
            Node[i]->SetState( State_Up );

            // Set bit indicating node is up
            upNodes_ |= (1ull << i);

            mem_log_write(CMonLog::MON_REINTEGRATE_6, MyPNID, i);
        }
        else if ( i != MyPNID)
        {
            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
            {
                trace_printf("%s@%d Connection to node %d not attempted, "
                             "no port information.  nodeInfo[%d].port=%s, "
                             "IntegratingMonitorPort=%s\n", method_name,
                             __LINE__, i, i, nodeInfo[i].port,
                             IntegratingMonitorPort);
            }
        }
    }

    if (trace_settings & TRACE_INIT)
    {
        trace_printf("%s@%d upNodes set: %llx\n",
                     method_name, __LINE__, upNodes_);
    }

    mem_log_write(CMonLog::MON_REINTEGRATE_7, MyPNID);

    TEST_POINT( TP015_NODE_UP );

    // Inform creator monitor that connections are complete and
    // this monitor is ready to participate in "allgather"
    // communications with the other monitors.
    SendReIntegrateStatus( State_Up, 0 );

    mem_log_write(CMonLog::MON_REINTEGRATE_8, MyPNID);

    MyNode->SetState( State_Merged );

    delete[] nodeInfo;

    TRACE_EXIT;
}

// Save information about a new communicator for a node that is reintegrating
void CCluster::addNewComm(int pnid, int otherRank,  MPI_Comm comm)
{
    const char method_name[] = "CCluster::addNewComm";
    TRACE_ENTRY;

    if (trace_settings & TRACE_RECOVERY)
    {
        trace_printf("%s@%d - saving communicator for pnid %d\n",
                     method_name, __LINE__, pnid);
    }

    // Insert info for new comm into list
    commInfo_t commInfo = {pnid, otherRank, comm, {0, 0}};
    clock_gettime(CLOCK_REALTIME, &commInfo.ts);

    newCommsLock_.lock();
    newComms_.push_back( commInfo );
    newCommsLock_.unlock();

    TRACE_EXIT;
}

// A node is reintegrating.   Add the communicator for the node to the set of 
// communicators used by "ag".
void CCluster::setNewComm( int pnid )
{
    const char method_name[] = "CCluster::setNewComm";
    TRACE_ENTRY;

    newComms_t::iterator it;
    bool foundComm = false;

    if ( comms_[pnid] != MPI_COMM_NULL )
    {   // Unexpectedly already have a communicator for this node
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s]  Unexpectedly already have a "
                 "communicator for node %d\n", method_name, pnid);
        mon_log_write(MON_CLUSTER_REINTEGRATE_2, SQ_LOG_ERR, buf);

        MPI_Comm_free( &comms_[pnid] );
        shutdown( socks_[pnid], SHUT_RDWR);
        close( socks_[pnid] );
        socks_[pnid] = -1;
    }

    newCommsLock_.lock();
    for ( it = newComms_.begin(); it != newComms_.end(); )
    {
        if ( it->pnid == pnid )
        {
            if ( comms_[pnid] != MPI_COMM_NULL )
            {   // Found another communicator for the specified node.
                // Disconnect from the previous one.  It must be a
                // stale leftover from a previous reintegration
                // attempt for the node.
                if (trace_settings & TRACE_RECOVERY)
                {
                    trace_printf("%s@%d - discarding stale communicator for "
                                 "pnid %d\n", method_name, __LINE__, pnid);
                }

                MPI_Comm_free( &comms_[pnid] );
                shutdown( socks_[pnid], SHUT_RDWR);
                close( socks_[pnid] );
                socks_[pnid] = -1;
                --CurNodes;
            }

            if (trace_settings & TRACE_RECOVERY)
            {
                trace_printf("%s@%d - setting new communicator for pnid %d, "
                             "otherRank=%d\n",
                             method_name, __LINE__, it->pnid, it->otherRank);
            }

            comms_[it->pnid] = it->comm;
            otherMonRank_[it->pnid] = it->otherRank;
            ++CurNodes;
            // Set bit indicating node is up
            upNodes_ |= (1ull << it->pnid);

            // Delete current list element and advance to next one
            it = newComms_.erase ( it );

            foundComm = true;
        }
        else
        {   // Advance to next list element
            ++it;
        }
    }
    newCommsLock_.unlock();

    if ( !foundComm )
    {  // We have no communicator for the specified node.
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s] Could not find a communicator for "
                 "node %d\n", method_name, pnid);
        mon_log_write(MON_CLUSTER_REINTEGRATE_3, SQ_LOG_ERR, buf);
    }

    TRACE_EXIT;
}


// When we get a communication error for a point-to-point monitor communicator
// verify that the other nodes in the cluster also lost communications
// with that monitor.  If all nodes concur we consider that monitor
// down.
void CCluster::ValidateClusterState( cluster_state_def_t nodestate[],
                                     bool haveDivergence)
{
    const char method_name[] = "CCluster::ValidateClusterState";

    exitedMons_t::iterator it;
    unsigned long long nodeMask;

    for ( it = exitedMons_.begin(); it != exitedMons_.end(); )
    {
        if (trace_settings & TRACE_SYNC)
        {
            trace_printf("%s@%d checking exited monitor pnid=%d, seqNum=%lld"
                         " (current seqNum_=%lld)\n", method_name, __LINE__,
                         it->exitedPnid, it->seqNum, seqNum_);
        }

        if ( seqNum_ >= (it->seqNum + 2) )
        {
            char buf[MON_STRING_BUF_SIZE];
            snprintf( buf, sizeof(buf), "[%s] Validating exited node %d, "
                      "detected by node %d at seq #%lld "
                      "(current seq # is %lld).\n",
                      method_name, it->exitedPnid, it->detectingPnid,
                      it->seqNum, seqNum_);
            mon_log_write(MON_CLUSTER_VALIDATE_STATE_1, SQ_LOG_ERR, buf);

            int concurringNodes = 0;

            // Check if all active nodes see the node as down.
            nodeMask = 1ull << it->exitedPnid;
            string setSeesUp;
            string setSeesDown;
            char nodeX[10];

            for (int pnid = 0; pnid < cfgPNodes_; ++pnid)
            {
                if ( nodestate[pnid].seq_num != 0 )
                {  // There is valid nodestate info from node "pnid"

                    if ((nodestate[pnid].upNodes & nodeMask) == 0)
                    {  // Node "pnid" sees the node as down

                        // temp trace
                        if (trace_settings & TRACE_SYNC)
                        {
                            trace_printf("%s@%d node %d concurs that node %d "
                                         "is down\n", method_name, __LINE__,
                                         pnid, it->exitedPnid);
                        }

                        snprintf(nodeX, sizeof(nodeX), "%d, ", pnid);
                        setSeesDown.append(nodeX);

                        ++concurringNodes;
                    }
                    else
                    {
                        // temp trace
                        if (trace_settings & TRACE_SYNC)
                        {
                            trace_printf("%s@%d node %d says node %d is up\n",
                                         method_name, __LINE__, pnid,
                                         it->exitedPnid);
                        }

                        snprintf(nodeX, sizeof(nodeX), "%d, ", pnid);
                        setSeesUp.append(nodeX);

                    }
                }
                else
                {
                    // temp trace
                    if (trace_settings & TRACE_SYNC)
                    {
                        trace_printf("%s@%d ignoring state from node %d\n",
                                     method_name, __LINE__, pnid);
                    }
                }
            }

            if (trace_settings & TRACE_SYNC)
            {
                trace_printf("%s@%d concurringNodes=%d, CurNodes=%d\n",
                             method_name, __LINE__, concurringNodes, CurNodes);
            }

            if ( concurringNodes == CurNodes)
            {   // General agreement that node is down, proceed to mark it down

                // temp trace
                if (trace_settings & TRACE_SYNC)
                {
                    trace_printf("%s@%d proceeding to mark node %d down\n",
                                 method_name, __LINE__, it->exitedPnid);
                }

                mem_log_write(CMonLog::MON_UPDATE_CLUSTER_3, it->exitedPnid);

                HandleDownNode(it->exitedPnid);
            }
            else if ( concurringNodes != 0 && !enqueuedDown_ )
            {   // Some monitors say the node is down, others don't.
                // This is not supposed to happen.  Enqueue request to
                // bring this node down.  All monitors will do the same
                // so the cluster will be brought down.
                if (setSeesUp.length() > 2)
                    setSeesUp.erase(setSeesUp.length()-2, 2);
                if (setSeesDown.length() > 2)
                    setSeesDown.erase(setSeesDown.length()-2, 2);
                char buf[MON_STRING_BUF_SIZE];
                snprintf( buf, sizeof(buf), "[%s] Lost connection to node "
                          "%d but only %d of %d nodes also lost the "
                          "connection.  See up: %s.  See down: %s.  So node "
                          "%d is going down (at seq #%lld).\n", method_name,
                          it->exitedPnid, concurringNodes, CurNodes,
                          setSeesUp.c_str(), setSeesDown.c_str(),
                          MyPNID, seqNum_ );
                mon_log_write(MON_CLUSTER_VALIDATE_STATE_2, SQ_LOG_ERR, buf);

                mem_log_write(CMonLog::MON_UPDATE_CLUSTER_4, MyPNID,
                              it->exitedPnid);

                enqueuedDown_ = true;
                ReqQueue.enqueueDownReq(MyPNID);
            }

            // Delete current list element and advance to next one
            it = exitedMons_.erase( it );
        }
        else
        {   // Advance to next list element
            ++it;
        }
    }


    if ( haveDivergence )
    {
        char buf[MON_STRING_BUF_SIZE];
        snprintf( buf, sizeof(buf), "[%s] Cluster view divergence (at seq "
                  "#%lld), node %d sees set: %llx\n", method_name,
                  seqNum_, MyPNID, upNodes_ );
        mon_log_write(MON_CLUSTER_VALIDATE_STATE_3, SQ_LOG_ERR, buf);

        // For each "up node" (from local perspective)
        // go through nodestate for each other node. If any node
        // says the node is down, add an item to the exitedMons_ list
        // for examination during the next sync cycle (by which time
        // all nodes will have had a chance to detect the down monitor.)

        for (int pnid = 0; pnid < cfgPNodes_; ++pnid)
        {
            // No need to check local monitor's view of the cluster since
            // any down connections are handled directly when detected.
            if (pnid == MyPNID) continue;

            // Check if all active nodes see the node as up.
            nodeMask = 1ull << pnid;

            if ( upNodes_ & nodeMask )
            {  // This node sees node pnid as up
                for (int pnid2 = 0; pnid2 < cfgPNodes_; ++pnid2)
                {
                    if ( pnid != pnid2
                         &&  (nodestate[pnid].seq_num != 0)
                         &&  (nodestate[pnid2].upNodes & nodeMask) == 0 )
                    {  // Node pnid2 sees node pnid as down

                        // temp trace
                        if (trace_settings & TRACE_SYNC)
                        {
                            trace_printf("%s@%d Divergence, queueing "
                                         "monExited{%d, %d, %lld}\n",
                                         method_name, __LINE__, pnid, pnid2,
                                         seqNum_);
                        }

                        monExited_t monExited = {pnid, pnid2, seqNum_};
                        exitedMons_.push_back( monExited );

                        // No need to look further for any other
                        // monitor's view of node pnid.  When the
                        // exitedMons_ element is processed all nodes
                        // will be checked for concurrence.
                        break;
                    }
                }
            }
        }
    }
}

bool CCluster::ValidateSeqNum( cluster_state_def_t nodestate[] )
{
    const char method_name[] = "CCluster::ValidateSeqNum";

    unsigned long long seqNum;
    unsigned long long seqNumBucket[256];
    int seqNumCount[256];
    int maxBucket = 0;
    bool found;
    int mostCountsIndex;

    if ( cfgPNodes_ ==  1 ) return true;

    // Count occurrences of sequence numbers from other nodes
    for (int pnid = 0; pnid < cfgPNodes_; pnid++)
    {
        seqNum = nodestate[pnid].seq_num;

        if (seqNum != 0)
        {
            found = false;
            for (int i=0; i<maxBucket; ++i)
            {
                if ( seqNum == seqNumBucket[i] )
                {
                    ++seqNumCount[i];
                    found = true;
                    break;
                }
            }
            if ( ! found )
            {
                seqNumBucket[maxBucket] = seqNum;
                seqNumCount[maxBucket] = 1;
                ++maxBucket;
            }
        }
    }

    if ( maxBucket == 0 )
    {  // Normal case, all nodes have same sequence number
        mostCountsIndex = 0;
    }
    else
    {  // Look for majority sequence number
        int mostCounts = 0;
        mostCountsIndex = 0;
        for (int i=0; i<maxBucket; ++i)
        {
            if ( seqNumCount[i] > mostCounts )
            {
                mostCounts = seqNumCount[i];
                mostCountsIndex = i;
            }
        }
    }

    if (trace_settings & TRACE_SYNC)
    {
        if ( seqNum_ != seqNumBucket[mostCountsIndex] )
        {
            trace_printf("%s@%d Most common seq num=%lld (%d nodes), %d buckets"
                         ", local seq num (%lld) did not match.\n",
                         method_name, __LINE__, seqNumBucket[mostCountsIndex],
                         seqNumCount[mostCountsIndex], maxBucket, seqNum_);
        }
    }

    // Fail if my seqnum does not match majority
    return seqNum_ == seqNumBucket[mostCountsIndex];
}

void CCluster::HandleDownNode( int pnid )
{
    const char method_name[] = "CCluster::HandleDownNode";
    TRACE_ENTRY;

    // Add to dead node name list
    CNode *downNode = Nodes->GetNode( pnid );
    assert(downNode);
    deadNodeList_.push_back( downNode );
    
    if (trace_settings & TRACE_INIT)
        trace_printf("%s@%d - Added down node to list, pnid=%d, name=(%s)\n", method_name, __LINE__, downNode->GetPNid(), downNode->GetName());

    // assign new TmLeader if TMLeader node is dead.
    AssignTmLeader(pnid);

    // Build available list of spare nodes
    CNode *spareNode;
    NodesList *spareNodesList = Nodes->GetSpareNodesList();
    NodesList::iterator itSn;
    for ( itSn = spareNodesList->begin(); itSn != spareNodesList->end() ; itSn++ ) 
    {
        spareNode = *itSn;
        if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
            trace_printf( "%s@%d - %s (pnid=%d) is in available spare node list, state=%s, spare=%d, rank failure=%d\n"
                        , method_name, __LINE__, spareNode->GetName(), spareNode->GetPNid()
                        , StateString(spareNode->GetState()), spareNode->IsSpareNode(), spareNode->IsRankFailure());
        // if spare node is available
        if ( spareNode->IsSpareNode()    && 
             !spareNode->IsRankFailure() && 
             spareNode->GetState() == State_Up )
        {
            spareNodeVector_.push_back( spareNode );
            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                trace_printf("%s@%d - pnid=%d, name=(%s) is available Spare\n", method_name, __LINE__, spareNode->GetPNid(), spareNode->GetName());
        }
    }
    
    // Activate spare or down node
    NodesList::iterator itDn;
    for ( itDn = deadNodeList_.begin(); itDn != deadNodeList_.end() ; itDn++ ) 
    {
        downNode = *itDn;
        if ( Emulate_Down )
        {
            ReqQueue.enqueueDownReq( downNode->GetPNid() );
        }
        else
        {
            bool done = false;
            spareNode = NULL;
            if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                trace_printf( "%s@%d - spare node vector size=%ld\n"
                            , method_name, __LINE__, spareNodeVector_.size());
            // Find available spare node for current down node
            for ( unsigned int ii = 0; ii < spareNodeVector_.size() && !done ; ii++ )
            {
                PNidVector sparePNids = spareNodeVector_[ii]->GetSparePNids();
                if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                    trace_printf( "%s@%d - spare pnids vector size=%ld\n"
                                , method_name, __LINE__, sparePNids.size());
                // Check each pnid it is configured to spare
                for ( unsigned int jj = 0; jj < sparePNids.size(); jj++ )
                {
                    if (trace_settings & (TRACE_INIT | TRACE_RECOVERY))
                        trace_printf( "%s@%d - %s (pnid=%d) is in spare node vector[%d], size=%ld\n"
                                    , method_name, __LINE__
                                    , spareNodeVector_[ii]->GetName()
                                    , spareNodeVector_[ii]->GetPNid()
                                    , jj, sparePNids.size());
                    // if this is a spare for the down node
                    if ( spareNodeVector_[ii]->IsSpareNode() &&
                         downNode->GetPNid() == sparePNids[jj] )
                    {
                        // assign it and remove it from the vector
                        spareNode = spareNodeVector_[ii];
                        spareNodeVector_.erase( spareNodeVector_.begin() + ii );
                        done = true;
                        break;
                    }
                }
            }

            if ( spareNode )
            {
                Nodes->RemoveFromSpareNodesList( spareNode );
                downNode->SetState( State_Takeover ); // change state so that pending requests could fail.
                spareNode->SetActivatingSpare( true );
                if ( spareNode->GetPNid() == MyPNID )
                {
                    ReqQueue.enqueueActivateSpareReq( spareNode, downNode, true );
                }
            }
            else 
            {
                if ( downNode->IsSpareNode() )
                {
                    Nodes->RemoveFromSpareNodesList( downNode );
                }
                ReqQueue.enqueueDownReq( downNode->GetPNid() );
            }
        }
    }

    spareNodeVector_.clear();
    deadNodeList_.clear();

    TRACE_EXIT;
}

void CCluster::UpdateClusterState( bool & fl_shutdown,
                                   struct sync_buffer_def * syncBuf,
                                   MPI_Status *status,
                                   int sentChangeNid)
{
    const char method_name[] = "CCluster::UpdateClusterState";
    TRACE_ENTRY;

    struct sync_buffer_def *recvBuf;
    STATE node_state;
    int change_nid;
    cluster_state_def_t nodestate[cfgPNodes_];
    bool clusterViewDivergence = false;


    // Populate nodestate array using node state info from "allgather"
    // along with local node state.
    for (int pnid = 0; pnid < cfgPNodes_; pnid++)
    {
        // Only process active nodes
        if (comms_[pnid] == MPI_COMM_NULL 
        || status[pnid].MPI_ERROR != MPI_SUCCESS)
        {
            // Not an active node, set default values
            nodestate[pnid].node_state = State_Unknown;
            nodestate[pnid].change_nid = -1;
            nodestate[pnid].seq_num     = 0;
            nodestate[pnid].upNodes     = 0;

            continue;
        }

        recvBuf = (struct sync_buffer_def *)
            (((char *) syncBuf) + pnid * CommBufSize);

        if (trace_settings & TRACE_SYNC_DETAIL)
        {
            int nr;
            MPI_Get_count(&status[pnid], MPI_CHAR, &nr);
            trace_printf("%s@%d - Received %d bytes from node %d, "
                         "message count=%d\n",
                         method_name, __LINE__, nr, pnid,
                         recvBuf->msgInfo.msg_count);
        }

        nodestate[pnid].node_state  = recvBuf->nodeInfo.node_state;
        nodestate[pnid].change_nid  = recvBuf->nodeInfo.change_nid;
        nodestate[pnid].seq_num     = recvBuf->nodeInfo.seq_num;
        nodestate[pnid].upNodes     = recvBuf->nodeInfo.upNodes;

        if ( nodestate[pnid].upNodes != upNodes_ ) 
        {
fprintf(stderr, "%s@%d - Divergence  (at seq #%lld), node %s "
                              "(pnid=%d) sees cluster state %llx, local "
                              "monitor sees %llx\n",
                              method_name, __LINE__, seqNum_, 
                              Node[pnid]->GetName(),
                              pnid, nodestate[pnid].upNodes, upNodes_);
            if (trace_settings & TRACE_SYNC)
            {
                trace_printf( "%s@%d - Divergence  (at seq #%lld), node %s "
                              "(pnid=%d) sees cluster state %llx, local "
                              "monitor sees %llx\n",
                              method_name, __LINE__, seqNum_, 
                              Node[pnid]->GetName(),
                              pnid, nodestate[pnid].upNodes, upNodes_);
            }
            clusterViewDivergence = true;
        }

        if (trace_settings & (TRACE_SYNC_DETAIL | TRACE_TMSYNC))
        {
           trace_printf( "%s@%d - Node %s (pnid=%d) TmSyncState=(%d)(%s)\n"
                       , method_name, __LINE__
                       , Node[pnid]->GetName()
                       , pnid
                       , recvBuf->nodeInfo.tmSyncState
                       , SyncStateString( recvBuf->nodeInfo.tmSyncState ));
        }

        if ( Node[pnid]->GetTmSyncState() != recvBuf->nodeInfo.tmSyncState )
        {    
            Node[pnid]->SetTmSyncState(recvBuf->nodeInfo.tmSyncState);
            if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
            {
                trace_printf("%s@%d - Node %s (pnid=%d) TmSyncState updated"
                             " (%d)(%s)\n", method_name, __LINE__,
                             Node[pnid]->GetName(), pnid,
                             recvBuf->nodeInfo.tmSyncState,
                             SyncStateString( recvBuf->nodeInfo.tmSyncState ));
            }
        }

        // Check if we need to increase my node's shutdown level ...
        // all nodes should be at the highest level selected from any source
        if ( MyNode->GetShutdownLevel() < recvBuf->nodeInfo.sdLevel )
        {
            MyNode->SetShutdownLevel( recvBuf->nodeInfo.sdLevel );
            if (MyNode->GetState() == State_Up)
            {
                MyNode->SetState( State_Shutdown );
            }
            if (trace_settings & (TRACE_REQUEST | TRACE_SYNC))
                trace_printf("%s@%d - Node %s Shutdown Level updated (%d)\n",
                             method_name, __LINE__,
                             Node[pnid]->GetName(), recvBuf->nodeInfo.sdLevel);
        }

        Node[pnid]->SetInternalState( recvBuf->nodeInfo.internalState );
        if ( recvBuf->nodeInfo.internalState == State_Ready_To_Exit )
        {   // The node is exiting.  Don't communicate with it any more.
            if (trace_settings & (TRACE_REQUEST | TRACE_SYNC))
                trace_printf("%s@%d - Node %s (%d) ready to exit, setting comm "
                             "to null\n", method_name, __LINE__,
                             Node[pnid]->GetName(), pnid);

            MPI_Comm_free( &comms_[pnid] );
            shutdown( socks_[pnid], SHUT_RDWR);
            close( socks_[pnid] );
            socks_[pnid] = -1;
            Node[pnid]->SetState( State_Down );
            --CurNodes;
            // Clear bit in set of "up nodes"
            upNodes_ &= ~(1ull << pnid);
        }
    }

    if ( checkSeqNum_ && !ValidateSeqNum( nodestate ) && !enqueuedDown_ )
    {
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s] Sync cycle sequence number (%lld) "
                 "incorrect.  Scheduling node down.\n", method_name, seqNum_);
        mon_log_write(MON_CLUSTER_UPDTCLUSTERSTATE_1, SQ_LOG_ERR, buf);

        mem_log_write(CMonLog::MON_UPDATE_CLUSTER_2, MyPNID);

        enqueuedDown_ = true;

        ReqQueue.enqueueDownReq(MyPNID);
    }

    nodestate[MyPNID].node_state = Node[MyPNID]->GetState();
    nodestate[MyPNID].change_nid = sentChangeNid;
    nodestate[MyPNID].seq_num = seqNum_;
    nodestate[MyPNID].upNodes = upNodes_;

    // Examine status returned from MPI receive requests
    for (int pnid = 0; pnid < cfgPNodes_; pnid++)
    {
        if (comms_[pnid] == MPI_COMM_NULL ) continue;
        
        if (status[pnid].MPI_ERROR != MPI_SUCCESS)
        { 
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s], MPI communications error=%s "
                     "(%d) for node %d (at seq #%lld).\n", method_name,
                     ErrorMsg(status[pnid].MPI_ERROR), status[pnid].MPI_ERROR,
                     pnid,  seqNum_);
            mon_log_write(MON_CLUSTER_UPDTCLUSTERSTATE_1, SQ_LOG_ERR, buf); 

            if ( status[pnid].MPI_ERROR == MPI_ERR_EXITED )
            {   // A monitor has gone away

                mem_log_write(CMonLog::MON_UPDATE_CLUSTER_1, pnid);

                MPI_Comm_free( &comms_[pnid] );
                shutdown( socks_[pnid], SHUT_RDWR);
                close( socks_[pnid] );
                socks_[pnid] = -1;
                --CurNodes;

                // Clear bit in set of "up nodes"
                upNodes_ &= ~(1ull << pnid);

                // Pretend node is still up until down node processing
                // completes.
                nodestate[pnid].node_state = State_Unknown;
                nodestate[pnid].change_nid  = -1;
                nodestate[pnid].seq_num     = 0;
                nodestate[pnid].upNodes     = 0;

                if ( validateNodeDown_ )
                {
                    // Save info for the exited monitor so can confirm
                    // that all monitors have the same view.
                    monExited_t monExited = {pnid, MyPNID, seqNum_};
                    exitedMons_.push_back( monExited );
                }
                else
                {
                    HandleDownNode(pnid);
                }
            }
        }
    }

    if ( validateNodeDown_ )
        ValidateClusterState( nodestate, clusterViewDivergence );

    if (trace_settings & (TRACE_SYNC_DETAIL | TRACE_TMSYNC))
    {
       trace_printf( "%s@%d - Node %s (pnid=%d) TmSyncState=(%d)(%s)\n"
                   , method_name, __LINE__
                   , MyNode->GetName()
                   , MyPNID
                   , tmSyncBuffer_->nodeInfo.tmSyncState
                   , SyncStateString( tmSyncBuffer_->nodeInfo.tmSyncState ));
    }

    // Update our node states
    for (int pnid = 0; pnid < cfgPNodes_; pnid++)
    {
        node_state = (STATE)nodestate[pnid].node_state;
        change_nid = nodestate[pnid].change_nid;

        if ( pnid == MyPNID && MyNode->GetState() == State_Merged
             && seqNum_ == 0)
        {   // Initial "allgather" for this re-integrated monitor.

            seqNum_ = EnsureAndGetSeqNum(nodestate);

            if (trace_settings & TRACE_SYNC_DETAIL)
            {
                trace_printf("%s@%d Completed initial allgather for pnid=%d, "
                             "state=%d(%s), seqNum_=%lld\n", method_name, __LINE__,
                             pnid, MyNode->GetState(), 
                             StateString(MyNode->GetState()), seqNum_ );
            }

            // Queue the node up request for processing by a
            // worker thread.
            ReqQueue.enqueueUpReq( MyPNID, NULL, -1 );
        }

        if ( change_nid == MyPNID )
        {
            if( MyNode->GetState() == State_Down ||
                MyNode->GetState() == State_Merged || 
                MyNode->GetState() == State_Joining )
            {
                if (trace_settings & TRACE_RECOVERY)
                    trace_printf( "%s@%d enqueueing node up, state=%s\n",
                                  method_name, __LINE__,
                                  StateString(MyNode->GetState()) );

                // Queue the node up request for processing by a
                // worker thread.
                ReqQueue.enqueueUpReq( MyPNID, NULL, -1 );
            }
            else
            {   // This node is being "downed"

                if (trace_settings & TRACE_RECOVERY)
                    trace_printf( "%s@%d enqueueing node down, state=%s\n",
                                  method_name, __LINE__,
                                  StateString(MyNode->GetState()) );

                // Queue the node down request for processing by a
                // worker thread.
                ReqQueue.enqueueDownReq( MyPNID );
            }
        }
        else
        {
            // In a real cluster, existing monitors need to merge new
            // monitor.

            CNode *pnode = change_nid != -1 ? Nodes->GetNode( change_nid ) : NULL;
            if ( ! Emulate_Down && change_nid != -1 && pnode )
            {
                switch ( pnode->GetState() )
                {
                case State_Down:
                    if (trace_settings & TRACE_RECOVERY)
                        trace_printf( "%s@%d - change_nid=%d, state=%s, "
                                      "queueing up request\n",
                                      method_name, __LINE__ , change_nid,
                                      StateString(pnode->GetState()));

                    mem_log_write(CMonLog::MON_UPDATE_CLUSTER_5, change_nid);

                    // Queue the node up request for processing by a
                    // worker thread.
                    ReqQueue.enqueueUpReq( change_nid,
                                           (char *)pnode->GetName(),
                                           -1 );
                    break;
                case State_Merging:
                    if (trace_settings & TRACE_RECOVERY)
                        trace_printf( "%s@%d - change_nid=%d, state=%s, "
                                      "queueing up request\n",
                                      method_name, __LINE__ , change_nid,
                                      StateString(pnode->GetState()));

                    mem_log_write(CMonLog::MON_UPDATE_CLUSTER_6, change_nid);

                    setNewComm(change_nid);
                    pnode->SetState( State_Merged );
                    ReqQueue.enqueueUpReq( change_nid,
                                           (char *)pnode->GetName(),
                                           -1 );
                    break;
 
                case State_Merged:
                case State_Joining:
                default:
                    if (trace_settings & TRACE_RECOVERY)
                        trace_printf( "%s@%d - change_nid=%d, state=%s, "
                                      "no action required.\n",
                                      method_name, __LINE__ , change_nid,
                                      StateString( pnode->GetState() ));
                    break;
                }
            }
        }
        switch ( node_state )
        {
        case State_Up:
        case State_Joining:
        case State_Merged:
        case State_Merging:
        case State_Initializing:
        case State_Unlinked:
        case State_Unknown:
           break;
        case State_Down:
            fl_shutdown = true;
            break;
        case State_Stopped:
        case State_Shutdown:
            if (trace_settings & TRACE_SYNC_DETAIL)
                trace_printf("%s@%d - Node %d is stopping.\n", method_name, __LINE__, pnid);
            Node[pnid]->SetState( (STATE) node_state );
            fl_shutdown = true;
            break;
        default:
            if (trace_settings & TRACE_SYNC)
                trace_printf("%s@%d - Node %d in unknown state (%d).\n",
                             method_name, __LINE__, pnid, node_state);
        }
    }

    TRACE_EXIT;
}


bool CCluster::ProcessClusterData( struct sync_buffer_def * syncBuf,
                                   struct sync_buffer_def * sendBuf,
                                   bool deferredTmSync )
{
    const char method_name[] = "CCluster::ProcessClusterData";
    TRACE_ENTRY;

    // Using the data returned from Allgather, process replication data
    // from all nodes.  If there are any TmSync messages from other
    // nodes, defer processing until all other replicated data are 
    // processed.
    struct internal_msg_def *msg;
    struct sync_buffer_def *msgBuf;
    bool haveDeferredTmSync = false;

    for (int pnid = 0; pnid < cfgPNodes_; pnid++)
    {
        // Only process active nodes
        if (comms_[pnid] == MPI_COMM_NULL && pnid != MyPNID) continue;

        if ( pnid == MyPNID )
        {   // Get pointer to message sent by this node
            msgBuf = sendBuf;
        }
        else
        {   // Compute pointer to receive buffer element for node "i"
            msgBuf = (struct sync_buffer_def *)
                (((char *) syncBuf) + pnid * CommBufSize);
        }

        // reset msg length to zero to initialize for PopMsg()
        msgBuf->msgInfo.msg_offset = 0;

        if ( msgBuf->msgInfo.msg_count == 1 
        && (( internal_msg_def *)msgBuf->msg)->type == InternalType_Sync )
        {
            if ( deferredTmSync )
            {   // This node has sent a TmSync message.  Process it now.
                if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                    trace_printf("%s@%d - Handling deferred TmSync message for "
                                 "node %d\n", method_name, __LINE__, pnid);

                struct internal_msg_def *msg;
                msg = Nodes->PopMsg( msgBuf );

                if ( pnid == MyPNID )
                    HandleMyNodeMsg (msg, MyPNID);
                else
                    HandleOtherNodeMsg (msg, pnid);
            }
            else
            {
                // This node has sent a TmSync message.  Defer processing
                // until we handle all of the non-TmSync messages from
                // other nodes.
                haveDeferredTmSync = true;

                if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
                    trace_printf("%s@%d - Deferring TmSync processing for node"
                                 " %d until replicated data is handled\n",
                                 method_name, __LINE__, pnid);
            }
        }
        else if ( !deferredTmSync )
        {
            // temp trace
            if (trace_settings & TRACE_SYNC_DETAIL)
            {
                trace_printf("%s@%d - For node %d, msg_count=%d, msg_offset"
                             "=%d\n", method_name, __LINE__, pnid,
                             msgBuf->msgInfo.msg_count,
                             msgBuf->msgInfo.msg_offset);
            }
            do
            {
                // Get the next sync msg for the node
                msg = Nodes->PopMsg( msgBuf );
                if (msg->type == InternalType_Null) break;

                if ( pnid == MyPNID )
                    HandleMyNodeMsg (msg, MyPNID);
                else
                    HandleOtherNodeMsg (msg, pnid);
            }
            while ( true );
        }
    }

    TRACE_EXIT;

    return haveDeferredTmSync;
}

bool CCluster::checkIfDone (  )
{
    const char method_name[] = "CCluster::checkIfDone";
    TRACE_ENTRY;

    if (trace_settings & TRACE_SYNC_DETAIL)
        trace_printf("%s@%d - Node %d shutdown level=%d, state=%s.  Process "
                     "count=%d, internal state=%d, CurNodes=%d, "
                     "local process count=%d\n",
                     method_name, __LINE__, MyNode->GetPNid(),
                     MyNode->GetShutdownLevel(),
                     StateString(MyNode->GetState()),
                     Nodes->ProcessCount(),
                     MyNode->getInternalState(),
                     CurNodes, MyNode->GetNumProcs());

    // Check if we are also done
    if (( MyNode->GetState() != State_Down    ) &&
        ( MyNode->GetState() != State_Stopped )   )
    {
        if ( MyNode->GetShutdownLevel() != ShutdownLevel_Undefined )
        {
            if ( Nodes->ProcessCount() == 0 )  // all WDTs exited
            {
                if (trace_settings & TRACE_SYNC)
                   trace_printf("%s@%d - Monitor signaled to exit.\n", method_name, __LINE__);
                MyNode->SetState( State_Stopped );
                MyNode->SetInternalState(State_Ready_To_Exit);

                // we need to sync one more time so other nodes see our state
                return false;
            }
            else if ( (Nodes->ProcessCount() <= CurNodes) // only WDTs are alive
                      && !MyNode->isInQuiesceState()      // post-quiescing will
                                                          //    expire WDT
                      && !waitForWatchdogExit_ )          // watchdog not yet
                                                          //    exiting
            {
                if (trace_settings & TRACE_SYNC)
                   trace_printf("%s@%d - Stopping watchdog process.\n",
                                method_name, __LINE__);

                waitForWatchdogExit_ = true;
                // stop the watchdog timer first
                HealthCheck.setState(MON_STOP_WATCHDOG);
                // let the watchdog process exit
                HealthCheck.setState(MON_EXIT_WATCHDOG);
            }
        }
    }
    else if ( MyNode->GetShutdownLevel() != ShutdownLevel_Undefined
              && MyNode->GetState() == State_Down 
              && MyNode->GetNumProcs() == 0)
    {
        if (trace_settings & TRACE_SYNC)
            trace_printf("%s@%d - No processes remaining, monitor exiting.\n",
                         method_name, __LINE__);

        MyNode->SetState( State_Stopped );
        MyNode->SetInternalState(State_Ready_To_Exit);
        // we need to sync one more time so other nodes see our state
        return false;
    }

    MyNode->CheckShutdownProcessing();

    TRACE_EXIT;

    return ( MyNode->getInternalState() == State_Ready_To_Exit );
}


// Gather "ag" performance statistics
// Given the beginning and ending time of an "ag" operation, compute
// the elapsed time and increment the count for the appropriate range
// bucket.

const struct timespec CCluster::agBuckets_[] = {
    {0,         0},  // lowest
    {0,     20000},  // 20 us
    {0,     50000},  // 50 us
    {0,    500000},  // 500 us
    {0,   1000000},  // 1 ms
    {0,  10000000},  // 10 ms
    {0,  25000000},  // 25 ms
    {0,  50000000},  // 50 ms
    {0, 100000000},  // 100 ms
    {0, 500000000}}; // 500 ms
const int CCluster::agBucketsSize_ = sizeof(agBuckets_)/sizeof(timespec);

bool CCluster::agTimeStats(struct timespec & ts_begin,
                           struct timespec & ts_end)
{
    const char method_name[] = "CCluster::agTimeStats";
    bool slowAg = false;

    struct timespec timediff;
    if ( (ts_end.tv_nsec - ts_begin.tv_nsec )  < 0 )
    {
        timediff.tv_sec = ts_end.tv_sec - ts_begin.tv_sec - 1;
        timediff.tv_nsec = 1000000000 + ts_end.tv_nsec - ts_begin.tv_nsec;
    }
    else
    {
        timediff.tv_sec = ts_end.tv_sec - ts_begin.tv_sec;
        timediff.tv_nsec = ts_end.tv_nsec - ts_begin.tv_nsec;
    }

    if ( timediff.tv_sec > agMaxElapsed_.tv_sec
         || (timediff.tv_sec == agMaxElapsed_.tv_sec 
             && timediff.tv_nsec > agMaxElapsed_.tv_nsec ))
        // Have a new maximum elapsed time
        agMaxElapsed_ = timediff;

    if ( timediff.tv_sec < agMinElapsed_.tv_sec
         || (timediff.tv_sec == agMinElapsed_.tv_sec
             && timediff.tv_nsec < agMinElapsed_.tv_nsec ))
        // Have a new minimum time
        agMinElapsed_ = timediff;

    for (int i=agBucketsSize_-1; i>=0; --i)
    {
        if (timediff.tv_sec > agBuckets_[i].tv_sec
            || (timediff.tv_sec == agBuckets_[i].tv_sec
                && timediff.tv_nsec > agBuckets_[i].tv_nsec ))
        {
            ++agElapsed_[i];
            if (i >= 7)
            {
                slowAg = true;
                if (trace_settings & TRACE_SYNC)
                {
                    trace_printf("%s@%d slow ag=(%ld, %ld) seqNum_=%lld, i=%d\n",
                                 method_name, __LINE__,
                                 timediff.tv_sec, timediff.tv_nsec, seqNum_, i);
                }
            }
            break;
        }
    }

    return slowAg;
}

// Display "ag" statistics
void CCluster::stats()
{
    const char method_name[] = "CCluster::stats";

    trace_printf("%s@%d ag min elapsed=%ld.%ld\n", method_name, __LINE__,
                 agMinElapsed_.tv_sec, agMinElapsed_.tv_nsec);

    trace_printf("%s@%d ag max elapsed=%ld.%ld\n", method_name, __LINE__,
                 agMaxElapsed_.tv_sec, agMaxElapsed_.tv_nsec);

    unsigned long int bucket;
    const char * unit;
    const char * range;
    for (int i=0; i<agBucketsSize_; ++i)
    {
        if ( i == (agBucketsSize_-1))
        {
            bucket = agBuckets_[i].tv_nsec;
            range = ">";
        }
        else
        {
            bucket = agBuckets_[i+1].tv_nsec;
            range = "<=";
        }
        bucket = bucket/1000;
        if (bucket < 1000) 
            unit = "usec";
        else
        {
            bucket = bucket / 1000;
            if ( bucket < 1000 ) 
                unit = "msec";
            else
                unit = "???";
        }
        trace_printf("%s@%d bucket[%d]=%d (%s %ld %s)\n",
                     method_name, __LINE__, i, agElapsed_[i],
                     range, bucket, unit);
    }
}

bool CCluster::exchangeNodeData ( )
{
    const char method_name[] = "CCluster::exchangeNodeData";
    TRACE_ENTRY;

    // Record statistics (sonar counters)
       MonStats->req_sync_Incr();

    ++swpRecCount_; // recursive count for this function

    bool fl_shutdown = false;

    struct internal_msg_def *msg;
    MPI_Status status[cfgPNodes_];
    int err;
    struct sync_buffer_def *recv_buffer;

    // if we are here in a second recursive call that occurred while
    // processing TMSync data, use the second receive buffer
    // else, use the first one.
    if (swpRecCount_ == 1)
    {
      recv_buffer = recvBuffer_;
    }
    else
    {
      // should not be here in more than one recursive call.
      assert(swpRecCount_ == 2); 
      recv_buffer = recvBuffer2_;
    }

    // Initialize sync buffer header including node state
    msg = Nodes->InitSyncBuffer( Nodes->GetSyncBuffer(), seqNum_, upNodes_ );

    // Fill sync buffer based on queue of replication requests
    Replicator.FillSyncBuffer ( msg );

    if (trace_settings & (TRACE_SYNC_DETAIL | TRACE_TMSYNC))
        trace_printf("%s@%d - doing ag size=%d, message count=%d, seqNum_=%lld\n",
                     method_name, __LINE__, Nodes->GetSyncSize(),
                     (Nodes->GetSyncBuffer())->msgInfo.msg_count, seqNum_);

    struct timespec ts_ag_begin;
    clock_gettime(CLOCK_REALTIME, &ts_ag_begin);


    // Exchange info with other nodes
    err = ag(Nodes->GetSyncSize(), Nodes->GetSyncBuffer(), (char *)recv_buffer,
             0 /*seqNum_*/, status );

    struct timespec ts_ag_end;
    clock_gettime(CLOCK_REALTIME, &ts_ag_end);

    if (err != MPI_SUCCESS && err != MPI_ERR_IN_STATUS)
    {
        if (trace_settings & TRACE_SYNC)
        {
            trace_printf("%s@%d - unexpected ag error=%s (%d)\n",
                         method_name, __LINE__, ErrorMsg(err), err);
        }

        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s], Unexpected MPI communications "
                 "error=%s (%d).\n", method_name, ErrorMsg(err), err);
        mon_log_write(MON_CLUSTER_EXCHANGENODEDATA_1, SQ_LOG_ERR, buf); 

        // ag() failed in a fundamental way, bring this node down
        if ( !enqueuedDown_ )
        {
            enqueuedDown_ = true;
            ReqQueue.enqueueDownReq(MyPNID);
        }
    }
    else
    {
        if (agTimeStats( ts_ag_begin, ts_ag_end))
        {  // Slow cycle, print info
            if ( trace_settings & TRACE_SYNC )
            {
                trace_printf("%s@%d - slow ag info: sync size=%d, message count=%d, MyPNID=%d\n",
                             method_name, __LINE__,  Nodes->GetSyncSize(), 
                             (Nodes->GetSyncBuffer())->msgInfo.msg_count, MyPNID);
                struct sync_buffer_def *msgBuf;
                int nr;

                for (int pnid = 0; pnid < cfgPNodes_; pnid++)
                {
                    // Only process active nodes
                    if (comms_[pnid] == MPI_COMM_NULL) continue;

                    msgBuf = (struct sync_buffer_def *)
                        (((char *) recv_buffer) + pnid * CommBufSize);

                    MPI_Get_count(&status[pnid], MPI_CHAR, &nr);

                    trace_printf("%s@%d - slow ag info, node=%d: received bytes=%d, message count=%d, msg_offset=%d\n",
                                 method_name, __LINE__, pnid, nr,
                                 msgBuf->msgInfo.msg_count,
                                 msgBuf->msgInfo.msg_offset);
                }
            }
        }

        UpdateClusterState( fl_shutdown, recv_buffer, status,
                            Nodes->GetSyncBuffer()->nodeInfo.change_nid);
    }

    // Increment count of "ag" calls.  If wrap-around, start again at 1.
    if ( ++seqNum_ == 0) seqNum_ = 1;

    if ( ProcessClusterData( recv_buffer, Nodes->GetSyncBuffer(), false ) )
    {   // There is a TmSync message remaining to be handled
        ProcessClusterData( recv_buffer, Nodes->GetSyncBuffer(), true );
    }

    // ?? Need the following?  Possibly not since maybe all sync cycle
    // dependent code was removed -- need to check.
    // Wake up any threads waiting on the completion of a sync cycle
    syncCycle_.wakeAll();

    bool result = false;
    if (fl_shutdown) result = checkIfDone( );

    --swpRecCount_;

    TRACE_EXIT;

    return result;
}

void CCluster::exchangeTmSyncData ( struct sync_def *sync )
{
    const char method_name[] = "CCluster::exchangeTmSyncData";
    TRACE_ENTRY;

    ++swpRecCount_; // recursive count for this function

    bool fl_shutdown = false;

    struct internal_msg_def *msg;
    MPI_Status status[cfgPNodes_];
    int err;
    struct sync_buffer_def *recv_buffer;

    // if we are here in a second recursive call that occurred while
    // processing TMSync data, use the second receive buffer
    // else, use the first one.
    if (swpRecCount_ == 1)
    {
      recv_buffer = recvBuffer_;
    }
    else
    {
      // should not be here in more than one recursive call.
      assert(swpRecCount_ == 2); 
      recv_buffer = recvBuffer2_;
    }

    // Initialize sync buffer header including node state
    msg = Nodes->InitSyncBuffer( tmSyncBuffer_, seqNum_, upNodes_ );

    // Add tmsync data
    AddTmsyncMsg( sync, msg );

    if (trace_settings & (TRACE_SYNC_DETAIL | TRACE_TMSYNC))
        trace_printf("%s@%d - tmSyncBuffer_=%p, msg=%p\n",
                     method_name, __LINE__, tmSyncBuffer_, msg);

    int syncSize = sizeof(cluster_state_def_t)
                 + sizeof(msgInfo_t)
                 + tmSyncBuffer_->msgInfo.msg_offset;

    if (trace_settings & (TRACE_SYNC_DETAIL | TRACE_TMSYNC))
        trace_printf("%s@%d - doing ag size=%d, message count=%d, seqNum_=%lld\n",
                     method_name, __LINE__, syncSize,
                     tmSyncBuffer_->msgInfo.msg_count, seqNum_);

    // Exchange info with other nodes
    err = ag(syncSize, tmSyncBuffer_, (char *)recv_buffer, 0 /*seqNum_*/, status );
    if (err != MPI_SUCCESS && err != MPI_ERR_IN_STATUS)
    {
        if (trace_settings & (TRACE_SYNC | TRACE_TMSYNC))
        {
            trace_printf("%s@%d - unexpected ag error=%s (%d)\n",
                         method_name, __LINE__, ErrorMsg(err), err);
        }

        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s], Unexpected MPI communications "
                 "error=%s (%d).\n", method_name, ErrorMsg(err), err);
        mon_log_write(MON_CLUSTER_EXCHANGETMSYNC_1, SQ_LOG_ERR, buf); 

        // ag() failed in a fundamental way, bring this node down
        if ( !enqueuedDown_ )
        {
            enqueuedDown_ = true;
            ReqQueue.enqueueDownReq(MyPNID);
        }
    }
    else
    {
        UpdateClusterState( fl_shutdown, recv_buffer, status,
                            tmSyncBuffer_->nodeInfo.change_nid);
    }

    // Increment count of "ag" calls.  If wrap-around, start again at 1.
    if ( ++seqNum_ == 0) seqNum_ = 1;

    if ( ProcessClusterData( recv_buffer, tmSyncBuffer_, false ) )
    {   // There is a TmSync message remaining to be handled
        ProcessClusterData( recv_buffer, tmSyncBuffer_, true );
    }

    --swpRecCount_;
    
    TRACE_EXIT;
}

static void
epollCtl(int efd, int op, int fd, struct epoll_event *event)
{
    const char method_name[] = "epollCtl";
    TRACE_ENTRY;

    int rc = epoll_ctl(efd, op, fd, event);
    if (rc == -1) {
        char ebuff[256];
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s@%d] epoll_ctl() error: %s\n",
                 method_name, __LINE__, strerror_r(errno, ebuff, 256));
        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
        MPI_Abort(MPI_COMM_SELF,99);
    }

    TRACE_EXIT;
    return;
}

int CCluster::ag(int nbytes, void *sbuf, char *rbuf, int tag, MPI_Status *stats)
{
    const char method_name[] = "CCluster::socketAG";
    TRACE_ENTRY;

    static int hdrSize = Nodes->GetSyncHdrSize();
    int err = MPI_SUCCESS;
    typedef struct {
        int p_sent;
        int p_received;
        int p_n2recv;
        bool p_sending;
        bool p_receiving;
        char *p_buff;
    } peer_t;
    peer_t p[cfgPNodes_];
    memset(p, 0, sizeof(p));
    tag = 0; // make compiler happy

    int nsent = 0, nrecv = 0;
    for ( int iPeer = 0; iPeer < cfgPNodes_; iPeer++ ) {
        peer_t *peer = &p[iPeer];
        stats[iPeer].MPI_ERROR = MPI_SUCCESS;
        stats[iPeer].count = 0;
        if (iPeer == MyPNID || socks_[iPeer] == -1) {
            peer->p_sending = peer->p_receiving = false;
            nsent++;
            nrecv++;
        } else {
            peer->p_sending = peer->p_receiving = true;
            peer->p_sent = peer->p_received = 0;
            peer->p_n2recv = -1;
            peer->p_buff = ((char *) rbuf) + (iPeer * CommBufSize);
            struct epoll_event event;
            event.data.fd = socks_[iPeer];
            event.events = EPOLLIN | EPOLLOUT | EPOLLET;
            epollCtl(epollFD, EPOLL_CTL_ADD, socks_[iPeer], &event);
        }
    }

    inBarrier_ = true;
    MonStats->BarrierWaitIncr();

    // do the work
    struct epoll_event events[2*cfgPNodes_ + 1];
    while (1) {
        int maxEvents = 2*cfgPNodes_ - nsent - nrecv;
        if (maxEvents == 0) break;
        int nw;
        while (1) {
            nw = epoll_wait(epollFD, events, maxEvents, -1);
            if (nw >= 0 || errno != EINTR) break;
        }
        if (nw < 0) {
            char ebuff[256];
            char buf[MON_STRING_BUF_SIZE];
            snprintf(buf, sizeof(buf), "[%s@%d] epoll_wait(%d, %d) error: %s\n",
                     method_name, __LINE__, epollFD, maxEvents,
                     strerror_r(errno, ebuff, 256));
            mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
            MPI_Abort(MPI_COMM_SELF,99);
        }
        for (int iEvent = 0; iEvent < nw; iEvent++) {
            bool stateChange = false;
            int fd = events[iEvent].data.fd;
            int iPeer;
            for (iPeer = 0; iPeer < cfgPNodes_; iPeer++) {
                if (events[iEvent].data.fd == socks_[iPeer]) break;
            }
            if (iPeer < 0 || iPeer >= cfgPNodes_ || iPeer == MyPNID
            || socks_[iPeer] == -1
            || (!p[iPeer].p_sending && !p[iPeer].p_receiving)) {
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s@%d] invalid peer %d\n",
                         method_name, __LINE__, iPeer);
                mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                MPI_Abort(MPI_COMM_SELF,99);
            }
            peer_t *peer = &p[iPeer];
            if ((events[iEvent].events & EPOLLERR) ||
                (events[iEvent].events & EPOLLHUP) ||
                (!(events[iEvent].events & (EPOLLIN|EPOLLOUT))))
            {
              // An error has occured on this fd, or the socket is not
              // ready for reading nor writing
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s@%d] event[i]() error %d\n",
                         method_name, __LINE__, events[iEvent].events);
                mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                stats[iPeer].MPI_ERROR = MPI_ERR_EXITED;
                err = MPI_ERR_IN_STATUS;
                if (peer->p_sending) {
                    peer->p_sending = false;
                    nsent++;
                }
                if (peer->p_receiving) {
                    peer->p_receiving = false;
                    nrecv++;
                }
                stateChange = 1;
                goto early_exit;
	    }
            if (peer->p_receiving
            && events[iEvent].events & EPOLLIN) {
                int eagain_ok = 0;
read_again:
                char *r = &peer->p_buff[peer->p_received];
                int n2get;
                if (peer->p_received >= hdrSize) {
                    n2get = peer->p_n2recv;
                } else {
                    n2get = hdrSize - peer->p_received;
                }
                int nr;
                while (1) {
                    nr = recv(fd, r, n2get, 0);
                    if (nr >= 0 || errno == EINTR) break;
                }
                if (nr <= 0) {
                    if (nr < 0 && eagain_ok && errno == EAGAIN) {
                        // do nothing
                    } else {
                        // error, down socket
                        char buf[MON_STRING_BUF_SIZE];
                        snprintf(buf, sizeof(buf),
                                 "[%s@%d] recv[%d]() error %d\n",
                                 method_name, __LINE__, iPeer, nr);
                        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                        peer->p_receiving = false;
                        nrecv++;
                        if (peer->p_sending) {
                            peer->p_sending = false;
                            nsent++;
                        }
                        stats[iPeer].MPI_ERROR = MPI_ERR_EXITED;
                        err = MPI_ERR_IN_STATUS;
                        stateChange = true;
                    }
                } else {
                    peer->p_received += nr;
                    if (peer->p_received < hdrSize) {
                        // do nothing
                    } else {
                        if (peer->p_received == hdrSize) {
                            // complete header, get buffer size
                            struct sync_buffer_def *sb;
                            sb = (struct sync_buffer_def *)peer->p_buff;
                            peer->p_n2recv = sb->msgInfo.msg_offset;
                            if (peer->p_n2recv) {
                                eagain_ok = 1;
                                goto read_again;
                            }
                        } else {
                            // reading buffer, update counters
                            peer->p_n2recv -= nr;
                        }
                        if (peer->p_n2recv < 0) {
                            char buf[MON_STRING_BUF_SIZE];
                            snprintf(buf, sizeof(buf),
                                     "[%s@%d] error n2recv %d\n",
                                     method_name, __LINE__, peer->p_n2recv);
                            mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                            MPI_Abort(MPI_COMM_SELF,99);
                        }
                        if (peer->p_n2recv == 0) {
                            // this buffer is done
                            peer->p_receiving = false;
                            nrecv++;
                            stats[iPeer].count = peer->p_received;
                            stateChange = true;
                        }
                    }
                }
            }
            if (peer->p_sending
            && events[iEvent].events & EPOLLOUT) {
                char *s = &((char *)sbuf)[peer->p_sent];
                int n2send = nbytes - peer->p_sent;
                int ns;
                while (1) {
                    ns = send(fd, s, n2send, 0);
                    if (ns >= 0 || errno != EINTR) break;
                }
                if (ns <= 0) {
                    // error, down socket
                    char buf[MON_STRING_BUF_SIZE];
                    snprintf(buf, sizeof(buf), "[%s@%d] send[%d]() error %d\n",
                             method_name, __LINE__, iPeer, ns);
                    mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                    peer->p_sending = false;
                    nsent++;
                    if (peer->p_receiving) {
                        peer->p_receiving = false;
                        nrecv++;
                    }
                    stats[iPeer].MPI_ERROR = MPI_ERR_EXITED;
                    err = MPI_ERR_IN_STATUS;
                    stateChange = true;
                } else {
                    peer->p_sent += ns;
                    if (peer->p_sent == nbytes) {
                        // finsished sending to this destination
                        peer->p_sending = false;
                        nsent++;
                        stateChange = true;
                    }
                }
            }
early_exit:
            if (stateChange) {
            	struct epoll_event event;
            	event.data.fd = socks_[iPeer];
                int op;
                if (!peer->p_sending && !peer->p_receiving) {
                    op = EPOLL_CTL_DEL;
                } else if (peer->p_sending) {
                    op = EPOLL_CTL_MOD;
                    event.events = EPOLLOUT | EPOLLET;
                } else {
                    op = EPOLL_CTL_MOD;
                    event.events = EPOLLIN | EPOLLET;
                }
                epollCtl(epollFD, op, fd, &event);
            }
        }
    }

    MonStats->BarrierWaitDecr();
    inBarrier_ = false;

    barrierCount_++;

    TRACE_EXIT;
    return err;
}

static void
InitClusterSocks(int worldSize, int myRank, char *nodeNames)
{
    const char method_name[] = "CCluster::InitClusterComm";
    TRACE_ENTRY;

    serverSock = mksrvsock(&serverSockPort);
    if (serverSock < 0) {
        char ebuff[256];
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s@%d] mksrvsock() error: %s\n",
                 method_name, __LINE__, strerror_r(errno, ebuff, 256));
        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
        MPI_Abort(MPI_COMM_SELF,99);
    }
    epollFD = epoll_create1(EPOLL_CLOEXEC);
    if (epollFD < 0) {
        char ebuff[256];
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s@%d] epoll_create1() error: %s\n",
                 method_name, __LINE__, strerror_r(errno, ebuff, 256));
        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
        MPI_Abort(MPI_COMM_SELF,99);
    }

    // Exchange ports with collective
    int rc = MPI_Allgather (&serverSockPort, 1, MPI_INT,
                        sockPorts_, 1, MPI_INT, MPI_COMM_WORLD);
    if (rc != MPI_SUCCESS) { 
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s@%d] MPI_Allgather error=%s\n",
                 method_name, __LINE__, ErrorMsg(rc));
        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
        MPI_Abort(MPI_COMM_SELF,99);
    }

    char *n, nodeName[MPI_MAX_PROCESSOR_NAME];
    unsigned char srcaddr[4], dstaddr[4];
    struct hostent *he;
    if (nodeNames) {
        n = &nodeNames[myRank*MPI_MAX_PROCESSOR_NAME];
    } else {
        strcpy(nodeName, "localhost");
        n = nodeName;
    }
    he = gethostbyname(n);
    if (!he) {
        char ebuff[256];
        char buf[MON_STRING_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[%s@%d] gethostbyname(%s) error: %s\n",
                 method_name, __LINE__, n, strerror_r(errno, ebuff, 256));
        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
        MPI_Abort(MPI_COMM_SELF,99);
    }
    memcpy(srcaddr, he->h_addr, 4);
    int idst;
    for (int i = 0; i < worldSize; i++) {
        for (int j = i+1; j < worldSize; j++) {
            if (i == myRank) {
                idst = j;
                if (nodeNames) {
                    n = &nodeNames[j*MPI_MAX_PROCESSOR_NAME];
                    he = gethostbyname(n);
                    if (!he) {
                        char ebuff[256];
                        char buf[MON_STRING_BUF_SIZE];
                        snprintf(buf, sizeof(buf),
                                 "[%s@%d] gethostbyname(%s) error: %s\n",
                                 method_name, __LINE__, n,
                                 strerror_r(errno, ebuff, 256));
                        mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                        MPI_Abort(MPI_COMM_SELF,99);
                    }
                    memcpy(dstaddr, he->h_addr, 4);
                } else {
                    memcpy(dstaddr, srcaddr, 4);
                }
                socks_[j] = mkcltsock(srcaddr, dstaddr, sockPorts_[j]);
            } else if (j == myRank) {
                idst = i;
                socks_[i] = acceptsock(serverSock);
            } else {
                idst = -1;
            }
            if (idst >= 0 && socks_[idst] < 0) {
                char buf[MON_STRING_BUF_SIZE];
                if (idst == i) {
                    snprintf(buf, sizeof(buf), "[%s@%d] mkcltsock src %d.%d.%d.%d dst %d.%d.%d.%d failed\n",
                             method_name, __LINE__,
                             (int)((unsigned char *)srcaddr)[0],
                             (int)((unsigned char *)srcaddr)[1],
                             (int)((unsigned char *)srcaddr)[2],
                             (int)((unsigned char *)srcaddr)[3],
                             (int)((unsigned char *)dstaddr)[0],
                             (int)((unsigned char *)dstaddr)[1],
                             (int)((unsigned char *)dstaddr)[2],
                             (int)((unsigned char *)dstaddr)[3]);
                } else {
                    snprintf(buf, sizeof(buf), "[%s@%d] acceptsock(%d) failed\n",
                             method_name, __LINE__, serverSock);
                }
                mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                MPI_Abort(MPI_COMM_SELF,99);
            }
            if (idst >= 0 && fcntl(socks_[idst], F_SETFL, O_NONBLOCK)) {
                char ebuff[256];
                char buf[MON_STRING_BUF_SIZE];
                snprintf(buf, sizeof(buf), "[%s@%d] fcntl(NONBLOCK) error: %s\n",
                         method_name, __LINE__, strerror_r(errno, ebuff, 256));
                mon_log_write(MON_MONITOR_MAIN_7, SQ_LOG_CRIT, buf);
                MPI_Abort(MPI_COMM_SELF,99);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }
}

static int
acceptsock(int sock)

{
	int 		csock;			/* connected socket */

	while (((csock = accept(sock, (struct sockaddr *) 0,
			(socklen_t *) 0)) < 0) && (errno == EINTR));

	if (csock > 0) {
		int reuse;
		if (setsockopt(csock, SOL_SOCKET, SO_REUSEADDR, 
		(char *) &reuse, sizeof(int))) {
			return(-2);		
		}
	}

	return((int)csock);
}

static int
mksrvsock(int *pport)

{
	int	sock;			/* socket */
	int	err;			/* return code */
#if defined(_XOPEN_SOURCE_EXTENDED)
#ifdef __LP64__
	socklen_t	size;			/* size of socket address */
#else
	size_t		size;			/* size of socket address */
#endif
#else
	unsigned int	size;			/* size of socket address */
#endif
	struct sockaddr_in
			sockinfo;		/* socket address info */

#ifdef  _WIN32
	__init_WSAEnvironment();
#endif
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return(-1);
	}
/*
 * Bind socket.
 */
	size = sizeof(sockinfo);
	memset((char *) &sockinfo, 0, size);

	sockinfo.sin_family = AF_INET;
	sockinfo.sin_addr.s_addr = htonl(INADDR_ANY);

	sockinfo.sin_port = htons(0);

	do {	
		err = bind(sock, (struct sockaddr *) &sockinfo, size);
		sched_yield();
	} while (err && errno == EADDRINUSE);

	if (err) {
		close((int)sock);
		return(-1);
	}
	if (pport) {
		if (getsockname(sock, (struct sockaddr *) &sockinfo, &size)) {
			close((int)sock);
			return(-1);
		}

		*pport = (int) ntohs(sockinfo.sin_port);
	}
/*
 * Listen.
 */
	if (listen(sock, SOMAXCONN)) {
		close((int)sock);
		return(-1);
	}

	return((int)sock);
}

static int
mkcltsock(unsigned char srcip[4], unsigned char dstip[4], int port)

{
	int		sock;			/* socket */
	int		ret;			/* returned value */
	int		reuse = 1;		/* sockopt reuse option */
#if defined(_XOPEN_SOURCE_EXTENDED)
#ifdef __LP64__
	socklen_t	size;			/* size of socket address */
#else
	size_t		size;			/* size of socket address */
#endif
#else
	int		size;			/* size of socket address */
#endif
	static int	retries = 0;		/* # times to retry connect */
	int		outer_failures = 0;	/* # failed connect loops */
	int		connect_failures = 0;	/* # failed connects */
	char		*p;			/* getenv results */
	struct sockaddr_in
			sockinfo;		/* socket address info */

	size = sizeof(sockinfo);

#ifdef  _WIN32
	__init_WSAEnvironment();
#endif

	if (!retries) {
		p = getenv("HPMP_CONNECT_RETRIES");
		if (p) retries = atoi(p);
		else retries = 5;
	}

	for (;;) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) return(-1);
/*
 * Bind local address if specified.
 */
		if (srcip) {
			memset((char *) &sockinfo, 0, size);
			memcpy((char *) &sockinfo.sin_addr,
				(char *) srcip, sizeof(srcip));
			sockinfo.sin_family = AF_INET;
/*
 * sockinfo.sin_port = htons((unsigned short) port);
 */
			sockinfo.sin_port = 0;
			if (bind(sock, (struct sockaddr *) &sockinfo, size)) {
				fprintf(stderr, "mkcltsock:bind() failed: %s",strerror(errno));
				close((int)sock);
				return(-1);
			}
		}
/*
 * Connect socket.
 */
		memset((char *) &sockinfo, 0, size);
		memcpy((char *) &sockinfo.sin_addr, (char *) dstip, 4);
		sockinfo.sin_family = AF_INET;
		sockinfo.sin_port = htons((unsigned short) port);

/*
 *  Note the outer loop uses "retries" from HPMP_CONNECT_RETRIES,
 *  and has a yield between each retry, since it's more oriented
 *  toward failures from network overload and putting a pause
 *  between retries.  This inner loop should only iterate when
 *  a signal interrupts the local process, so it doesn't pause
 *  or use the same HPMP_CONNECT_RETRIES count.
 */
		connect_failures = 0;
		ret = 1;
		while (ret != 0 && connect_failures <= 10) {
			ret = connect(sock, (struct sockaddr *) &sockinfo,
				size);
			if (ret == 0) break;
			fprintf(stderr, "mkcltsock:connect() failed: %s",strerror(errno));
			if (errno == EINTR) {
				++connect_failures;
			} else {
				break;
			}
		}

		if (ret == 0) break;
/* 
 * For large clusters, the connect/accept calls seem to fail occasionally,
 * no doubt do to the large number (1000's) of simultaneous connect packets
 * flooding the network at once.  So, we retry up to HPMP_CONNECT_RETRIES
 * number of times.
 */
		if (errno != EINTR) { 
			if (++outer_failures > retries) {
				close((int)sock);
				return(-1);		
			}
			struct timespec req, rem;
			req.tv_sec = 0;
			req.tv_nsec = 500000;
			nanosleep(&req, &rem);
		}
		close((int)sock);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(int))) {
		close((int)sock);
		return(-2);		
	}

	return((int)sock);
}
