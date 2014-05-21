// deathWatch: does process startup, get commands from controller process
// to register interest in death of processes, waits for death notices,
// reports status, and exits upon command.

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "clio.h"
#include "sqevlog/evl_sqlog_writer.h"
#include "montestutil.h"
#include "deathNotice.h"

MonTestUtil util;

long trace_settings = 0;
FILE *shell_locio_trace_file = NULL;
bool tracing = false;

const char *MyName;
int gv_ms_su_nid = -1;          // Local IO nid to make compatible w/ Seabed
char ga_ms_su_c_port[MPI_MAX_PORT_NAME] = {0};

char victimName[25];
bool gotDeathNotice = false;

// Routine for handling notices:
//   NodeDown, NodeUp, ProcessDeath, Shutdown, TmSyncAbort, TmSyncCommit
void recv_notice_msg(struct message_def *recv_msg, int )
{
    if ( recv_msg->type == MsgType_ProcessDeath )
    {
        if ( tracing )
            printf("[%s] Process death notice received for %s (%d, %d),"
                   " trans_id=%lld.%lld.%lld.%lld., aborted=%d\n", 
                   MyName,
                   recv_msg->u.request.u.death.process_name,
                   recv_msg->u.request.u.death.nid,
                   recv_msg->u.request.u.death.pid,
                   recv_msg->u.request.u.death.trans_id.txid[0],
                   recv_msg->u.request.u.death.trans_id.txid[1],
                   recv_msg->u.request.u.death.trans_id.txid[2],
                   recv_msg->u.request.u.death.trans_id.txid[3],
                   recv_msg->u.request.u.death.aborted);

        if (strcmp(recv_msg->u.request.u.death.process_name, victimName) == 0)
        {   // Got expected death notice
            gotDeathNotice = true;
        }
    }
    else
    {
        printf("[%s] unexpected notice, type=%s\n", MyName,
               MessageTypeString( recv_msg->type));
    }
}


void processCommands()
{
    MPI_Comm worker_comm;
    int servNid;
    int servPid;
    int rc;
    MPI_Status status;
    int recvbuf[6];
    char sendbuf[100];
    bool done = false;
    const int serverTag = 100;

    if ( tracing )
    {
        printf ("[%s] Port: %s\n", MyName, util.getPort());
    }

    if ( tracing )
    {
        printf ("[%s] Wait to connect.\n", MyName);
    }

    MPI_Comm_accept (util.getPort(), MPI_INFO_NULL, 0, MPI_COMM_SELF,
                     &worker_comm);
    MPI_Comm_set_errhandler (worker_comm, MPI_ERRORS_RETURN);

    if ( tracing )
    {
        printf ("[%s] Connected.\n", MyName);
    }

    do
    {
        rc = MPI_Recv (recvbuf, 6, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG,
                       worker_comm, &status);

        if (rc == MPI_SUCCESS)
        {
            switch (recvbuf[0])
            {
            case CMD_REG_INTEREST:
                strcpy(victimName, (char *)&recvbuf[1]);
                if ( tracing )
                {
                    printf("[%s] got command CMD_REG_INTEREST for %s.\n",MyName,
                           victimName);
                }

                if (util.requestProcInfo ( victimName, servNid, servPid) )
                {
                    _TM_Txid_External transid = {{0LL, 0LL, 0LL, 0LL}};
                    
                    util.requestNotice(servNid, servPid, false, transid);
                }
                strcpy(sendbuf, "OK");
                break;

            case CMD_GET_STATUS:
                if ( tracing )
                {
                    printf("[%s] got command CMD_GET_STATUS.\n",MyName);
                }
                if (gotDeathNotice)
                    strcpy(sendbuf, "OK");
                else
                    strcpy(sendbuf, "FAILED");
               break;

            case CMD_END:
                if ( tracing )
                {
                    printf("[%s] got command CMD_END.\n",MyName);
                }
                strcpy(sendbuf, "OK");
                done = true;
                break;
            default:
               sprintf (sendbuf, "[%s] Received (%d:%d) UNKNOWN", MyName,
                        recvbuf[0], recvbuf[1]);
            }
            rc = MPI_Send (sendbuf, (int) strlen (sendbuf) + 1, MPI_CHAR, 0,
                           serverTag, worker_comm);
        }
        else
        {  // Receive failed
            printf("[%s] MPI_Recv failed, rc = (%d) %s\n",
                   MyName, rc, util.MPIErrMsg(rc));
            done = true;
        }

    }
    while (!done);

    if ( tracing )
    {
        printf ("[%s] disconnecting.\n", MyName);
    }
    util.closeProcess ( worker_comm );
}

int main (int argc, char *argv[])
{
    int MyRank = -1;
    victimName[0] = '\0';

    // Setup HP_MPI software license
    int key = 413675219; //413675218 to display banner
    MPI_Initialized(&key);

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &MyRank);

    util.processArgs (argc, argv);
    tracing = util.getTrace();
    MyName = util.getProcName();

    util.InitLocalIO( );
    assert (gp_local_mon_io);

    // Set local io callback function for "notices"
    gp_local_mon_io->set_cb(recv_notice_msg, "notice");

    util.requestStartup ();

    // Get and execute commands from controller process
    processCommands();

    // tell monitor we are exiting
    util.requestExit ( );

    fflush (stdout);
    MPI_Close_port( util.getPort() );
    MPI_Finalize ();
    if ( gp_local_mon_io )
    {
        delete gp_local_mon_io;
    }
    exit (0);
}
