/*
 *  tvdaemon
 *
 *  DVB Transponder class
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

#include "Transponder.h"

#include "dvb-file.h"
#include <RPCObject.h>

#include "Source.h"
#include "Service.h"
#include "Transponder_DVBS.h"
#include "Transponder_DVBT.h"
#include "Transponder_DVBC.h"
#include "Transponder_ATSC.h"
#include "Log.h"
#include "HTTPServer.h"
#include "Activity.h"

Transponder::Transponder( Source &source, const fe_delivery_system_t delsys, int config_id ) :
  ConfigObject( source, "transponder", config_id ),
  source(source),
  delsys(delsys),
  signal(0), noise(0),
  TSID(0),
  enabled(true),
  state(State_New),
  last_epg_update(0),
  last_epg_failed(0),
  has_channels(false),
  has_nit(false),
  has_sdt(false),
  has_vct(false),
  has_mgt(false),
  epg_state(EPGState_Missing)
{
  SetModified( );
}

Transponder::Transponder( Source &source, std::string configfile ) :
  ConfigObject( source, configfile ),
  source(source),
  last_epg_failed(0),
  has_channels(false),
  has_nit(false),
  has_sdt(false),
  has_vct(false),
  has_mgt(false),
  epg_state(EPGState_Missing)
{
}

Transponder::~Transponder( )
{
  for( std::map<uint16_t, Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
  {
    delete it->second;
  }
}

Transponder *Transponder::Create( Source &source, const fe_delivery_system_t delsys, int config_id )
{
  switch( delsys )
  {
    case SYS_DVBS:
    case SYS_DVBS2:
      return new Transponder_DVBS( source, delsys, config_id );
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_B:
    case SYS_DVBC_ANNEX_C:
      return new Transponder_DVBC( source, delsys, config_id );
    case SYS_DVBT:
    case SYS_DVBT2:
      return new Transponder_DVBT( source, delsys, config_id );
    case SYS_ATSC:
    case SYS_ATSCMH:
      return new Transponder_ATSC( source, delsys, config_id );
    default:
      LogError( "Unknown delivery system: %s", delivery_system_name[delsys] );
      break;
  }
  return NULL;
}

Transponder *Transponder::Create( Source &source, std::string configfile )
{
  ConfigObject cfg;
  cfg.SetConfigFile( configfile );
  cfg.ReadConfigFile( );
  fe_delivery_system_t delsys;
  cfg.ReadConfig( "DelSys", (int &) delsys );
  switch( delsys )
  {
    case SYS_DVBS:
    case SYS_DVBS2:
      return new Transponder_DVBS( source, configfile );
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_B:
    case SYS_DVBC_ANNEX_C:
      return new Transponder_DVBC( source, configfile );
    case SYS_DVBT:
    case SYS_DVBT2:
      return new Transponder_DVBT( source, configfile );
    case SYS_ATSC:
    case SYS_ATSCMH:
      return new Transponder_ATSC( source, configfile );
    default:
      LogError( "Unknown delivery system: %s", delivery_system_name[delsys] );
      break;
  }
  return NULL;
}

/**
 * @brief check whether a Transponder and a record from a scan file are the same
 *
 * @param transponder
 * @param info
 *
 * @return true if same
 */
bool Transponder::IsSame( const Transponder &transponder, const struct dvb_entry &info )
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
  if( transponder.delsys != delsys ||
      transponder.frequency != frequency )
    return false;

  //FIXME: call virtual IsSame
  return true;
}

void Transponder::AddProperty( const struct dtv_property &prop )
{
  switch( prop.cmd )
  {
    case DTV_DELIVERY_SYSTEM:
      delsys = (fe_delivery_system_t) prop.u.data;
      break;
    case DTV_FREQUENCY:
      frequency = prop.u.data;
      break;
  }
}

