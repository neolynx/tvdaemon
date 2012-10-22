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
  {
    delete *it;
  }
}

bool Adapter::SaveConfig( )
{
  Lookup( "Name", Setting::TypeString )    = name;
  Lookup( "UDev-ID", Setting::TypeString ) = uid;

  WriteConfig( );

  for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Adapter::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;
  name = (const char *) Lookup( "Name", Setting::TypeString );
  uid  = (const char *) Lookup( "UDev-ID", Setting::TypeString );

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
  json_object_array_add( entry, json_object_new_string( name.c_str( )));
  json_object_array_add( entry, json_object_new_int( GetKey( )));
}

bool Adapter::RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters )
{
  const std::map<std::string, std::string>::const_iterator action = parameters.find( "a" );
  if( action == parameters.end( ))
  {
    HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
    response->AddStatus( HTTP_NOT_FOUND );
    response->AddTimeStamp( );
    response->AddMime( "html" );
    response->AddContents( "RPC source: action not found" );
    httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
    return false;
  }

  if( cat == "frontend" )
  {
    if( action->second == "list" )
    {
      int count = frontends.size( );
      json_object *h = json_object_new_object();
      //std::string echo =  parameters["sEcho"];
      int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
      json_object_object_add( h, "sEcho", json_object_new_int( echo ));
      json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
      json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));
      json_object *a = json_object_new_array();

      for( std::vector<Frontend *>::iterator it = frontends.begin( ); it != frontends.end( ); it++ )
      {
        json_object *entry = json_object_new_array( );
        (*it)->json( entry );
        json_object_array_add( a, entry );
      }

      json_object_object_add( h, "aaData", a );

      const char *json = json_object_to_json_string( h );

      HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
      response->AddStatus( HTTP_OK );
      response->AddTimeStamp( );
      response->AddMime( "json" );
      response->AddContents( json );
      httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
      json_object_put( h ); // this should delete it
      return true;
    }
  }

  if( cat == "port" )
  {
    const std::map<std::string, std::string>::const_iterator data = parameters.find( "frontend" );
    if( data == parameters.end( ))
    {
      HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
      response->AddStatus( HTTP_NOT_FOUND );
      response->AddTimeStamp( );
      response->AddMime( "html" );
      response->AddContents( "RPC: adapter not found" );
      httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
      return false;
    }

    int id = atoi( data->second.c_str( ));

    if( id >= 0 && id < frontends.size( ))
    {
      return frontends[id]->RPC( httpd, client, cat, parameters );
    }
  }

  HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
  response->AddStatus( HTTP_NOT_FOUND );
  response->AddTimeStamp( );
  response->AddMime( "html" );
  response->AddContents( "RPC transponder: unknown action" );
  httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
  return false;
}

