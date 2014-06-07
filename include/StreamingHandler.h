/*
 *  tvdaemon
 *
 *  DVB StreamingHandler class
 *
 *  Copyright (C) 2012 André Roth
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _StreamingHandler_
#define _StreamingHandler_

#include "Thread.h"

#include <string>
#include <map>
#include <list>

#include <ccrtp/rtp.h> // FIXME: make interfase/impl

class Channel;
class Activity_Stream;
class Activity_Record;

class StreamingHandler : public Thread
{
  public:
    static StreamingHandler *Instance( );
    virtual ~StreamingHandler( );

    int GetFreeRTPPort( );

    bool SetupChannel( const std::string &channel_name, const std::string &client, int port, int &session_id, int &ssrc );
    bool SetupPlayback( int recording_id, const std::string &client, int port, int &session_id, int &ssrc );

    bool Play( int session_id );
    bool Stop( int session_id );

    void Shutdown( );

    bool KeepAlive( int session_id );

  private:
    StreamingHandler( );

    bool up;
    virtual void Run( );


    class RTPSession : public ost::RTPSession
    {
      public:
        RTPSession( std::string client, int port );
        virtual ~RTPSession( );

      private:
        void onGotGoodbye( const ost::SyncSource &source, const std::string &reason );
    };

    class Client
    {
      public:
        Client( Channel *channel, const std::string &client, int port );
        Client( Activity_Record *recording, const std::string &client, int port );
        ~Client( );

        bool Play( );
        bool Stop( );
        void KeepAlive( );
        int GetSSRC( );

        Channel *channel;
        Activity_Record *recording;

        std::string client;
        int         port;

        Activity_Stream *activity;
        RTPSession *session;

        time_t ts;
    };

    void RemoveClient( std::map<int, Client *>::iterator it );

    std::map<int, Client *> clients;

    std::list<int> rtpports;
};

#endif
