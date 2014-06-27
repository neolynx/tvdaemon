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

#include "StreamingHandler.h"

#include "Channel.h"
#include "Log.h"
#include "Activity_Stream.h"
#include "Activity_Record.h"
#include "TVDaemon.h"
#include "Recorder.h"

StreamingHandler *StreamingHandler::Instance( )
{
  static StreamingHandler instance;
  return &instance;
}

StreamingHandler::StreamingHandler( ) : Thread( ), up(true)
{
  StartThread( );
}

StreamingHandler::~StreamingHandler( )
{
  Shutdown( );
}

bool StreamingHandler::Init( const std::string &session, in_addr client, const std::string &channel_name, double &duration )
{
  Channel *channel = TVDaemon::Instance( )->GetChannel( channel_name );
  if( !channel )
  {
    LogError( "RTSP: unknown channel: '%s'", channel_name.c_str( ));
    return false;
  }
  Client *c = new Client( channel, client );
  c->Init( ); // FIXME: check outcome
  duration = c->GetDuration( );
  Lock( );
  clients[session] = c;
  Unlock( );
}

bool StreamingHandler::Init( const std::string &session, const in_addr &client, int recording_id, double &duration )
{
  Activity_Record *recording = Recorder::Instance( )->GetRecording( recording_id );
  if( !recording )
  {
    LogError( "RTSP: unknown recording: %d", recording_id );
    return false;
  }
  Client *c = new Client( recording, client );
  c->Init( ); // FIXME: check outcome
  duration = c->GetDuration( );
  Lock( );
  clients[session] = c;
  Unlock( );
}

bool StreamingHandler::SetupChannel( const std::string &session, const std::string &channel_name, int port, int &ssrc )
{
  Lock( );
  std::map<std::string, Client *>::iterator it = clients.find( session );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Setup session not found '%s'", session.c_str( ));
    Unlock( );
    return false;
  }
  // FIXME: verify channel_name matches
  it->second->Connect( port );
  ssrc = it->second->GetSSRC( );
  Unlock( );

  Log( "StreamingHandler::Setup session %s, rtp port %d, ssrc %04x", session.c_str( ), port, ssrc );
  return true;
}


bool StreamingHandler::SetupPlayback( const std::string &session, int recording_id, int port, int &ssrc )
{
  Lock( );
  std::map<std::string, Client *>::iterator it = clients.find( session );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Setup session not found '%s'", session.c_str( ) );
    Unlock( );
    return false;
  }
  // FIXME: verify recording_id matches
  it->second->Connect( port );
  ssrc = it->second->GetSSRC( );
  Unlock( );

  Log( "StreamingHandler::Setup session %s, rtp port %d, ssrc %04x", session.c_str( ), port, ssrc );
  return true;
}

bool StreamingHandler::Play( const std::string &session, double &from, double &to, int &seq, int &rtptime )
{
  Lock( );
  std::map<std::string, Client *>::iterator it = clients.find( session );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Play session not found %s", session.c_str( ));
    Unlock( );
    return false;
  }

  Log( "StreamingHandler::Play session %s", session.c_str( ));
  bool ret = it->second->Play( from, to, seq, rtptime );
  Unlock( );
  return ret;
}

bool StreamingHandler::Stop( const std::string &session )
{
  Lock( );
  Log( "StreamingHandler::Stop session %s", session.c_str( ));

  std::map<std::string, Client *>::iterator it = clients.find( session );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Stop session not found %s", session.c_str( ));
    Unlock( );
    return false;
  }

  RemoveClient( it );

  Unlock( );
  return true;
}

bool StreamingHandler::KeepAlive( const std::string &session )
{
  Lock( );
  Log( "StreamingHandler::KeepAlive session %s", session.c_str( ));
  std::map<std::string, Client *>::iterator it = clients.find( session );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::KeepAlive session not found %s", session.c_str( ));
    Unlock( );
    return false;
  }

  it->second->KeepAlive( );
  Unlock( );
  return true;
}

int StreamingHandler::GetFreeRTPPort( )
{
  Lock( );
  int port = 7766;
  while( std::find( rtpports.begin( ), rtpports.end( ), port ) != rtpports.end( ))
    port += 2;
  rtpports.push_back( port );
  Unlock( );
  return port;
}

