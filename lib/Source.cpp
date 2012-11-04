/*
 *  tvdaemon
 *
 *  DVB Source class
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

#include "Source.h"

#include <algorithm> // find
#include <json/json.h>

#include "Transponder.h"
#include "Adapter.h"
#include "Frontend.h"
#include "Port.h"
#include "Log.h"

#include "dvb-file.h"
#include "descriptors.h"

Source::Source( TVDaemon &tvd, std::string name, int config_id ) :
  ConfigObject( tvd, "source", config_id ),
  tvd(tvd), name(name),
  type(TVDaemon::Source_ANY)
{
}

Source::Source( TVDaemon &tvd, std::string configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd)
{
}

Source::~Source( )
{
  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    delete *it;
  }
}

bool Source::SaveConfig( )
{
  WriteConfig( "Name", name );
  WriteConfig( "Type", type );

  SaveReferences<Port, Source>( *this, "Ports", ports );

  WriteConfigFile( );

  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Source::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;

  ReadConfig( "Name", name );
  ReadConfig( "Type", (int &) type );

  Log( "Loading Source '%s'", name.c_str( ));

  if( !CreateFromConfigFactory<Transponder, Source>( *this, "transponder", transponders ))
    return false;

  Setting &n = ConfigList( "Ports" );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    Setting &n2 = n[i];
    if( n2.getLength( ) != 3 )
    {
      LogError( "Error in port path: should be [adapter, frontend, port] in %s", GetConfigFile( ).c_str( ));
      continue;
    }
    Adapter  *a = tvd.GetAdapter( n2[0] );
    if( !a )
    {
      LogError( "Error in port path: adapter %d not found in %s", (int) n2[0], GetConfigFile( ).c_str( ));
      continue;
    }
    Frontend *f = a->GetFrontend( n2[1] );
    if( !f )
    {
      LogError( "Error in port path: frontend %d not found in %s", (int) n2[1], GetConfigFile( ).c_str( ));
      continue;
    }
    Port     *p = f->GetPort( n2[2] );
    if( !f )
    {
      LogError( "Error in port path: port %d not found in %s", (int) n2[2], GetConfigFile( ).c_str( ));
      continue;
    }
    // FIXME: verify frontend type
    Log( "  Adding configured port [%d, %d, %d] to source %s", (int) n2[0], (int) n2[1], (int) n2[2], name.c_str( ));
    ports.push_back( p );
  }
  return true;
}

bool Source::ReadScanfile( std::string scanfile )
{
  struct dvb_file *file = dvb_read_file_format( scanfile.c_str( ), SYS_UNDEFINED, FILE_CHANNEL );
  if( !file )
  {
    LogError( "Failed to parse '%s'", scanfile.c_str( ));
    return false;
  }

  for( struct dvb_entry *entry = file->first_entry; entry != NULL; entry = entry->next )
  {
    CreateTransponder( *entry );
  }

  return true;
}

bool Source::AddTransponder( Transponder *t )
{
  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( (*it)->IsSame( *t ))
    {
      //LogWarn( "Already known transponder: %s", t->toString( ).c_str( ));
      return false;
    }
  }
  transponders.push_back( t );
  return true;
}

Transponder *Source::CreateTransponder( const struct dvb_entry &info )
{
  fe_delivery_system_t delsys;
  uint32_t frequency;
  for( int i = 0; i < info.n_props; i++ )
    switch( info.props[i].cmd )
    {
      case DTV_DELIVERY_SYSTEM:
        delsys = (fe_delivery_system_t) info.props[i].u.data;
        break;
      case DTV_FREQUENCY:
        frequency = info.props[i].u.data;
        break;
    }
  switch( delsys )
  {
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_B:
    case SYS_DVBC_ANNEX_C:
      type = TVDaemon::Source_DVB_C;
      break;
    case SYS_DVBT:
    case SYS_DVBT2:
      type = TVDaemon::Source_DVB_T;
      break;
    case SYS_DSS:
    case SYS_DVBS:
    case SYS_DVBS2:
      type = TVDaemon::Source_DVB_S;
      break;
    case SYS_ATSC:
    case SYS_ATSCMH:
      type = TVDaemon::Source_ATSC;
      break;
  }

  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( Transponder::IsSame( **it, info ))
    {
      //LogWarn( "Already known transponder: %s %d", delivery_system_name[delsys], frequency );
      return NULL;
    }
  }
  Log( "Creating Transponder: %s, %d", delivery_system_name[delsys], frequency );
  Transponder *t = Transponder::Create( *this, delsys, transponders.size( ));
  if( t )
  {
    transponders.push_back( t );
    for( int i = 0; i < info.n_props; i++ )
      t->AddProperty( info.props[i] );
  }
  return t;
}

Transponder *Source::GetTransponder( int id )
{
  if( id >= transponders.size( ))
    return NULL;
  return transponders[id];
}

bool Source::AddPort( Port *port )
{
  if( !port )
    return false;
  if( std::find( ports.begin( ), ports.end( ), port ) != ports.end( ))
  {
    LogWarn( "Port already added to Source %s", name.c_str( ));
    return false;
  }

  // FIXME: verify frontend type
  ports.push_back( port );
  return true;
}

bool Source::TuneTransponder( int id )
{
  Transponder* t = GetTransponder( id );
  if( !t )
    return false;

  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->Tune( *t ))
      return true;
  }
  return false;
}

bool Source::ScanTransponder( int id )
{
  Transponder *t = GetTransponder( id );
  if( !t )
  {
    LogError( "Transponder with id %d not found", id );
    return false;
  }

  if( t->Disabled( ))
  {
    LogWarn( "Transponder is disabled" );
    return false;
  }

  // FIXME: support scanning on next free transponder
  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( !(*it)->Scan( *t ))
      return false;
    t->SaveConfig( );
    return true;
  }
  return false;
}


bool Source::Tune( Transponder &transponder, uint16_t pno )
{
  if( this != &transponder.GetSource() )
  {
    LogError("Transponder '%d' not in this source '%s'", transponder.GetFrequency(), name.c_str());
    return false;
  }
  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->Tune( transponder, pno ))
      return true;
  }
  return false;
}

void Source::json( json_object *entry ) const
{
  json_object_object_add( entry, "name", json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "id",   json_object_new_int( GetKey( )));
  json_object_object_add( entry, "type", json_object_new_int( type ));
}

bool Source::RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters )
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

  if( cat == "transponder" )
  {
    if( action->second == "list" )
    {
      int count = transponders.size( );
      json_object *h = json_object_new_object();
      //std::string echo =  parameters["sEcho"];
      int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
      json_object_object_add( h, "sEcho", json_object_new_int( echo ));
      json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
      json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));
      json_object *a = json_object_new_array();

      for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
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

    if( action->second == "states" )
    {
      json_object *h = json_object_new_object();
      for( uint8_t i = 0; i < Transponder::State_Last; i++ )
      {
        char tmp[8];
        snprintf( tmp, sizeof( tmp ), "%d", i );
        json_object_object_add( h, tmp, json_object_new_string( Transponder::GetStateName((Transponder::State) i )));
      }
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

  if( cat == "service" )
  {
    const std::map<std::string, std::string>::const_iterator obj = parameters.find( "transponder" );
    if( obj == parameters.end( ))
    {
      if( action->second == "list" )
      {
        int count = 0;
        json_object *h = json_object_new_object();
        //std::string echo =  parameters["sEcho"];
        int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
        json_object_object_add( h, "sEcho", json_object_new_int( echo ));
        json_object *a = json_object_new_array();

        std::list<Service *> list;
        for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
        {
          const std::map<uint16_t, Service *> &services = (*it)->GetServices( );
          for( std::map<uint16_t, Service *>::const_iterator it2 = services.begin( ); it2 != services.end( ); it2++ )
          {
            list.push_back( it2->second );
            count++;
          }
        }
        list.sort( Service::SortTypeName );

        for( std::list<Service *>::iterator it = list.begin( ); it != list.end( ); it++ )
        {
          json_object *entry = json_object_new_array( );
          (*it)->json( entry );
          json_object_array_add( a, entry );
        }

        json_object_object_add( h, "aaData", a );
        json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
        json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));

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

      if( action->second == "types" )
      {
        json_object *h = json_object_new_object();
        for( uint8_t i = 0; i < Service::Type_Last; i++ )
        {
          char tmp[8];
          snprintf( tmp, sizeof( tmp ), "%d", i );
          json_object_object_add( h, tmp, json_object_new_string( Service::GetTypeName((Service::Type) i )));
        }
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
      HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
      response->AddStatus( HTTP_NOT_FOUND );
      response->AddTimeStamp( );
      response->AddMime( "html" );
      response->AddContents( "RPC transponder: unknown action" );
      httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
      return false;
    }

    int obj_id = atoi( obj->second.c_str( ));
    if( obj_id >= 0 && obj_id < transponders.size( ))
    {
      return transponders[obj_id]->RPC( httpd, client, cat, parameters );
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

