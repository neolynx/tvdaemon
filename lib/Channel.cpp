/*
 *  tvdaemon
 *
 *  Channel class
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

#include "Channel.h"

#include <algorithm> // find

#include "TVDaemon.h"
#include "Source.h"
#include "Transponder.h"
#include "Service.h"
#include "Log.h"

Channel::Channel( TVDaemon &tvd, std::string name, int config_id ) :
  ConfigObject( tvd, "channel", config_id ),
  tvd(tvd),
  name(name)
{
}

Channel::Channel( TVDaemon &tvd, std::string configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd)
{
}

Channel::~Channel( )
{
}

bool Channel::SaveConfig( )
{
  Lookup( "Name", Setting::TypeString ) = name;

  SaveReferences<Service, Channel>( *this, "Services", services );
  return WriteConfig( );
}

bool Channel::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;
  name = (const char *) Lookup( "Name", Setting::TypeString );

  Log( "Loading Channel '%s'", name.c_str( ));

  Setting &n = Lookup( "Services", Setting::TypeList );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    Setting &n2 = n[i];
    if( n2.getLength( ) != 3 )
    {
      LogError( "Error in service path: should be [source, transponder, service] in %s", GetConfigFile( ).c_str( ));
      continue;
    }
    Source      *s = tvd.GetSource( n2[0] );
    if( !s )
    {
      LogError( "Error in service path: source %d not found in %s", (int) n2[0], GetConfigFile( ).c_str( ));
      continue;
    }
    Transponder *t = s->GetTransponder( n2[1] );
    if( !t )
    {
      LogError( "Error in service path: transponder %d not found in %s", (int) n2[1], GetConfigFile( ).c_str( ));
      continue;
    }
    Service     *v = t->GetService((int) n2[2] );
    if( !v )
    {
      LogError( "Error in service path: service %d not found in %s", (int) n2[2], GetConfigFile( ).c_str( ));
      continue;
    }
    // FIXME: verify frontend type
    Log( "Adding configured service [%d, %d, %d] to channel %s", (int) n2[0], (int) n2[1], (int) n2[2], name.c_str( ));
    services.push_back( v );
  }
  return true;
}

bool Channel::AddService( Service *service )
{
  if( !service )
    return false;
  if( std::find( services.begin( ), services.end( ), service ) != services.end( ))
  {
    LogWarn( "Service already added to Channel %s", name.c_str( ));
    return false;
  }
  Log( "Adding Service %d to Channel %s", service->GetKey( ), name.c_str( ));
  services.push_back( service );
  return true;
}

bool Channel::Tune( )
{
  for( std::vector<Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
  {
    if( (*it)->Tune( ))
      return true;
  }
  return false;
}
