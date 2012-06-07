/*
 *  tvheadend
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

#include "Transponder.h"
#include "Adapter.h"
#include "Frontend.h"
#include "Port.h"
#include "Log.h"

#include "dvb-file.h"

Source::Source( TVDaemon &tvh, std::string name, int config_id ) :
  ConfigObject( tvh, "source", config_id ),
  tvh(tvh), name(name),
  type(TVDaemon::Source_ANY)
{
}

Source::Source( TVDaemon &tvh, std::string configfile ) :
  ConfigObject( tvh, configfile ),
  tvh(tvh)
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
  Lookup( "Name", Setting::TypeString ) = name;

  SaveReferences<Port, Source>( *this, "Ports", ports );

  WriteConfig( );

  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Source::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  name = (const char *) Lookup( "Name", Setting::TypeString );

  Log( "Found configured Source '%s'", name.c_str( ));

  if( !CreateFromConfigFactory<Transponder, Source>( *this, "transponder", transponders ))
    return false;

  Setting &n = Lookup( "Ports", Setting::TypeList );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    Setting &n2 = n[i];
    if( n2.getLength( ) != 3 )
    {
      LogError( "Error in port path: should be [adapter, frontend, port] in %s", GetConfigFile( ).c_str( ));
      continue;
    }
    Adapter  *a = tvh.GetAdapter( n2[0] );
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
    Log( "Adding configured port [%d, %d, %d] to source %s", (int) n2[0], (int) n2[1], (int) n2[2], name.c_str( ));
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
  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( (*it)->IsSame( info ))
    {
      LogWarn( "Already known transponder: %s %d", delivery_system_name[delsys], frequency );
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
    return false;

  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->Scan( *t ))
      return true;
  }
  return false;
}


bool Source::Tune( Transponder &transponder, uint16_t pno )
{
  // FIXME: verify transponder is in this source
  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->Tune( transponder, pno ))
      return true;
  }
  return false;
}

