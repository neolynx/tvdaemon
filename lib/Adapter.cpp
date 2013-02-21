/*
 *  tvdaemon
 *
 *  DVB Adapter class
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

#include "Adapter.h"

#include "TVDaemon.h"
#include "Frontend.h"
#include "Utils.h"
#include "Log.h"

#include <algorithm> // find
#include <json/json.h>
#include <string.h>  // strlen
#include <fcntl.h>   // open
#include <unistd.h>  // read

Adapter::Adapter( TVDaemon &tvd, std::string uid, std::string name, int config_id ) :
  ConfigObject( tvd, "adapter", config_id ),
  tvd(tvd),
  uid(uid),
  name(name),
  present(false)
{
  Log( "Creating new Adapter: %s", uid.c_str( ));
}

Adapter::Adapter( TVDaemon &tvd, std::string configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd),
  present(false)
{
}

Adapter::~Adapter( )
{
  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
    delete *it;
}

void Adapter::Shutdown( )
{
  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
    (*it)->Shutdown( );
}

bool Adapter::SaveConfig( )
{
  WriteConfig( "Name", name );
  WriteConfig( "UDev-ID", uid );

  WriteConfigFile( );

  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Adapter::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;
  ReadConfig( "Name", name );
  ReadConfig( "UDev-ID", uid );

  Log( "Loading Adapter '%s'", name.c_str( ));

  if( !CreateFromConfigFactory<Frontend, Adapter>( *this, "frontend", frontends ))
    return false;
  return true;
}

void Adapter::SetFrontend( std::string frontend, int adapter_id, int frontend_id )
{
  if( std::find( ftempnames.begin( ), ftempnames.end( ), frontend ) != ftempnames.end( ))
  {
    LogError( "Frontend already set" );
    return;
  }
  int id = ftempnames.size( );
  ftempnames.push_back( frontend );

  Frontend *f = NULL;
  if( id >= frontends.size( ))
  {
    // create frontend
    f = Frontend::Create( *this, adapter_id, frontend_id, frontends.size( ));
    if( f )
      frontends.push_back( f );
  }
  else
  {
    f = frontends[id];
    f->SetIDs( adapter_id, frontend_id );
  }

  if( f )
    f->SetPresence( true );
}

void Adapter::SetPresence( bool present )
{
  if( this->present && present )
    return; // already present

  if( this->present && !present ) // removed
  {
    ftempnames.clear( );
  }

  if( present )
    Log( "Adapter %d hardware is present '%s'", GetKey( ), name.c_str( ));
  else
    Log( "Adapter %d hardware removed '%s'", GetKey( ), name.c_str( ));

  this->present = present;
}

Frontend *Adapter::GetFrontend( int id )
{
  if( id >= frontends.size( ))
    return NULL;
  return frontends[id];
}

void Adapter::json( json_object *entry ) const
{
  json_object_object_add( entry, "name", json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "id",   json_object_new_int( GetKey( )));
  json_object_object_add( entry, "path", json_object_new_string( uid.c_str( )));
  json_object_object_add( entry, "present", json_object_new_int( IsPresent( )));
  json_object *a = json_object_new_array();

  for( std::vector<Frontend *>::const_iterator it = frontends.begin( ); it != frontends.end( ); it++ )
  {
    json_object *entry = json_object_new_object( );
    (*it)->json( entry );
    json_object_array_add( a, entry );
  }

  json_object_object_add( entry, "frontends", a );
}

bool Adapter::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( cat == "port" or cat == "frontend" )
  {
    std::string t;
    if( !request.GetParam( "frontend_id", t ))
      return false;
    int i = atoi( t.c_str( ));
    if( i >= 0 && i < frontends.size( ))
    {
      return frontends[i]->RPC( request, cat, action );
    }
  }

  request.NotFound( "RPC: unknown action '%s'", action.c_str( ));
  return false;
}

