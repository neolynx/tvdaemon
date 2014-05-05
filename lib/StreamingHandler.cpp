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

void StreamingHandler::Setup( Channel *channel, std::string client, int port, int &session_id, int &ssrc )
{
  Lock( );
  std::map<int, Client *>::iterator it;
  do
  {
    session_id = rand( );
    it = clients.find( session_id );
  } while( it != clients.end( ));

  Client *c = new Client( channel, client, port );
  clients[session_id] = c;

  Log( "StreamingHandler::Setup %s:%d session %04x", client.c_str( ), port, session_id );

  ssrc = c->GetSSRC( );

  Unlock( );
}


bool StreamingHandler::Play( int session_id )
{
  Lock( );
  Log( "StreamingHandler::Play session %04x", session_id );

  std::map<int, Client *>::iterator it = clients.find( session_id );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Play session not found %04x", session_id );
    Unlock( );
    return false;
  }

  bool ret = it->second->Play( );
  Unlock( );
  return ret;
}

bool StreamingHandler::Stop( int session_id )
{
  Lock( );
  Log( "StreamingHandler::Stop session %04x", session_id );

  std::map<int, Client *>::iterator it = clients.find( session_id );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Stop session not found %04x", session_id );
    Unlock( );
    return false;
  }

  RemoveClient( it );

  Unlock( );
  return true;
}

bool StreamingHandler::KeepAlive( int session_id )
{
  Lock( );
  Log( "StreamingHandler::KeepAlive session %04x", session_id );
  std::map<int, Client *>::iterator it = clients.find( session_id );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::KeepAlive session not found %04x", session_id );
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
  for( std::map<int, Client *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
    delete it->second;
  clients.clear( );
  Unlock( );
}

void StreamingHandler::RemoveClient( std::map<int, Client *>::iterator it )
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
    Lock( );
    time_t now = time( NULL );
    for( std::map<int, Client *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
      if( difftime( now, it->second->ts ) > 60.0 )
      {
        LogWarn( "StreamingHandler RTSP session %d timeouted", it->first );
        RemoveClient( it );
      }
    Unlock( );
    sleep( 1 );
  }
}






StreamingHandler::Client::Client( Channel *channel, std::string client, int port ) :
  channel(channel),
  client(client),
  port(port),
  activity(NULL),
  session(NULL)
{
  ts = time( NULL );
  session = new RTPSession( client, port );

}

StreamingHandler::Client::~Client( )
{
  if( activity )
    delete activity;
}


bool StreamingHandler::Client::Play( )
{
  if( !channel )
  {
    LogError( "StreamingHandler::Client::Play no channel defined" );
    return false;
  }

  if( activity )
  {
    LogError( "StreamingHandler::Client::Play activita already started" );
    return false;
  }

  activity = new Activity_Stream( *channel, session );

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
  return session->getLocalSSRC( );
}

StreamingHandler::RTPSession::RTPSession( std::string client, int port ) :
  ost::RTPSession( ost::InetHostAddress( "0.0.0.0" ))
{
  //ost::InetHostAddress local_ip = server.c_str( );
  //if( !local_ip )
  //{
    //LogError( "RTP: invalid local ip '%s'", server.c_str( ));
    //return false;
  //}


  //ost::InetHostAddress local_ip = ost::RTPSession::getDataSender(  );
  //server = local_ip;
  //LogWarn( "local port %s", server.c_str( ));

  ost::InetHostAddress remote_ip = client.c_str( );
  if( !remote_ip )
  {
    LogError( "RTP: invalid remote ip '%s'", client.c_str( ));
    //return false;
  }

  setSchedulingTimeout( 10000 );
  if( !addDestination( remote_ip, port ) )
  {
    LogError( "RTP: connection failed" );
    //return false;
  }

  setPayloadFormat( ost::StaticPayloadFormat( ost::sptMP2T ));

  startRunning();
  if( !isActive() )
  {
    LogError( "RTP: unable to start" );
    //return false;
  }

}

StreamingHandler::RTPSession::~RTPSession( )
{
}

void StreamingHandler::RTPSession::onGotGoodbye( const ost::SyncSource &source, const std::string &reason )
{
  LogError( "RTP: got goodbye" );
}
