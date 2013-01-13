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

#include "TVDaemon.h"
#include "Transponder.h"
#include "Adapter.h"
#include "Frontend.h"
#include "Port.h"
#include "Log.h"
#include "Activity.h"

#include "dvb-file.h"
#include "descriptors.h"
#include <dirent.h>

Source::Source( TVDaemon &tvd, std::string name, Type type, int config_id ) :
  ConfigObject( tvd, "source", config_id ),
  ThreadBase( ),
  tvd(tvd), name(name),
  type(type)
{
}

Source::Source( TVDaemon &tvd, std::string configfile ) :
  ConfigObject( tvd, configfile ),
  tvd(tvd)
{
}

Source::~Source( )
{
  Lock( );
  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
    delete *it;
  Unlock( );
}

bool Source::SaveConfig( )
{
  WriteConfig( "Name", name );
  WriteConfig( "Type", type );

  SaveReferences<Port, Source>( *this, "Ports", ports );

  WriteConfigFile( );

  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if((*it)->IsModified( ))
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

  //Setting &n = ConfigList( "Ports" );
  //for( int i = 0; i < n.getLength( ); i++ )
  //{
    //Setting &n2 = n[i];
    //if( n2.getLength( ) != 3 )
    //{
      //LogError( "Error in port path: should be [adapter, frontend, port] in %s", GetConfigFile( ).c_str( ));
      //continue;
    //}
    //Adapter  *a = tvd.GetAdapter( n2[0] );
    //if( !a )
    //{
      //LogError( "Error in port path: adapter %d not found in %s", (int) n2[0], GetConfigFile( ).c_str( ));
      //continue;
    //}
    //Frontend *f = a->GetFrontend( n2[1] );
    //if( !f )
    //{
      //LogError( "Error in port path: frontend %d not found in %s", (int) n2[1], GetConfigFile( ).c_str( ));
      //continue;
    //}
    //Port     *p = f->GetPort( n2[2] );
    //if( !f )
    //{
      //LogError( "Error in port path: port %d not found in %s", (int) n2[2], GetConfigFile( ).c_str( ));
      //continue;
    //}
    //// FIXME: verify frontend type
    //Log( "  Adding configured port [%d, %d, %d] to source %s", (int) n2[0], (int) n2[1], (int) n2[2], name.c_str( ));
    //ports.push_back( p );
    //p->SetSource( GetKey( ));
  //}
  return true;
}

bool Source::ReadScanfile( std::string scanfile )
{
  std::string filename = SCANFILE_DIR;
  Utils::EnsureSlash( filename );
  switch( type )
  {
    case Type_DVBT:
      filename += "dvb-t/";
      break;
    case Type_DVBS:
      filename += "dvb-s/";
      break;
    case Type_DVBC:
      filename += "dvb-c/";
      break;
    case Type_ATSC:
      filename += "atsc/";
      break;
  }
  filename += scanfile;

  struct dvb_file *file = dvb_read_file_format( filename.c_str( ), SYS_UNDEFINED, FILE_CHANNEL );
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
  Lock( );
  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( (*it)->IsSame( *t ))
    {
      //LogWarn( "Already known transponder: %s", t->toString( ).c_str( ));
      Unlock( );
      return false;
    }
  }
  transponders.push_back( t );
  Unlock( );
  return true;
}

Transponder *Source::CreateTransponder( const struct dvb_entry &info )
{
  Lock( );
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
      type = Type_DVBC;
      break;
    case SYS_DVBT:
    case SYS_DVBT2:
      type = Type_DVBT;
      break;
    case SYS_DSS:
    case SYS_DVBS:
    case SYS_DVBS2:
      type = Type_DVBS;
      break;
    case SYS_ATSC:
    case SYS_ATSCMH:
      type = Type_ATSC;
      break;
  }

  for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( Transponder::IsSame( **it, info ))
    {
      //LogWarn( "Already known transponder: %s %d", delivery_system_name[delsys], frequency );
      Unlock( );
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
  Unlock( );
  return t;
}

Transponder *Source::GetTransponder( int id )
{
  // FIXME: locking ?
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
  port->SetSource( this );
  return true;
}

bool Source::RemovePort( Port *port )
{
  //FIXME: locking
  if( !port )
    return false;

  std::vector<Port *>::iterator it = std::find( ports.begin( ), ports.end( ), port );
  if( it != ports.end( ))
    ports.erase( it );
}

int Source::CountServices( ) const
{
  int count = 0;
  Lock( );
  for( std::vector<Transponder *>::const_iterator it = transponders.begin( ); it != transponders.end( ); it++ )
    count += (*it)->CountServices( );
  Unlock( );
  return count;
}

