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
#include <json/json.h>

#include "RPCObject.h"
#include "TVDaemon.h"
#include "Service.h"
#include "Transponder.h"
#include "Source.h"
#include "Activity_UpdateEPG.h"
#include "Log.h"

Channel::Channel( TVDaemon &tvd, Service *service, int config_id ) :
  ConfigObject( tvd, "channel", config_id ),
  tvd(tvd),
  number(config_id)
{
  name = service->GetName( );
  services.push_back( service );
  epg = false;
}

Channel::Channel( TVDaemon &tvd, std::string configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd)
{
  epg = false;
}

Channel::~Channel( )
{
  ClearEPG( );
}

bool Channel::SaveConfig( )
{
  WriteConfig( "Name", name );
  WriteConfig( "Number", number );

  DeleteConfig( "EPG" );
  Setting &n = ConfigList( "EPG" );
  ConfigBase c( n );
  for( int i = 0; i < events.size( ); i++ )
  {
    Setting &n2 = c.ConfigGroup( );
    ConfigBase c2( n2 );
    events[i]->SaveConfig( c2 );
  }

  return WriteConfigFile( );
}

bool Channel::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;
  ReadConfig( "Name", name );
  ReadConfig( "Number", number );

  Setting &n = ConfigList( "EPG" );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    ConfigBase c2( n[i] );
    Event *e = new Event( *this );
    e->LoadConfig( c2 );
    events.push_back (e );
  }
  return true;
}

bool Channel::AddService( Service *service )
{
  if( !service )
    return false;
  if( HasService( service ))
  {
    LogWarn( "Service already added to Channel %s", name.c_str( ));
    return false;
  }
  services.push_back( service );
  return true;
}

bool Channel::HasService( Service *service ) const
{
  if( std::find( services.begin( ), services.end( ), service ) != services.end( ))
  {
    return true;
  }
  return false;
}

void Channel::json( json_object *entry ) const
{
  json_object_object_add( entry, "name",   json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "number", json_object_new_int( number ));
  json_object_object_add( entry, "id",     json_object_new_int( GetKey( )));
}

bool Channel::RPC( const HTTPRequest &request,  const std::string &cat, const std::string &action )
{
  if( action == "schedule" )
  {
    int event_id;
    request.GetParam( "event_id", event_id );
    for( int i = 0; i < events.size( ); i++ )
    {
      if( events[i]->GetID( ) == event_id )
      {
        tvd.Schedule( *events[i] );
        request.Reply( HTTP_OK );
        return true;
      }
    }
    request.NotFound( "Event %d not found", event_id );
    return false;
  }

  request.NotFound( "RPC: unknown action: '%s'", action.c_str( ));
  return false;
}

bool Channel::Tune( Activity &act )
{
  for( std::vector<Service *>::const_iterator it = services.begin( ); it != services.end( ); it++ )
  {
    if( (*it)->Tune( act ))
      return true;
  }
  return false;
}

void Channel::UpdateEPG( )
{
  Log( "Channel::UpdateEPG %s", name.c_str( ));
  for( std::vector<Service *>::const_iterator it = services.begin( ); it != services.end( ); it++ )
  {
    (*it)->GetTransponder( ).UpdateEPG( );
  }
}

void Channel::ClearEPG( )
{
  events.clear( );
}

void Channel::AddEPGEvent( const struct dvb_table_eit_event *event )
{
  Event *e = new Event( *this, event );
  events.push_back( e );
}


