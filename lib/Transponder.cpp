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
  enabled(true),
  state(New)
{
}

Transponder::Transponder( Source &source, std::string configfile ) :
  ConfigObject( source, configfile ),
  source(source)
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
  cfg.ReadConfig( );
  fe_delivery_system_t delsys = (fe_delivery_system_t) (int) cfg.Lookup( "DelSys", Setting::TypeInt );
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
  Lookup( "DelSys",            Setting::TypeInt ) = (int) delsys;
  Lookup( "Frequency",         Setting::TypeInt ) = (int) frequency;
  Lookup( "TransportStreamID", Setting::TypeInt ) = TransportStreamID;
  Lookup( "VersionNumber",     Setting::TypeInt ) = VersionNumber;
  Lookup( "Enabled",           Setting::TypeInt ) = (int) enabled;
  Lookup( "State",             Setting::TypeInt ) = (int) state;

  for( std::map<uint16_t, Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
  {
    it->second->SaveConfig( );
  }

  return WriteConfig( );
}

bool Transponder::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  delsys            = (fe_delivery_system_t) (int) Lookup( "DelSys", Setting::TypeInt );
  frequency         = (uint32_t) (int) Lookup( "Frequency",          Setting::TypeInt );
  TransportStreamID =            (int) Lookup( "TransportStreamID",  Setting::TypeInt );
  VersionNumber     =            (int) Lookup( "VersionNumber",      Setting::TypeInt );
  enabled           = (bool)     (int) Lookup( "Enabled",            Setting::TypeInt );
  state             = (State)    (int) Lookup( "State",              Setting::TypeInt );

  if( !CreateFromConfig<Service, uint16_t, Transponder>( *this, "service", services ))
    return false;
  return true;
}

bool Transponder::UpdateProgram( uint16_t pno, uint16_t pid )
{
  std::map<uint16_t, Service *>::iterator it = services.find( pno );
  if( it == services.end( ))
  {
    Service *s = new Service( *this, pno, pid, services.size( ));
    services[pno] = s;
    return true;
  }
  Service *s = it->second;
  if( s->GetPID( ) != pid )
  {
    LogError( "pid mismatch in service %d: %d != %d", pno, s->GetPID( ), pid );
    s->SetPID( pid );
    return false;
  }
  LogWarn( "Already known Service with pno %d, pid %d", pno, pid );
  return true;
}

bool Transponder::UpdateProgram( uint16_t pno, std::string name, std::string provider )
{
  Service *s = NULL;
  std::map<uint16_t, Service *>::iterator it = services.find( pno );
  if( it == services.end( ))
  {
    s = new Service( *this, pno, 0, services.size( ));
    services[pno] = s;
  }
  else
    s = it->second;
  s->SetName( name );
  s->SetProvider( provider );
  return true;
}

bool Transponder::UpdateStream( uint16_t sid, int id, int type )
{
  for( std::map<uint16_t, Service *>::iterator it = services.begin( ); it != services.end( ); it++ )
    if( it->second->GetPID( ) == sid )
    {
      it->second->UpdateStream( id, (Stream::Type) type );
      return true;
    }

  LogError( "Service with id %d not found", sid );
  return false;
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
  dvb_fe_store_parm( params, DTV_INVERSION, INVERSION_AUTO );
  return true;
}