void Source::json( json_object *entry ) const
{
  json_object_object_add( entry, "name", json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "id",   json_object_new_int( GetKey( )));
  json_object_object_add( entry, "type", json_object_new_int( type ));
  json_object_object_add( entry, "transponders", json_object_new_int( transponders.size( )));
  json_object_object_add( entry, "services", json_object_new_int( CountServices( )));
}

bool Source::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( cat == "source" )
  {
    if( action == "show" )
    {
      HTTPServer::Response response;
      response.AddStatus( HTTP_OK );
      response.AddTimeStamp( );
      response.AddMime( "html" );
      std::string data;
      std::string filename = request.GetDocRoot( ) + "/source.html";
      int fd = open( filename.c_str( ), O_RDONLY );
      if( fd < 0 )
      {
        request.NotFound( "RPC source: template '%s' not found", filename.c_str( ));
        return false;
      }
      char tmp[256];
      int len;
      while(( len = read( fd, tmp, 255 )) > 0 )
      {
        tmp[len] = '\0';
        data += tmp;
      }
      snprintf( tmp, sizeof( tmp ), "%d", GetKey( ));
      size_t pos = 0;
      if(( pos = data.find( "@source_id@" )) != std::string::npos )
        data.replace( pos, strlen( "@source_id@" ), tmp );
      response.AddContents( data );
      request.Reply( response );
      return true;
    }
    else if( action == "get_transponders" )
    {
      json_object *h = json_object_new_object();
      json_object_object_add( h, "iTotalRecords", json_object_new_int( transponders.size( )));
      json_object *a = json_object_new_array();

      for( std::vector<Transponder *>::iterator it = transponders.begin( ); it != transponders.end( ); it++ )
      {
        json_object *entry = json_object_new_object( );
        (*it)->json( entry );
        json_object_array_add( a, entry );
      }

      json_object_object_add( h, "data", a );

      request.Reply( h );
      return true;
    }
  }
  else if( cat == "transponder" || cat == "service" )
  {
    std::string t;
    if( !request.GetParam( "transponder_id", t ))
      return false;

    int obj_id = atoi( t.c_str( ));
    if( obj_id >= 0 && obj_id < transponders.size( ))
    {
      return transponders[obj_id]->RPC( request, cat, action );
    }

    request.NotFound( "RPC transponder: unknown transponder" );
    return false;
  }


  if( action == "states" )
  {
    json_object *h = json_object_new_object();
    for( uint8_t i = 0; i < Transponder::State_Last; i++ )
    {
      char tmp[8];
      snprintf( tmp, sizeof( tmp ), "%d", i );
      json_object_object_add( h, tmp, json_object_new_string( Transponder::GetStateName((Transponder::State) i )));
    }
    const char *json = json_object_to_json_string( h );

    request.Reply( h );
    return true;
  }

  request.NotFound( "RPC: unknown action '%s'", action.c_str( ));
  return false;
}

std::vector<std::string> Source::GetScanfileList( Type type, std::string country )
{
  std::vector<std::string> result;

  const char *dirs[4] = { "dvb-t/", "dvb-c/", "dvb-s/", "atsc/" };
  for( int i = 0; i < 4; i++ )
  {
    if( type == Type_DVBT &&  i != 0 ) continue;
    if( type == Type_DVBC &&  i != 1 ) continue;
    if( type == Type_DVBS &&  i != 2 ) continue;
    if( type == Type_ATSC  &&  i != 3 ) continue;

    DIR *dirp;
    struct dirent *dp;
    char dir[128];
    snprintf( dir, sizeof( dir ), SCANFILE_DIR"%s", dirs[i]);
    if(( dirp = opendir( dir )) == NULL )
    {
      LogError( "couldn't open %s", dir  );
      continue;
    }

    while(( dp = readdir( dirp )) != NULL ) // FIXME: use reentrant
    {
      if( dp->d_name[0] == '.' ) continue;
      if( country != "" && i != 2 ) // DVB-S has no countries
      {
        if( country.compare( 0, 2, dp->d_name, 2 ) != 0 )
          continue;
      }
      std::string s ; //= dirs[i];
      s += dp->d_name;
      result.push_back( s );
    }
    closedir( dirp );
  }
  std::sort( result.begin( ), result.end( ));
  return result;
}

Transponder *Source::GetTransponderForScanning( )
{
  std::vector<Transponder *>::const_iterator it;
  Lock( );
  for( it = transponders.begin( ); it != transponders.end( ); it++ )
    if( (*it)->GetState( ) == Transponder::State_New )
    {
      (*it)->SetState( Transponder::State_Selected );
      break;
    }
  Unlock( );
  if( it != transponders.end( ))
    return *it;
  return NULL;
}

bool Source::Tune( Activity &act )
{
  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->Tune( act ))
      return true;
  }
  return false;
}
