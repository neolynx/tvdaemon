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

#ifdef	CCXX_NAMESPACES
using namespace ost;
//using namespace std;
#endif

StreamingHandler::StreamingHandler( Channel *channel ) : channel(channel), activity(NULL)
{
  Log( "StreamingHandler created" );
}

StreamingHandler::~StreamingHandler( )
{
  delete activity;
}

void StreamingHandler::Setup( std::string server, std::string client, int port )
{
  this->server = server;
  this->client = client;
  this->port = port;

  Log( "StreamingHandler::Setup %s -> %s:%d", server.c_str( ), client.c_str( ), port );
}


bool StreamingHandler::Play( std::string url )
{
  Log( "StreamingHandler::Play %s", url.c_str( ));

  InetHostAddress local_ip = server.c_str( );
  if( !local_ip )
  {
    LogError( "RTP: invalid local ip '%s'", server.c_str( ));
    return false;
  }

  socket = new RTPSession( local_ip, 7766 );

  InetHostAddress remote_ip = client.c_str( );
  if( !local_ip )
  {
    LogError( "RTP: invalid remote ip '%s'", client.c_str( ));
    return false;
  }

  socket->setSchedulingTimeout( 10000 );
  if( !socket->addDestination( remote_ip, port ) )
  {
    LogError( "RTP: connection failed" );
    return false;
  }

  socket->setPayloadFormat( StaticPayloadFormat( sptMPV ));

  socket->startRunning();
  if( !socket->isActive() )
  {
    LogError( "RTP: unable to start" );
    return false;
  }

  if( !activity )
    activity = new Activity_Stream( *channel );

  activity->AddStream( socket );

}
