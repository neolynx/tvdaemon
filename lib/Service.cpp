/*
 *  transpondereadend
 *
 *  DVB Service class
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

#include "Service.h"

#include <algorithm> // find
#include <json/json.h>
#include <strings.h> // strcasecmp

#include "ConfigObject.h"
#include "Transponder.h"
#include "HTTPServer.h"
#include "Stream.h"
#include "Log.h"

Service::Service( Transponder &transponder, uint16_t service_id, uint16_t pid, int config_id ) :
  transponder(transponder),
  service_id(service_id),
  pid(pid),
  scrambled(false)
{
}

Service::Service( Transponder &transponder ) :
  transponder(transponder)
{
}

Service::~Service( )
{
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    delete it->second;
  }
}

bool Service::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "ServiceID",    service_id );
  config.WriteConfig( "PID",          pid );
  config.WriteConfig( "Type",         type );
  config.WriteConfig( "Name",         name );
  config.WriteConfig( "Provider",     provider );
  config.WriteConfig( "Scrambled",    scrambled );

  config.DeleteConfig( "Streams" );
  Setting &n = config.ConfigList( "Streams" );
  ConfigBase c( n );
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    Setting &n2 = c.ConfigGroup( );
    ConfigBase c2( n2 );
    it->second->SaveConfig( c2 );
  }
  return true;
}

bool Service::LoadConfig( ConfigBase &config )
{
  config.ReadConfig( "ServiceID", service_id );
  config.ReadConfig( "PID", pid );
  config.ReadConfig( "Type", (int &) type );
  config.ReadConfig( "Name", name );
  config.ReadConfig( "Provider", provider );
  config.ReadConfig( "Scrambled", scrambled );

  Setting &n = config.ConfigList( "Streams" );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    ConfigBase c2( n[i] );
    Stream *s = new Stream( *this );
    s->LoadConfig( c2 );
    streams[s->GetKey( )] = s;
  }
  return true;
}

bool Service::UpdateStream( int id, Stream::Type type )
{
  std::map<uint16_t, Stream *>::iterator it = streams.find( id );
  Stream *s;
  if( it == streams.end( ))
  {
    s = new Stream( *this, id, type, streams.size( ));
    streams[id] = s;
    return true;
  }

  //LogWarn( "Already known Stream %d", id );
  s = it->second;
  return s->Update( type );
}

std::map<uint16_t, Stream *> &Service::GetStreams() // FIXME: const
{
  return streams;
}

bool Service::Tune( )
{
  return transponder.Tune( service_id );
}

const char *Service::GetTypeName( Type type )
{
  switch( type )
  {
    case Type_TV:
      return "TV";
    case Type_TVHD:
      return "HD TV";
    case Type_Radio:
      return "Radio";
  }
  return "Unknown";
}

void Service::json( json_object *entry ) const
{
  json_object_array_add( entry, json_object_new_string( name.c_str( )));
  json_object_array_add( entry, json_object_new_int( GetKey( )));
  json_object_array_add( entry, json_object_new_int( type ));
  json_object_array_add( entry, json_object_new_int( transponder.GetKey( )));
  json_object_array_add( entry, json_object_new_int( scrambled ));
}

bool Service::RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters )
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

  HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
  response->AddStatus( HTTP_NOT_FOUND );
  response->AddTimeStamp( );
  response->AddMime( "html" );
  response->AddContents( "RPC transponder: unknown action" );
  httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
  return false;
}

bool Service::SortTypeName( Service *s1, Service *s2 )
{
  if( s1->type != s2->type )
    return s1->type < s2->type;
  return strcasecmp( s1->name.c_str( ), s2->name.c_str( )) < 0;
}

