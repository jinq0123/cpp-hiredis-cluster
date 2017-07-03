/*
 * Copyright (c) 2015, Dmitrii Shinkevich <shinmail at gmail dot com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __libredisCluster__asynchirediscommand__
#define __libredisCluster__asynchirediscommand__

#include <cassert>
#include <functional>  // for function<>
#include <memory>  // for shared_ptr<>

#include "adapters/adapter.h"  // for Adapter
#include "cluster.h"
#include "hiredisprocess.h"

extern "C"
{
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
}

namespace RedisCluster
{
    using std::string;
    
    // Asynchronous command class. Use Adapter to adapt different event library.
    // Copyable.
    class AsyncHiredisCommand
    {
    private:
        typedef Cluster<redisAsyncContext> Clstr;
        typedef redisAsyncContext Connection;
        typedef Clstr::ptr_t ClstrPtr;

        struct ConnectContext {
            Adapter *adapter;
            ClstrPtr pcluster;
        };

        AsyncHiredisCommand(const AsyncHiredisCommand&) = delete;
        AsyncHiredisCommand& operator=(const AsyncHiredisCommand&) = delete;

    public:
        // Utility to format string.
        static inline string formatf( const char *pformat, ... )
        {
            va_list ap;
            va_start( ap, pformat );
            string s = vformat( pformat, ap );
            va_end( ap );
            return s;
        }

        static inline string vformat( const string &formatStr, va_list ap )
        {
            char * buf = nullptr;
            int len = redisvFormatCommand( &buf, formatStr.c_str(), ap );
            string s( buf, len );
            redisFreeCommand( buf );
            return s;
        }

        static inline string formatArgv( int argc, const char ** argv,
                                    const size_t *argvlen )
        {
            sds buf = nullptr;
            int len = redisFormatSdsCommandArgv( &buf, argc, argv, argvlen );
            string s( static_cast<char*>(buf), len );
            redisFreeSdsCommand( buf );
            return s;
        }

    public:
        enum Action
        {
            REDIRECT,
            FINISH,
            ASK,
            RETRY
        };
        
        // hiredis will freeReplyObject(). RedisCallback must not delete the reply.
        typedef std::function<void (const redisReply* reply)> RedisCallback;
        // Retry cmd if UserErrorCb returns RETRY, otherwise finish cmd.
        typedef std::function<Action (const ClusterException &,
            HiredisProcess::processState)> UserErrorCb;  // UserErrorCallback

        // cmdStr must be formatted according to the Redis protocol,
        //  like: "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n".
        static inline void commandStr( Clstr &cluster, const string &key,
            const string &cmdStr,
            const RedisCallback& redisCallback = RedisCallback(),
            const UserErrorCb& userErrorCb = UserErrorCb() )
        {
            AsyncHiredisCommand* cmd(new AsyncHiredisCommand(cluster));
            cmd->setKey( key );
            cmd->setCmd( cmdStr );
            cmd->setRedisCallback( redisCallback );
            cmd->setUserErrorCb( userErrorCb );
            if (cmd->process() == REDIS_OK)
                return;

            delete cmd;
            throw DisconnectedException();
        }

        static inline void commandArgv( Clstr &cluster, const string &key,
            int argc, const char ** argv, const size_t *argvlen,
            const RedisCallback& redisCallback = RedisCallback(),
            const UserErrorCb& userErrorCb = UserErrorCb() )
        {
            string cmdStr = formatArgv( argc, argv, argvlen );
            commandStr( cluster, key, cmdStr, redisCallback, userErrorCb );
        }
        
        static inline void commandf0( Clstr &cluster, const string &key,
                                     const char *pformat, ... )
        {
            va_list ap;
            va_start( ap, pformat );
            string cmdStr = vformat( pformat, ap );
            va_end( ap );
            commandStr( cluster, key, cmdStr );
        }

        // Same as commandf1().
        static inline void commandf( Clstr &cluster, const string &key,
            const RedisCallback& redisCallback, const char *pformat, ... )
        {
            va_list ap;
            va_start( ap, pformat );
            string cmdStr = vformat( pformat, ap );
            va_end( ap );
            commandStr( cluster, key, cmdStr, redisCallback );
        }

        // Same as commandf().
        static inline void commandf1( Clstr &cluster, const string &key,
            const RedisCallback& redisCallback, const char *pformat, ... )
        {
            va_list ap;
            va_start( ap, pformat );
            string cmdStr = vformat( pformat, ap );
            va_end( ap );
            commandStr( cluster, key, cmdStr, redisCallback );
        }

        static inline void commandf2( Clstr &cluster, const string &key,
            const RedisCallback& redisCallback, const UserErrorCb& userErrorCb,
            const char *pformat, ... )
        {
            va_list ap;
            va_start( ap, pformat );
            string cmdStr = vformat( pformat, ap );
            va_end(ap);
            commandStr( cluster, key, cmdStr, redisCallback, userErrorCb );
        }

        static inline AsyncHiredisCommand& vcommand( Clstr &cluster,
            const string &key, const string &formatStr, va_list ap,
            const RedisCallback& redisCallback = RedisCallback(),
            const UserErrorCb& userErrorCb = UserErrorCb() )
        {
            string cmdStr = vformat( formatStr, ap );
            commandStr( cluster, key, cmdStr, redisCallback, userErrorCb);
        }

    public:
        // Todo: Allow hosts
        static ClstrPtr createCluster(
            const char* host,
            int port,
            Adapter& adapter,
            const struct timeval &timeout = { 3, 0 } )
        {
            redisContext *con = redisConnectWithTimeout(host, port, timeout);
            if( con == NULL || con->err )
                throw ConnectionFailedException(nullptr);
            
            redisReply * reply = static_cast<redisReply*>( redisCommand( con, Clstr::CmdInit() ) );
            HiredisProcess::checkCritical( reply, true );
            
            ClstrPtr cluster = new Clstr;
            ConnectContext cc{ &adapter, cluster };
            cluster->init( reply,
                [cc]( const string &host, int port ) {
                    return connect( host, port, cc );
                },
                disconnect );
            
            freeReplyObject( reply );
            redisFree( con );
            
            return cluster;
        }
        
    protected:
        explicit AsyncHiredisCommand( Clstr &cluster ) : cluster_( cluster )
        {
        }
        
        ~AsyncHiredisCommand()
        {
            assert(isSanity());
            assert(0 == (sanity_ = 0));
            if( redirectCon_ )
            {
                redisAsyncDisconnect( redirectCon_ );
            }
        }

    public:
        inline void setKey( const string &key)
        {
            key_ = key;
        }

        inline void setCmd( const string &cmd )
        {
            cmd_ = cmd;
        }

        inline void setRedisCallback( const RedisCallback &redisCallback )
        {
            redisCallback_ = redisCallback;
        }

        inline void setUserErrorCb( const UserErrorCb &userErrorCb )
        {
            userErrorCb_ = userErrorCb;
        }

    protected:
        static void disconnect(Connection *ac) {
            redisAsyncDisconnect( ac );
        }
        
        inline int process()
        {
            Clstr::SlotConnection slotCon = cluster_.getConnection( key_ );
            return processHiredisCommand( slotCon.second );
        }
        
        inline int processHiredisCommand( Connection* con )
        {
            assert(isSanity());
            return redisAsyncFormattedCommand( con, processCommandReply,
                this, cmd_.data(), cmd_.size() );
        }
        
        // Callback fun of redisAsyncCommand "ASKING".
        static void askingCallbackFn( Connection* con, void *r, void *data )
        {
            assert(data);
            redisReply *reply = static_cast<redisReply*>(r);
            auto* that = static_cast<AsyncHiredisCommand*>(data);
            Action commandState = ASK;

            try
            {
                HiredisProcess::checkCritical(reply, false, false);
                if( reply->type == REDIS_REPLY_STATUS && string(reply->str) == "OK" )
                {
                    if( that->processHiredisCommand( that->redirectCon_ ) != REDIS_OK )
                    {
                        throw AskingFailedException(nullptr);
                    }
                }
                else
                {
                    throw AskingFailedException(nullptr);
                }
            }
            catch ( const ClusterException &ce )
            {
                if ( that->runUserErrorCb( ce, HiredisProcess::ASK ) == RETRY )
                {
                    commandState = RETRY;
                }
                else
                {
                    commandState = FINISH;
                }
            }
            
            if( commandState == RETRY )
            {
                if (!that->retry( con, reply ))
                    delete that;  // retry failed
            }
            else if( commandState == FINISH )
            {
                that->runRedisCallback( reply );
                if (!(con->c.flags & (REDIS_SUBSCRIBED)))
                    delete that;
            }
        }

        // Callback fun of redisAsyncFormattedCommand()
        static void processCommandReply( Connection* con, void *r, void *data )
        {
            assert( data );
            redisReply *reply = static_cast< redisReply* >(r);
            auto* that = static_cast<AsyncHiredisCommand*>(data);
            Action commandState = FINISH;
            HiredisProcess::processState state = HiredisProcess::FAILED;
            string host, port;
            
            try {
                HiredisProcess::checkCritical( reply, false, false );
                state = HiredisProcess::processResult( reply, host, port);
                switch (state) {
                    case HiredisProcess::ASK:
                        if ( REDIS_OK == that->goAsking( host, port ) )
                            commandState = ASK;
                        else
                            throw AskingFailedException(nullptr);
                        break;
                    case HiredisProcess::MOVED:
                        that->cluster_.moved();
                        that->redirectConnect( host, port );
                        if( that->processHiredisCommand( that->redirectCon_ ) == REDIS_OK )
                            commandState = REDIRECT;
                        else
                            throw MovedFailedException(nullptr);
                        break;
                    case HiredisProcess::READY:
                        break;
                    case HiredisProcess::CLUSTERDOWN:
                        throw ClusterDownException(nullptr);

                    default:
                        throw LogicError(nullptr);
                }
            }
            catch ( const ClusterException &ce )
            {
                if ( that->runUserErrorCb( ce, state ) == RETRY )
                {
                    commandState = RETRY;
                }
            }
            
            if( commandState == RETRY )
            {
                if (!that->retry( con, reply ))
                    delete that;  // retry failed
            }
            else if( commandState == FINISH )
            {
                that->runRedisCallback( reply );
                if (!(con->c.flags & (REDIS_SUBSCRIBED)))
                    delete that;
            }
        }
        
        static void disconnectCb(const struct redisAsyncContext*ctx, int /*status*/)
        {
            ClstrPtr clusterPtr = static_cast<ClstrPtr>(ctx->data);
            assert(clusterPtr);
            clusterPtr->deleteConnection(ctx);
        }
        
        static Connection* connect( const string &host, int port,
            const ConnectContext &context )
        {
            if ( context.adapter == NULL )
                throw ConnectionFailedException(nullptr);

            Connection *con = redisAsyncConnect( host.c_str(), port );
            if( con == NULL || con->err != 0 ||
                context.adapter->attachContext( *con ) != REDIS_OK )
                throw ConnectionFailedException(nullptr);

            assert(context.pcluster);
            con->data = static_cast<void*>(context.pcluster);
            redisAsyncSetDisconnectCallback(con, disconnectCb);
            return con;
        }

    private:
        void runRedisCallback( const redisReply* reply ) const
        {
            assert(isSanity());
            if (redisCallback_)
                redisCallback_( reply );
        }

        Action runUserErrorCb( const ClusterException &clusterException,
            HiredisProcess::processState state ) const
        {
            assert(isSanity());
            if (!userErrorCb_) return FINISH;
            return userErrorCb_( clusterException, state );
        }

        int goAsking(const string &host, const string &port)
        {
            assert(isSanity());
            redirectConnect( host, port );
            return redisAsyncCommand( redirectCon_, askingCallbackFn,
                this, "ASKING" );
        }

        void redirectConnect( const string &host, const string &port )
        {
            if ( redirectCon_ ) return;
            redirectCon_ = cluster_.createNewConnection(host, port).second;
        }

        bool retry(  Connection* con, const redisReply* reply )
        {
            assert(isSanity());
            if (processHiredisCommand(con) == REDIS_OK)
                return true;

            runUserErrorCb( DisconnectedException(), HiredisProcess::FAILED );
            runRedisCallback( reply );
            return false;
        }

    private:
        // cluster object ( not thread-safe )
        Clstr &cluster_;
        
        // user-defined callback to redis async command
        RedisCallback redisCallback_;
        // user error handler
        UserErrorCb userErrorCb_;
        
        // pointer to async context ( in case of redirection class creates new connection )
        Connection *redirectCon_ = nullptr;

        // key of redis command to find proper cluster node
        string key_;
        string cmd_;

#ifndef NDEBUG
        bool isSanity() const { return 0xa5a5a5a5 == sanity_; }
        int sanity_ = 0xa5a5a5a5;  // for sanity check
#endif
    };
}

#endif /* defined(__libredisCluster__asynchirediscommand__) */
