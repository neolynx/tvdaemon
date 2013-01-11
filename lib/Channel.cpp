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
#include "Log.h"

Channel::Channel( TVDaemon &tvd, Service *service, int config_id ) :
  ConfigObject( tvd, "channel", config_id ),
  tvd(tvd),
  number(config_id)
{
  name = service->GetName( );
  services.push_back( service );
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
  WriteConfig( "Name", name );
  WriteConfig( "Number", number );

  return WriteConfigFile( );
}

bool Channel::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;
  ReadConfig( "Name", name );
  ReadConfig( "Number", number );
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
  Log( "Adding Service %d to Channel %s", service->GetKey( ), name.c_str( ));
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

bool Channel::Tune( )
{
  for( std::vector<Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
  {
    if( (*it)->Tune( ))
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
  //const std::map<std::string, std::string>::const_iterator action = parameters.find( "a" );
  //if( action == parameters.end( ))
  //{
    //httpd->NotFound( client, "RPC source: action not found" );
    //return false;
  //}

  //if( cat == "transponder" )
  //{
    //if( action->second == "list" )
    //{
      //int count = transponders.size( );
      //json_object *h = json_object_new_object();
      ////std::string echo =  parameters["sEcho"];
      //int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
      //json_object_object_add( h, "sEcho", json_object_new_int( echo ));
      //json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
      //json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));
      //json_object *a = json_object_new_array();

      //for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
      //{
        //json_object *entry = json_object_new_array( );
        //(*it)->json( entry );
        //json_object_array_add( a, entry );
      //}

      //json_object_object_add( h, "aaData", a );

      //const char *json = json_object_to_json_string( h );

      //HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
      //response->AddStatus( HTTP_OK );
      //response->AddTimeStamp( );
      //response->AddMime( "json" );
      //response->AddContents( json );
      //httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
      //json_object_put( h ); // this should delete it
      //return true;
    //}

    //if( action->second == "states" )
    //{
      //json_object *h = json_object_new_object();
      //for( uint8_t i = 0; i < Transponder::State_Last; i++ )
      //{
        //char tmp[8];
        //snprintf( tmp, sizeof( tmp ), "%d", i );
        //json_object_object_add( h, tmp, json_object_new_string( Transponder::GetStateName((Transponder::State) i )));
      //}
      //const char *json = json_object_to_json_string( h );

      //HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
      //response->AddStatus( HTTP_OK );
      //response->AddTimeStamp( );
      //response->AddMime( "json" );
      //response->AddContents( json );
      //httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
      //json_object_put( h ); // this should delete it
      //return true;
    //}
  //}

  //if( cat == "service" )
  //{
    //const std::map<std::string, std::string>::const_iterator obj = parameters.find( "transponder" );
    //if( obj == parameters.end( ))
    //{
      //if( action->second == "list" )
      //{
        //int count = 0;
        //json_object *h = json_object_new_object();
        ////std::string echo =  parameters["sEcho"];
        //int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
        //json_object_object_add( h, "sEcho", json_object_new_int( echo ));
        //json_object *a = json_object_new_array();

        //std::list<Service *> list;
        //for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
        //{
          //const std::map<uint16_t, Service *> &services = (*it)->GetServices( );
          //for( std::map<uint16_t, Service *>::const_iterator it2 = services.begin( ); it2 != services.end( ); it2++ )
          //{
            //list.push_back( it2->second );
            //count++;
          //}
        //}
        //list.sort( Service::SortTypeName );

        //for( std::list<Service *>::iterator it = list.begin( ); it != list.end( ); it++ )
        //{
          //json_object *entry = json_object_new_array( );
          //(*it)->json( entry );
          //json_object_array_add( a, entry );
        //}

        //json_object_object_add( h, "aaData", a );
        //json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
        //json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));

        //const char *json = json_object_to_json_string( h );

        //HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
        //response->AddStatus( HTTP_OK );
        //response->AddTimeStamp( );
        //response->AddMime( "json" );
        //response->AddContents( json );
        //httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
        //json_object_put( h ); // this should delete it
        //return true;
      //}

      //if( action->second == "types" )
      //{
        //json_object *h = json_object_new_object();
        //for( uint8_t i = 0; i < Service::Type_Last; i++ )
        //{
          //char tmp[8];
          //snprintf( tmp, sizeof( tmp ), "%d", i );
          //json_object_object_add( h, tmp, json_object_new_string( Service::GetTypeName((Service::Type) i )));
        //}
        //const char *json = json_object_to_json_string( h );

        //HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
        //response->AddStatus( HTTP_OK );
        //response->AddTimeStamp( );
        //response->AddMime( "json" );
        //response->AddContents( json );
        //httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
        //json_object_put( h ); // this should delete it
        //return true;
      //}
      //HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
      //response->AddStatus( HTTP_NOT_FOUND );
      //response->AddTimeStamp( );
      //response->AddMime( "html" );
      //response->AddContents( "RPC transponder: unknown action" );
      //httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
      //return false;
    //}

    //int obj_id = atoi( obj->second.c_str( ));
    //if( obj_id >= 0 && obj_id < transponders.size( ))
    //{
      //return transponders[obj_id]->RPC( httpd, client, cat, parameters );
    //}
  //}

  request.NotFound( "RPC transponder: unknown action" );
  return false;
}
