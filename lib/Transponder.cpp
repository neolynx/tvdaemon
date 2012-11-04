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
#include <json/json.h>

#include "Source.h"
#include "Service.h"
#include "Transponder_DVBS.h"
#include "Transponder_DVBT.h"
#include "Transponder_DVBC.h"
#include "Transponder_ATSC.h"
#include "Log.h"

Transponder::Transponder( Source &source, const fe_delivery_system_t delsys, int config_id ) :
  ConfigObject( source, "transponder", config_id ),
  source(source),
  delsys(delsys),
  signal(0), noise(0),
  TSID(0),
  enabled(true),
  state(State_New),
  has_nit(false),
  has_sdt(false),
  has_vct(false)
{
  SetModified( );
}

Transponder::Transponder( Source &source, std::string configfile ) :
  ConfigObject( source, configfile ),
  source(source),
  has_nit(false),
  has_sdt(false),
  has_vct(false)
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

bool Transponder::Tune( uint16_t pno )
{
  return source.Tune( *this, pno );
}

bool Transponder::GetParams( struct dvb_v5_fe_parms *params ) const
{
  dvb_fe_store_parm( params, DTV_FREQUENCY, frequency );
  return true;
}

const char *Transponder::GetStateName( State state )
{
  switch( state )
  {
    case State_New:
      return "New";
    case State_Tuning:
      return "Tuning";
    case State_Tuned:
      return "Tuned";
    case State_TuningFailed:
      return "Tuning Failed";
    case State_Scanning:
      return "Scanning";
    case State_Scanned:
      return "Scanned";
    case State_ScanningFailed:
      return "Scanning Failed";
    case State_Idle:
      return "Idle";
    default:
      return "Unknown";
  }
  return "Unknown";
}

void Transponder::json( json_object *entry ) const
{
  json_object_array_add( entry, json_object_new_string( toString( ).c_str( )));
  json_object_array_add( entry, json_object_new_int( GetKey( )));
  json_object_array_add( entry, json_object_new_int( state ));
  json_object_array_add( entry, json_object_new_int( enabled ));
}

bool Transponder::RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters )
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

