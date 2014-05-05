/*
 *  tvdaemon
 *
 *  CAMClientHandler class
 *
 *  Copyright (C) 2014 André Roth
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

#include "CAMClientHandler.h"

#include "Log.h"
#include "TVDaemon.h"
#include "CAMClient.h"

CAMClientHandler *CAMClientHandler::Instance( )
{
  static CAMClientHandler instance;
  return &instance;
}

CAMClientHandler::CAMClientHandler( ) :
  ConfigObject( )
{
  std::string d = TVDaemon::Instance( )->GetConfigDir( );
  d += "camclients";
  SetConfigFile( d );
}

CAMClientHandler::~CAMClientHandler( )
{
  //SaveConfig( );
}

void CAMClientHandler::Shutdown( )
{
}

bool CAMClientHandler::SaveConfig( )
{
  DeleteConfig( "CAMClients" );
  Setting &n = ConfigList( "CAMClients" );
  ConfigBase c( n );
  for( std::vector<CAMClient *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
  {
    Setting &n2 = c.ConfigGroup( );
    ConfigBase c2( n2 );
    (*it)->SaveConfig( c2 );
  }
  WriteConfigFile( );
  return true;
}

bool CAMClientHandler::LoadConfig( )
{
  if( !ReadConfigFile( ))
  {
    return false;
  }

  Setting &n = ConfigList( "CAMClients" );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    ConfigBase c( n[i] );
    CAMClient *client = new CAMClient( );
    client->LoadConfig( c );
    clients.push_back( client );
    client->Connect();
  }
  return true;
}

CAMClient *CAMClientHandler::GetCAMClient( uint16_t caid )
{
  for( std::vector<CAMClient *>::iterator it = clients.begin( ); it != clients.end( ); it++ )
  {
    if( (*it)->HasCAID( caid ))
      return *it;
  }
  return NULL;
}


