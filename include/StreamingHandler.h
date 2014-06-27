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

    bool Init( const std::string &session, in_addr client, const std::string &channel_name, double &duration );
    bool Init( const std::string &session, const in_addr &client, int recording_id, double &duration  );

    bool SetupChannel ( const std::string &session, const std::string &channel_name, int port, int &ssrc );
    bool SetupPlayback( const std::string &session, int recording_id, int port, int &ssrc );

    bool Play( const std::string &session, double &from, double &to, int &seq, int &rtptime );
    bool Stop( const std::string &session );

    void Shutdown( );

    bool KeepAlive( const std::string &session );

  private:
    StreamingHandler( );

    bool up;
    virtual void Run( );


    class RTPSession : public ost::RTPSession
    {
      public:
        RTPSession( );
        virtual ~RTPSession( );

        bool Connect( const in_addr &client, uint16_t port );
        int GetSequence( ) const;
        int GetInitialTimestamp( );

      private:
        void onGotGoodbye( const ost::SyncSource &source, const std::string &reason );
    };

    class Client
    {
      public:
        Client( Channel *channel,           const in_addr &client );
        Client( Activity_Record *recording, const in_addr &client );
        ~Client( );

        bool Init( );
        bool Connect( uint16_t port );
        bool Play( double &from, double &to, int &seq, int &rtptime );
        bool Stop( );
        void KeepAlive( );

        int GetSSRC( );
        double GetDuration( ) const;

        Channel *channel;
        Activity_Record *recording;

        in_addr client;
        uint16_t    port;

        Activity_Stream *activity;
        RTPSession session;

        time_t ts;
        double duration;
    };

    void RemoveClient( std::map<std::string, Client *>::iterator it );

    std::map<std::string, Client *> clients;

    std::list<int> rtpports;
};

#endif
