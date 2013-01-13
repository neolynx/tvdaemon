/*
 *  tvdaemon
 *
 *  DVB Port class
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

#include "Port.h"

#include "Source.h"
#include "TVDaemon.h"
#include "Frontend.h"
#include "Log.h"
#include "Activity_Scan.h"

#include <json/json.h>
#include <stdlib.h> // atoi

Port::Port( Frontend &frontend, int config_id, std::string name, int port_num ) :
  ConfigObject( frontend, "port", config_id ),
  frontend(frontend),
  name(name),
  port_num(port_num),
  source(NULL)
{
}

Port::Port( Frontend &frontend, std::string configfile ) :
  ConfigObject( frontend, configfile ),
  frontend(frontend),
  source(NULL)
{
}

Port::~Port( )
{
}

bool Port::SaveConfig( )
{
  WriteConfig( "Name",   name );
  WriteConfig( "ID", port_num );
  WriteConfig( "Source", source ? source->GetKey( ) : -1 );

  return WriteConfigFile( );
}

bool Port::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;

  int source_id;

  ReadConfig( "Name",   name );
  ReadConfig( "ID",     port_num );
  ReadConfig( "Source", source_id );

  if( source_id >= 0 )
  {
    source = TVDaemon::Instance( )->GetSource( source_id );
    if( source == NULL )
    {
      LogError( "Source with id %d not found", source_id );
      return false;
    }
    source->AddPort( this );
  }
  return true;
}

bool Port::Scan( )
{
  if( !source )
    return false;
  Transponder *t = source->GetTransponderForScanning( );
  if( !t )
    return false;
  Log( "Scanning Transponder %d: %s", t->GetKey( ), t->toString( ).c_str( ));

  Activity_Scan *act = new Activity_Scan( t );
  act->Run( );
  delete act;
  return true;
}

void Port::json( json_object *entry ) const
{
  json_object_object_add( entry, "name",      json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "id",        json_object_new_int( GetKey( )));
  json_object_object_add( entry, "port_num",   json_object_new_int( port_num ));
  json_object_object_add( entry, "source_id", json_object_new_int( source ? source->GetKey( ) : -1 ));
}

bool Port::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( action == "set" )
  {
    int port_num;
    std::string name;
    int source_id;

    if( !request.GetParam( "port_num", port_num ))
      return false;
    this->port_num = port_num;

    if( !request.GetParam( "name", name ))
      return false;
    this->name = name;

    if( !request.GetParam( "source_id", source_id ))
      return false;

    if( source && ( source_id == -1 || source_id != source->GetKey( )))
    {
      source->RemovePort( this );
      source = NULL;
    }

    if( source_id != -1 )
    {
      if( !source || ( source && source_id != source->GetKey( )))
      {
        source = TVDaemon::Instance( )->GetSource( source_id );
        source->AddPort( this );
        SetSource( source );
      }
    }

    request.Reply( HTTP_OK );
    return true;
  }

  request.NotFound( "Port::RPC unknown action '%s'", action.c_str( ));
  return false;
}

bool Port::Tune( Activity &act )
{
  return frontend.Tune( *this, act );
}

