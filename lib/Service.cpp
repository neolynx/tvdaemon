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

#include <json/json.h>
#include <strings.h> // strcasecmp
#include <string.h>  // strlen
#include <fcntl.h>   // open

#include "ConfigObject.h"
#include "Transponder.h"
#include "HTTPServer.h"
#include "Stream.h"
#include "Channel.h"
#include "Log.h"

Service::Service( Transponder &transponder, uint16_t service_id, uint16_t pid, int config_id ) :
  transponder(transponder),
  service_id(service_id),
  pid(pid),
  scrambled(false),
  channel(NULL)
{
}

Service::Service( Transponder &transponder ) :
  transponder(transponder),
  channel(NULL)
{
}

Service::~Service( )
{
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
  {
    delete it->second;
  }
}

void Service::SetName( std::string &s )
{
  name = s;
  Utils::ToLower( name, name_lower );
}

bool Service::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "ServiceID",    service_id );
  config.WriteConfig( "PID",          pid );
  config.WriteConfig( "Type",         type );
  config.WriteConfig( "Name",         name );
  config.WriteConfig( "Provider",     provider );
  config.WriteConfig( "Scrambled",    scrambled );
  config.WriteConfig( "Channel",      channel ? channel->GetKey( ) : -1 );

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
  std::string t;
  config.ReadConfig( "Name", t );
  SetName( t );
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
  json_object_object_add( entry, "name",      json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "provider",  json_object_new_string( provider.c_str( )));
  json_object_object_add( entry, "id",        json_object_new_int( GetKey( )));
  json_object_object_add( entry, "type",      json_object_new_int( type ));
  json_object_object_add( entry, "scrambled", json_object_new_int( scrambled ));
}

bool Service::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( cat == "service" )
  {
    if( action == "show" )
    {
      HTTPServer::Response response;
      response.AddStatus( HTTP_OK );
      response.AddTimeStamp( );
      response.AddMime( "html" );
      std::string data;
      std::string filename = request.GetDocRoot( ) + "/service.html";
      int fd = open( filename.c_str( ), O_RDONLY );
      if( fd < 0 )
      {
        request.NotFound( "RPC template not found" );
        return false;
      }
      char tmp[256];
      int len;
      while(( len = read( fd, tmp, 255 )) > 0 )
      {
        tmp[len] = '\0';
        data += tmp;
      }
      size_t pos = 0;
      snprintf( tmp, sizeof( tmp ), "%d", transponder.GetParent( ).GetKey( ));
      if(( pos = data.find( "@source_id@" )) != std::string::npos )
        data.replace( pos, strlen( "@source_id@" ), tmp );
      snprintf( tmp, sizeof( tmp ), "%d", transponder.GetKey( ));
      if(( pos = data.find( "@transponder_id@" )) != std::string::npos )
        data.replace( pos, strlen( "@transponder_id@" ), tmp );
      snprintf( tmp, sizeof( tmp ), "%d", GetKey( ));
      if(( pos = data.find( "@service_id@" )) != std::string::npos )
        data.replace( pos, strlen( "@service_id@" ), tmp );
      response.AddContents( data );
      request.Reply( response );
      return true;
    }
    else if( action == "list_streams" )
    {
      json_object *h = json_object_new_object();
      json_object_object_add( h, "iTotalRecords", json_object_new_int( streams.size( )));
      json_object *a = json_object_new_array();

      for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++ )
      {
        json_object *entry = json_object_new_object( );
        it->second->json( entry );
        json_object_array_add( a, entry );
      }

      json_object_object_add( h, "data", a );

      request.Reply( h );
      return true;
    }
    else if( action == "add_channel" )
    {
      Log( "Should add channel..." );
      return true;
    }
  }

  request.NotFound( "RPC transponder: unknown action" );
  return false;
}

bool Service::SortTypeName( Service *s1, Service *s2 )
{
  if( s1->type != s2->type )
    return s1->type < s2->type;
  return strcasecmp( s1->name.c_str( ), s2->name.c_str( )) < 0;
}

bool Service::SortByName( const Service *a, const Service *b )
{
  return a->name_lower < b->name_lower;
}