bool Transponder::SaveConfig( )
{
  WriteConfig( "DelSys",    delsys );
  WriteConfig( "Frequency", (int) frequency );
  WriteConfig( "TSID",      TSID );
  WriteConfig( "Enabled",   enabled );
  WriteConfig( "State",     state );
  WriteConfig( "Signal",    signal );
  WriteConfig( "Noise",     noise );
  WriteConfig( "LastEPGUpdate", last_epg_update );

  DeleteConfig( "Services" );
  Setting &n = ConfigList( "Services" );
  ConfigBase c( n );
  for( std::map<uint16_t, Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
  {
    Setting &n2 = c.ConfigGroup( );
    ConfigBase c2( n2 );
    it->second->SaveConfig( c2 );
  }

  return WriteConfigFile( );
}

bool Transponder::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;

  ReadConfig( "DelSys", (int &) delsys );
  ReadConfig( "Frequency",      frequency );
  ReadConfig( "TSID",           TSID );
  ReadConfig( "Enabled",        enabled );
  ReadConfig( "State",  (int &) state );
  ReadConfig( "Signal",         signal );
  ReadConfig( "Noise",          noise );
  ReadConfig( "LastEPGUpdate",  last_epg_update );

  Setting &n = ConfigList( "Services" );
  for( int i = 0; i < n.getLength( ); i++ )
  {
    ConfigBase c2( n[i] );
    Service *s = new Service( *this );
    s->LoadConfig( c2 );
    services[s->GetKey( )] = s;
  }
  return true;
}

bool Transponder::UpdateProgram( uint16_t service_id, uint16_t pid )
{
  std::map<uint16_t, Service *>::iterator it = services.find( service_id );
  if( it == services.end( ))
  {
    Service *s = new Service( *this, service_id, pid, services.size( ));
    services[service_id] = s;
    SetModified( );
    return true;
  }
  Service *s = it->second;
  if( s->GetPID( ) != pid )
  {
    if( s->GetPID( ) != 0 )
      LogError( "pid mismatch in service %d: %d != %d", service_id, s->GetPID( ), pid );
    s->SetPID( pid );
    SetModified( );
    return false;
  }
  //LogWarn( "Already known Service with pno %d, pid %d", pno, pid );
  return true;
}

bool Transponder::UpdateService( uint16_t service_id, Service::Type type, std::string name, std::string provider, bool scrambled )
{
  Service *s = NULL;
  std::map<uint16_t, Service *>::iterator it = services.find( service_id );
  if( it == services.end( ))
  {
    s = new Service( *this, service_id, 0, services.size( ));
    services[service_id] = s;
  }
  else
    s = it->second;
  s->SetType( type );
  s->SetName( name );
  s->SetProvider( provider );
  s->SetScrambled( scrambled );
  SetModified( );
  return true;
}

bool Transponder::UpdateStream( uint16_t service_id, int id, int type )
{
  std::map<uint16_t, Service *>::iterator it = services.find( service_id );
  if( it == services.end( ))
  {
    LogError( "Service with id %d not found", service_id );
    return false;
  }
  it->second->UpdateStream( id, (Stream::Type) type );
  SetModified( );
  return true;
}

Service *Transponder::GetService( uint16_t id )
{
  std::map<uint16_t, Service *>::iterator it = services.find( id );
  if( it == services.end( ))
    return NULL;
  return it->second;
}

bool Transponder::GetParams( struct dvb_v5_fe_parms *params ) const
{
  dvb_fe_store_parm( params, DTV_FREQUENCY, frequency );
  return true;
}

void Transponder::SetState( State state )
{
  this->state = state;
}

void Transponder::SetEPGState( EPGState state )
{
  epg_state = state;
  if( state != EPGState_Updated && state != EPGState_Updating )
    last_epg_failed = time( NULL );
}


const char *Transponder::GetStateName( State state )
{
  switch( state )
  {
    case State_New:            return "New";
    case State_Selected:       return "Selected";
    case State_Tuning:         return "Tuning";
    case State_Tuned:          return "Tuned";
    case State_TuningFailed:   return "Tuning Failed";
    case State_Scanning:       return "Scanning";
    case State_Scanned:        return "Scanned";
    case State_ScanningFailed: return "Scanning Failed";
    case State_Idle:           return "Idle";
    case State_Duplicate:      return "Duplicate";
    case State_Last:           return NULL;
  }
  return NULL;
}