void StreamingHandler::Shutdown( )
{
  up = false;
  JoinThread( );
  Lock( );
  for( std::map<std::string, Client *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
    delete it->second;
  clients.clear( );
  Unlock( );
}

void StreamingHandler::RemoveClient( std::map<std::string, Client *>::iterator it )
{
  it->second->Stop( );
  std::list<int>::iterator it2 = std::find( rtpports.begin( ), rtpports.end( ), it->second->port );
  if( it2 != rtpports.end( ))
    rtpports.erase( it2 );
  delete it->second;
  clients.erase( it );
}

void StreamingHandler::Run( )
{
  while( up )
  {
    Lock( ); // FIXME: this can deadlock ! use timed lock...
    time_t now = time( NULL );
    for( std::map<std::string, Client *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
      if( difftime( now, it->second->ts ) > 60.0 )
      {
        LogWarn( "StreamingHandler RTSP session %s timeouted", it->first.c_str( ));
        RemoveClient( it );
      }
    Unlock( );
    sleep( 1 );
  }
}






StreamingHandler::Client::Client( Channel *channel, const in_addr &client ) :
  channel(channel),
  recording(NULL),
  client(client),
  port(0),
  activity(NULL),
  duration(NAN)
{
  ts = time( NULL );
}

StreamingHandler::Client::Client( Activity_Record *recording, const in_addr &client ) :
  channel(NULL),
  recording(recording),
  client(client),
  port(0),
  activity(NULL),
  duration(NAN)
{
  ts = time( NULL );
}

StreamingHandler::Client::~Client( )
{
  if( activity )
    delete activity;
}

bool StreamingHandler::Client::Init( )
{
  if( !channel and !recording )
  {
    LogError( "StreamingHandler::Client::Init no channel or recording defined" );
    return false;
  }
  if( channel )
    activity = new Activity_Stream( channel, session );
  else
    activity = new Activity_Stream( recording, session );

  this->duration = activity->GetDuration( );

  Log( "StreamingHandler::Client::Init duration %fs", duration );

  return true;
}

bool StreamingHandler::Client::Connect( uint16_t port )
{
  return session.Connect( client, port );
}

bool StreamingHandler::Client::Play( double &from, double &to, int &seq, int &rtptime )
{
  if( !channel and !recording )
  {
    LogError( "StreamingHandler::Client::Play no channel or recording defined" );
    return false;
  }

  if( !activity )
  {
    LogError( "StreamingHandler::Client not initialized" );
    return false;
  }

  from = 0.0;
  to = duration;

  seq = session.GetSequence( );
  rtptime = session.GetInitialTimestamp( );

  Log( "StreamingHandler::Client::Play rtptime %u", rtptime );

  activity->Start( );

  return true;
}

bool StreamingHandler::Client::Stop( )
{
  if( !activity )
  {
    LogError( "StreamingHandler::Client::Stop activity not started" );
    return false;
  }

  delete activity;
  activity = NULL;

  //delete session; -> segfault
  //session = NULL;

  return true;
}

void StreamingHandler::Client::KeepAlive( )
{
  ts = time( NULL );
}


int StreamingHandler::Client::GetSSRC( )
{
  return session.getLocalSSRC( );
}

double StreamingHandler::Client::GetDuration( ) const
{
  return duration;
}

StreamingHandler::RTPSession::RTPSession( ) :
  ost::RTPSession( ost::InetHostAddress( "0.0.0.0" ))
{
}

int StreamingHandler::RTPSession::GetSequence( ) const
{
  //return sendInfo.sendSeq;
  return 0;
}

int StreamingHandler::RTPSession::GetInitialTimestamp( )
{
  return getInitialTimestamp( );
}

bool StreamingHandler::RTPSession::Connect( const in_addr &client, uint16_t port )
{
  ost::InetHostAddress remote_ip = client;
  if( !remote_ip )
  {
    LogError( "RTP: invalid remote ip" ); // FIXME: log IP
    return false;
  }

  //setSchedulingTimeout( 20000 );
  setSchedulingTimeout( 200000 );
  setExpireTimeout( 3000000 );
  if( !addDestination( remote_ip, port ) )
  {
    LogError( "RTP: connection failed" );
    return false;
  }

  setPayloadFormat( ost::StaticPayloadFormat( ost::sptMP2T ));

  startRunning();
  if( !isActive() )
  {
    LogError( "RTP: unable to start" );
    return false;
  }

  return true;
}

StreamingHandler::RTPSession::~RTPSession( )
{
}

void StreamingHandler::RTPSession::onGotGoodbye( const ost::SyncSource &source, const std::string &reason )
{
  LogError( "RTP: got goodbye" );
}
