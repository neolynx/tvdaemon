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

StreamingHandler *StreamingHandler::instance = NULL;

StreamingHandler *StreamingHandler::Instance( )
{
  if( !instance )
    instance = new StreamingHandler( );
  return instance;
}

StreamingHandler::StreamingHandler( )
{
}

StreamingHandler::~StreamingHandler( )
{
  for( std::map<int, Client *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
    delete it->second;
}

int StreamingHandler::Setup( Channel *channel, std::string server, std::string client, int port )
{
  int session_id;
  // FIXME Lock
  std::map<int, Client *>::iterator it;
  do
  {
    session_id = rand( );
    it = clients.find( session_id );
  } while( it != clients.end( ));

  clients[session_id] = new Client( channel, server, client, port );

  Log( "StreamingHandler::Setup %s -> %s:%d session %04x", server.c_str( ), client.c_str( ), port, session_id );

  return session_id;
}


bool StreamingHandler::Play( int session_id )
{
  Log( "StreamingHandler::Play session %04x", session_id );

  std::map<int, Client *>::iterator it = clients.find( session_id );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Play session not found %04x", session_id );
    return false;
  }

  return it->second->Play( );
}

bool StreamingHandler::Stop( int session_id )
{
  Log( "StreamingHandler::Stop session %04x", session_id );

  std::map<int, Client *>::iterator it = clients.find( session_id );
  if( it == clients.end( ))
  {
    LogError( "StreamingHandler::Stop session not found %04x", session_id );
    return false;
  }

  it->second->Stop( );
  std::list<int>::iterator it2 = std::find( rtpports.begin( ), rtpports.end( ), it->second->port );
  if( it2 != rtpports.end( ))
    rtpports.erase( it2 );
  delete it->second;
  clients.erase( it );
  return true;
}

int StreamingHandler::GetFreeRTPPort( )
{
  // FIXME lock
  int port = 7766;
  while( std::find( rtpports.begin( ), rtpports.end( ), port ) != rtpports.end( ))
    port += 2;
  rtpports.push_back( port );
  return port;
}

StreamingHandler::Client::Client( Channel *channel, std::string server, std::string client, int port ) :
  channel(channel),
  server(server),
  client(client),
  port(port),
  activity(NULL),
  socket(NULL)
{
}

StreamingHandler::Client::~Client( )
{
  if( activity )
    delete activity;
}

StreamingHandler::RTPSession::RTPSession( std::string server, std::string client, int port ) :
  ost::RTPSession( ost::InetHostAddress( server.c_str( )), port )
{
  //ost::InetHostAddress local_ip = server.c_str( );
  //if( !local_ip )
  //{
    //LogError( "RTP: invalid local ip '%s'", server.c_str( ));
    //return false;
  //}

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

  socket = new RTPSession( server, client, port );

  activity = new Activity_Stream( *channel, socket );

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

  //delete socket;
  //socket = NULL;

  return true;
}