void Transponder::json( json_object *entry ) const
{
  json_object_object_add( entry, "name",    json_object_new_string( toString( ).c_str( )));
  json_object_object_add( entry, "id",      json_object_new_int( GetKey( )));
  json_object_object_add( entry, "state",   json_object_new_int( state ));
  json_object_object_add( entry, "epg_state",   json_object_new_int( epg_state ));
  json_object_object_add( entry, "enabled", json_object_new_int( enabled ));
  json_object_object_add( entry, "delsys", json_object_new_int( delsys ));
  json_object_object_add( entry, "frequency", json_object_new_int( frequency ));
  json_object_object_add( entry, "tsid", json_object_new_int( TSID ));
  json_object_object_add( entry, "signal", json_object_new_int( signal ));
  json_object_object_add( entry, "noise", json_object_new_int( noise ));
  json_object_object_add( entry, "services", json_object_new_int( services.size( )));
}

bool Transponder::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( cat == "transponder" )
  {
    if( action == "show" )
    {
      HTTPServer::Response response;
      response.AddStatus( HTTP_OK );
      response.AddTimeStamp( );
      response.AddMime( "html" );
      std::string data;
      std::string filename = request.GetDocRoot( ) + "/transponder.html";
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
      snprintf( tmp, sizeof( tmp ), "%d", source.GetKey( ));
      size_t pos = 0;
      if(( pos = data.find( "@source_id@" )) != std::string::npos )
        data.replace( pos, strlen( "@source_id@" ), tmp );
      snprintf( tmp, sizeof( tmp ), "%d", GetKey( ));
      if(( pos = data.find( "@transponder_id@" )) != std::string::npos )
        data.replace( pos, strlen( "@transponder_id@" ), tmp );
      response.AddContents( data );
      request.Reply( response );
      return true;
    }

    else if( action == "list_services" )
    {
      json_object *h = json_object_new_object();
      json_object_object_add( h, "iTotalRecords", json_object_new_int( services.size( )));
      json_object *a = json_object_new_array();

      for( std::map<uint16_t, Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
      {
        json_object *entry = json_object_new_object( );
        it->second->json( entry );
        json_object_array_add( a, entry );
      }

      json_object_object_add( h, "data", a );
      request.Reply( h );
      return true;
    }
  }
  else if( cat == "service" )
  {
    int service_id;
    if( !request.GetParam( "service_id", service_id ))
      return false;
    const std::map<uint16_t, Service *>::const_iterator it = services.find( service_id );
    if( it != services.end( ))
    {
      return it->second->RPC( request, cat, action );
    }

    request.NotFound( "RPC service: unknown service" );
    return false;
  }

  request.NotFound( "RPC transponder: unknown action" );
  return false;
}

void Transponder::SetTSID( uint16_t TSID )
{
  this->TSID = TSID;
  const std::vector<Transponder *> &transponders = source.GetTransponders( );
  for( std::vector<Transponder *>::const_iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( *it != this && (*it)->Enabled( ) && (*it)->GetTSID( ) == TSID )
    {
      LogWarn( "Disabling duplicate transponder %s: same as %s", toString( ).c_str( ), (*it)->toString( ).c_str( ));
      Disable( );
      SetState( State_Duplicate );
      SaveConfig( );
    }
  }
}

bool Transponder::Tune( Activity &act )
{
  act.SetTransponder( this );
  return source.Tune( act );
}

bool Transponder::UpdateEPG( )
{
  last_epg_update = 0;
  last_epg_failed = 0;
}

bool Transponder::ReadEPG( const struct dvb_table_eit_event *event )
{
  for( std::map<uint16_t, Service *>::const_iterator it = services.begin( ); it != services.end( ); it++ )
    it->second->ReadEPG( event );
  last_epg_update = time( NULL );
  SetEPGState( EPGState_Updated );
  SaveConfig( );
  return true;
}

bool Transponder::compare( const JSONObject &other, const int &p ) const
{
  const Transponder &b = (const Transponder &) other;
  if( delsys == b.delsys )
    return frequency < b.frequency;
  return delsys < b.delsys;
}


