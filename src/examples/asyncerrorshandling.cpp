#include <iostream>
#include <signal.h>
#include <adapters/libeventadapter.h>

#include "asynchirediscommand.h"

#ifdef _WIN32
#define SIGPIPE 13
#endif

using namespace RedisCluster;
using namespace std;

// This error is demonstrating error handling
// Since error handling in way of try catch in setCallback function is not acceptable
// so you can set your error callback, which will be able to check the Exception
// type and command processing state and make a decision to write a log file and
// in some cases to retry send a command
// This examples shows how to do this

AsyncHiredisCommand::Action errorHandler(const ClusterException &exception,
                                         HiredisProcess::processState state )
{
    AsyncHiredisCommand::Action action = AsyncHiredisCommand::FINISH;
    
    // Check the exception type, you can check any type of exceptions
    // This examples shows simple retry behaviour in case of exceptions is not
    // from criticals exceptions group
    if( dynamic_cast<const CriticalException*>(&exception) == NULL )
    {
        // here can be a log writing function
        cerr << "Exception in processing async redis callback: " << exception.what() << endl;
        cerr << "Retrying" << endl;
        // retry to send a command to redis node
        action = AsyncHiredisCommand::RETRY;
    }
    else
    {
        // here can be a log writing function
        cerr << "Critical exception in processing async redis callback: " << exception.what();
    }
    return action;
}

typedef typename Cluster<redisAsyncContext>::ptr_t ClusterPtr;

static void setCallback( ClusterPtr cluster_p,
    const redisReply &reply, const string &demoStr )
{
    if( reply.type == REDIS_REPLY_STATUS  || reply.type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply.str << endl;
    }
    
    cout << demoStr << endl;
    cluster_p->disconnect();
}

void processAsyncCommand()
{
    ClusterPtr cluster_p;
    
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    string demoStr("Demo data is ok");

    LibeventAdapter adapter(*base);
    cluster_p = AsyncHiredisCommand::createCluster("127.0.0.1", 7000, adapter);
    
    AsyncHiredisCommand &cmd = AsyncHiredisCommand::Command( cluster_p,
                                 "FOO5",
                                 [cluster_p, demoStr](const redisReply &reply) {
                                    setCallback(cluster_p, reply, demoStr);
                                 },
                                 "SET %s %s",
                                 "FOO",
                                 "BAR1");
    
    // set error callback function
    cmd.setUserErrorCb( errorHandler );
    
    event_base_dispatch(base);
    delete cluster_p;
    event_base_free(base);
}

int main(int argc, const char * argv[])
{
    try
    {
        processAsyncCommand();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}

