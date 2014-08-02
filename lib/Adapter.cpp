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
#include <RPCObject.h>
#include <string.h>  // strlen
#include <fcntl.h>   // open
#include <unistd.h>  // read

Adapter::Adapter( TVDaemon &tvd, const std::string &name, const std::string &uid, const int config_id ) :
  ConfigObject( tvd, "adapter", config_id ),
  tvd(tvd),
  uid(uid),
  frontends(),
  name(name)
{
  Log( "Create new adapter: '%s'", name.c_str() );
}

Adapter::Adapter( TVDaemon &tvd, const std::string &configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd),
  frontends(),
  name()
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

void Adapter::Delete( )
{
  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
    (*it)->Delete( );
  RemoveConfigFile( );
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

void Adapter::ResetPresence( )
{
  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); ++it)
  {
    (*it)->SetAdapterId( -1 );
    (*it)->SetFrontendId( -1 );
  }
}

bool Adapter::IsPresent( ) const
{
  for( std::vector<Frontend *>::const_iterator it = frontends.begin( ); it != frontends.end( ); ++it)
  {
    if( (*it)->IsPresent( ))
      return true;
  }
  return false;
}

bool Adapter::HasFrontend( int adapter_id, int frontend_id )
{
  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); ++it)
  {
    if( adapter_id == (*it)->GetAdapterId( ) && frontend_id == (*it)->GetFrontendId( ))
      return true;
  }
  return false;
}

void Adapter::SetFrontend( const std::string &name, const int adapter_id, const int frontend_id )
{
  // we have to be sure we are at the right adapter at this point

  // search for a not-present frontend
  for( std::vector<Frontend *>::const_iterator it = frontends.begin( ); it != frontends.end( ); ++it)
  {
    Frontend *f = *it;
    if( f->GetName( ) == name && false == f->IsPresent( ))
    {
      // we have found a not-taken frontend - occupy it
      f->SetAdapterId( adapter_id );
      f->SetFrontendId( frontend_id );
      return;
    }
    if( adapter_id == f->GetAdapterId( ) && frontend_id == f->GetFrontendId( ))
      return;
  }

  // no free frontend object found - we are creating a new one
  Frontend *f = Frontend::Create( *this, adapter_id, frontend_id, frontends.size( ));
  if( !f )
  {
    LogError( "%s: unable to create frontend '%s' adapter id '%d' frontend id '%d'",
              GetName( ).c_str( ), name.c_str( ), adapter_id, frontend_id );
    return;
  }
  frontends.push_back( f );
}

Frontend *Adapter::GetFrontend( const int id ) const
{
  if( id >= frontends.size( ))
    return NULL;
  return frontends[id];
}

void Adapter::json( json_object *entry ) const
{
  json_object_object_add( entry, "name", json_object_new_string( GetName().c_str( )));
  json_object_object_add( entry, "id",   json_object_new_int( GetKey( )));
  json_object_object_add( entry, "path", json_object_new_string( GetUID( ).c_str( )));
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
  if( cat != "port" and cat != "frontend" )
  {
    request.NotFound( "RPC: unknown action '%s'", action.c_str( ));
    return false;
  }
  std::string t;
  if( !request.GetParam( "frontend_id", t ))
    return false;
  int i = atoi( t.c_str( ));
  Frontend *fe = GetFrontend( i );
  if( !fe )
    return false;
  return fe->RPC( request, cat, action );
}

bool Adapter::compare( const JSONObject &other, const int &p ) const
{
  const Adapter &b = (const Adapter &) other;
  return GetKey( ) < b.GetKey( );
}
