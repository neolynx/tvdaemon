/*
 *  tvdaemon
 *
 *  DVB Frontend class
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

#include "Frontend.h"

#include "TVDaemon.h"
#include "Adapter.h"
#include "Frontend_DVBS.h"
#include "Frontend_DVBC.h"
#include "Frontend_DVBT.h"
#include "Frontend_ATSC.h"
#include "Port.h"
#include "Transponder.h"
#include "Source.h"
#include "Service.h"
#include "Log.h"
#include "Activity.h"
#include "Utils.h" // dump
#include "Channel.h"
#include "Activity_UpdateEPG.h"
#include "Activity_Scan.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm> // replace
#include <RPCObject.h>

#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-fe.h>
#include <errno.h> // ETIMEDOUT
#include <sys/time.h>

Frontend::Frontend( Adapter &adapter, std::string name, int adapter_id, int frontend_id, int config_id ) :
  ConfigObject( adapter, "frontend", config_id )
  , Thread( )
  , adapter(adapter)
  , fe(NULL)
  , name(name)
  , adapter_id(adapter_id)
  , frontend_id(frontend_id)
  , transponder(NULL)
  , activity(NULL)
  , current_port(0)
  , state(State_New)
  , usecount( 0 )
  , tune_timeout(5000)
  , up(true)
{
  ports.push_back( new Port( *this, 0 ));
  state = State_Ready;
  StartThread( );
}

Frontend::Frontend( Adapter &adapter, std::string configfile ) :
  ConfigObject( adapter, configfile )
  , Thread( )
  , adapter_id(-1)
  , frontend_id(-1)
  , adapter(adapter)
  , fe(NULL)
  , transponder(NULL)
  , activity(NULL)
  , current_port(0)
  , state(State_New)
  , usecount( 0 )
  , up(true)
{
  StartThread( );
}

Frontend::~Frontend( )
{
  if( fe )
    fe->abort = 1;

  up = false;
  JoinThread( );

  Close( );

  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    delete *it;
  }
}

Frontend *Frontend::Create( Adapter &adapter, std::string configfile )
{
  ConfigObject cfg;
  cfg.SetConfigFile( configfile );
  cfg.ReadConfigFile( );
  Source::Type type;
  cfg.ReadConfig( "Type", (int &) type );
  switch( type )
  {
    case Source::Type_DVBS:
      return new Frontend_DVBS( adapter, configfile );
    case Source::Type_DVBC:
      return new Frontend_DVBC( adapter, configfile );
    case Source::Type_DVBT:
      return new Frontend_DVBT( adapter, configfile );
    case Source::Type_ATSC:
      return new Frontend_ATSC( adapter, configfile );
    case Source::Type_Any:
      return NULL;
  }
  return NULL;
}

Frontend *Frontend::Create( Adapter &adapter, int adapter_id, int frontend_id, int config_id )
{
  std::string name;
  fe_delivery_system_t delsys;
  if( !GetInfo( adapter_id, frontend_id, &delsys, &name ))
    return NULL;

  switch( delsys )
  {
    case SYS_UNDEFINED:
    case SYS_DSS:
    case SYS_DVBH:
    case SYS_ISDBT:
    case SYS_ISDBS:
    case SYS_ISDBC:
    case SYS_DMBTH:
    case SYS_CMMB:
    case SYS_DAB:
    case SYS_TURBO:
      ::LogError( "%d.%d Delivery system %s not supported", adapter_id, frontend_id, delivery_system_name[delsys] );
      return NULL;
    case SYS_ATSC:
    case SYS_ATSCMH:
      return new Frontend_ATSC( adapter, name, adapter_id, frontend_id, config_id );
      break;
    case SYS_DVBT:
    case SYS_DVBT2:
      return new Frontend_DVBT( adapter, name, adapter_id, frontend_id, config_id );
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_B:
    case SYS_DVBC_ANNEX_C:
      return new Frontend_DVBC( adapter, name, adapter_id, frontend_id, config_id );
    case SYS_DVBS:
    case SYS_DVBS2:
      return new Frontend_DVBS( adapter, name, adapter_id, frontend_id, config_id );
  }
  return NULL;
}

Port *Frontend::AddPort( std::string name, int port_num )
{
  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->GetPortNum( ) == port_num )
    {
      LogError( "Port %d already exists", port_num );
      return NULL;
    }
  }
  Port *port = new Port( *this, ports.size( ), name, port_num );
  ports.push_back( port );
  return port;
}

bool Frontend::Open()
{
  if( fe )
    return true;
  if( !IsPresent( ))
  {
    LogWarn( "device not present '%s'", name.c_str( ));
    return false;
  }
  Log( "Opening /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
  fe = dvb_fe_open2( adapter_id, frontend_id, 0, 0, TVD_Log );
  if( !fe )
  {
    LogError( "Error opening /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
    return false;
  }
  state = State_Opened;
  return true;
}

void Frontend::Close()
{
  if( fe )
  {
    Log( "Closing /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
    dvb_fe_close( fe );
    transponder = NULL;
    fe = NULL;
    state = State_Ready;
    usecount = 0;
  }
}

bool Frontend::GetInfo( int adapter_id, int frontend_id, fe_delivery_system_t *delsys, std::string *name )
{
  struct dvb_v5_fe_parms *fe = dvb_fe_open( adapter_id, frontend_id, 0, 0 );
  if( !fe )
  {
    ::LogError( "Error opening /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
    return false;
  }

  if( delsys )
    *delsys = fe->current_sys;

  if( name )
    *name = fe->info.name;

  dvb_fe_close( fe );
  return true;
}

bool Frontend::SaveConfig( )
{
  WriteConfig( "Name", name );
  WriteConfig( "Type", type );
  WriteConfig( "TuneTimeout", tune_timeout );

  WriteConfigFile( );

  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Frontend::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;

  ReadConfig( "Name", name );
  ReadConfig( "Type", (int &) type );
  ReadConfig( "TuneTimeout", tune_timeout );
  if( tune_timeout == 0 )
    tune_timeout = 2500;

  if( !CreateFromConfig<Port, Frontend>( *this, "port", ports ))
    return false;
  state = State_Ready;
  return true;
}

void Frontend::SetFrontendId( int frontend_id )
{
  this->frontend_id = frontend_id;
  Log( "  Frontend on /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
}

int Frontend::GetFrontendId( ) const
{
  return frontend_id;
}

bool Frontend::SetPort( int id )
{
  if( current_port == id )
    return true;
  if( id < 0 or id >= ports.size( ))
  {
    LogError( "Frontend::SetPort unknown port %d", id );
    return false;
  }
  current_port = id;
  return true;
}

Port *Frontend::GetPort( int id )
{
  if( id >= ports.size( ))
  {
    LogError( "port %d not found", id );
    return NULL;
  }
  return ports[id];
}

Port *Frontend::GetCurrentPort( )
{
  return ports[current_port];
}

int Frontend::OpenDemux( )
{
  return dvb_dmx_open( adapter_id, frontend_id );
}

void Frontend::CloseDemux( int fd )
{
  dvb_dmx_close( fd );
}

bool Frontend::GetLockStatus( uint8_t &signal, uint8_t &noise, int timeout )
{
  if( !fe )
    return false;
  uint32_t snr = 0, sig = 0;
  uint32_t ber = 0, unc = 0;

  struct timeval ts;
  gettimeofday( &ts, NULL );
  long start = ts.tv_sec * 1000 + ts.tv_usec / 1000; // milli seconds

  while( state == State_Tuning && up )
  {
    int r = dvb_fe_get_stats( fe );
    if( r != 0 )
    {
      LogError( "Error getting frontend status" );
      return false;
    }

    uint32_t status = 0;
    dvb_fe_retrieve_stats( fe, DTV_STATUS, &status );
    if( status & FE_HAS_LOCK )
    {
      dvb_fe_retrieve_stats( fe, DTV_BER, &ber );
      dvb_fe_retrieve_stats( fe, DTV_SIGNAL_STRENGTH, &sig );
      dvb_fe_retrieve_stats( fe, DTV_UNCORRECTED_BLOCKS, &unc );
      dvb_fe_retrieve_stats( fe, DTV_SNR, &snr );

      //if( i > 0 )
      //printf( "\n" );
      Log( "Tuned: sig=%3u%% snr=%3u%% ber=%d unc=%d", (unsigned int) sig, (unsigned int) snr, ber, unc );
      sig *= 100;
      sig /= 0xffff;
      snr *= 100;
      snr /= 0xffff;

      Log( "Tuned: sig=%3u%% snr=%3u%% ber=%d unc=%d", (unsigned int) sig, (unsigned int) snr, ber, unc );

      signal = sig;
      noise  = snr;
      return true;
    }

    struct timeval ts2;
    gettimeofday( &ts2, NULL );

    if(( ts2.tv_sec * 1000 + ts2.tv_usec / 1000 ) - start > timeout )
      break;
    usleep( 10 );
    //printf( "." );
    //fflush( stdout );
  }
  //printf( "\n" );
  return false;
}

bool Frontend::Tune( Transponder &t )
{
  if( !IsPresent( ))
  {
    LogWarn( "device not present '%s'", name.c_str( ));
    return false;
  }
  Lock( );
  if( transponder )
  {
    if( transponder == &t )
    {
      usecount++;
      Log( "increasing usecount to %d", usecount );
      Unlock( );
      return true;
    }
    Log( "Frontend busy" );
    Unlock( );
    return false;
  }

  if( !Open( ))
  {
    Unlock( );
    return false;
  }

  state = State_Tuning;
  transponder = &t;
  usecount++;
  Log( "increasing usecount to %d", usecount );
  Unlock( );

  t.SetState( Transponder::State_Tuning );
  Log( "Tuning %s", t.toString( ).c_str( ));

  uint8_t signal, noise;
  int r = dvb_set_compat_delivery_system( fe, t.GetDelSys( ));
  if( r != 0 )
  {
    LogError( "dvb_set_compat_delivery_system return %d", r );
    goto fail;
  }

  SetTuneParams( t );
  t.GetParams( fe );
  r = dvb_fe_set_parms( fe );
  if( r != 0 )
  {
    LogError( "dvb_fe_set_parms failed with %d.", r );
    dvb_fe_prt_parms( fe );
    goto fail;
  }

  if( !GetLockStatus( signal, noise, tune_timeout ))
  {
    LogError( "Tuning failed" );
    goto fail;
  }

  t.SetState( Transponder::State_Tuned );
  t.SetSignal( signal, noise );
  return true;

fail:
  t.SetState( Transponder::State_TuningFailed );
  t.SaveConfig( );
  Release( );
  return false;
}

void Frontend::Release( )
{
  Lock( );
  if( usecount <= 0 )
  {
    LogError( "Cannot release unused Frontend" );
    Unlock( );
    return;
  }
  if( --usecount == 0 )
  {
    Log( "Use count 0, closing frontend" );
    Close( );
  }
  Unlock( );
}

bool Frontend::SetTuneParams( Transponder & )
{
  return true;
}

void Frontend::json( json_object *entry ) const
{
  json_object_object_add( entry, "name", json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "id",   json_object_new_int( GetKey( )));
  json_object_object_add( entry, "type", json_object_new_int( type ));

  json_object *a = json_object_new_array();
  for( std::vector<Port *>::const_iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    json_object *entry = json_object_new_object( );
    (*it)->json( entry );
    json_object_array_add( a, entry );
  }
  json_object_object_add( entry, "ports", a );
}

bool Frontend::RPC( const HTTPRequest &request, const std::string &cat, const std::string &action )
{
  if( cat == "port" )
  {
    int port_id;
    if( !request.GetParam( "port_id", port_id ))
      return false;

    if( port_id >= 0 && port_id < ports.size( ))
    {
      return ports[port_id]->RPC( request, cat, action );
    }
    return false;
  }

  if( action == "create_port" )
  {
    int port_num;
    if( !request.GetParam( "port_num", port_num ))
      return false;
    std::string name;
    if( !request.GetParam( "name", name ))
      return false;
    int source_id;
    if( !request.GetParam( "source_id", source_id ))
      return false;
    if( source_id < 0 )
    {
      LogError( "Invalid source: %d", source_id );
      return false;
    }

    Port *port = AddPort( name, port_num );
    if( port == NULL )
      return false;

    Source *source = TVDaemon::Instance( )->GetSource( source_id );
    if( !source )
      return false;

    source->AddPort( port );
    port->SetSource( source );
    request.Reply( HTTP_OK, port->GetKey( ));
    return true;
  }

  request.NotFound( "RPC unknown action: '%s'", action.c_str( ));
  return false;
}

void Frontend::Run( )
{
  Channel *channel;
  while( up )
  {
    if( !IsPresent( ))
    {
      sleep( 1 );
      continue;
    }

    bool idle = true;
    switch( state )
    {
      case State_Ready:
        {
          activity_lock.Lock( );
          Activity_Scan *act = new Activity_Scan( );
          act->SetFrontend( this );
          for( std::vector<Port *>::const_iterator it = ports.begin( ); it != ports.end( ); it++ )
          {
            if( (*it)->Scan( *act ))
            {
              activity_lock.Unlock( );
              act->Run( );
              idle = false;
              break;
            }
          }
          delete act;

          if( !idle )
            break;
          activity_lock.Unlock( );
          state = State_ScanEPG;
        }
        break;

      case State_ScanEPG:
        {
          activity_lock.Lock( );
          Activity_UpdateEPG *act = new Activity_UpdateEPG( );
          act->SetFrontend( this );
          for( std::vector<Port *>::const_iterator it = ports.begin( ); it != ports.end( ); it++ )
          {
            if( (*it)->ScanEPG( *act ))
            {
              activity_lock.Unlock( );
              act->Run( );
              idle = false;
              break;
            }
          }
          delete act;
          if( !idle )
            break;
          activity_lock.Unlock( );
          state = State_Ready;
        }
        break;
    }

    if( idle )
      sleep( 1 );
  }
}

void Frontend::Log( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  TVD_Log( LOG_INFO, "%d.%d %s", adapter.GetKey( ), GetKey( ), msg );
}

void Frontend::LogWarn( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  TVD_Log( LOG_WARNING, "%d.%d %s", adapter.GetKey( ), GetKey( ), msg );
}

void Frontend::LogError( const char *fmt, ... )
{
  char msg[255];
  va_list ap;
  va_start( ap, fmt );
  vsnprintf( msg, sizeof( msg ), fmt, ap );
  va_end( ap );
  TVD_Log( LOG_ERR, "%d.%d %s", adapter.GetKey( ), GetKey( ), msg );
}

bool Frontend::Tune( Activity &act )
{
  Port *port = act.GetPort( );
  if( !port )
    return false;
  Transponder *transponder = act.GetTransponder( );
  if( !transponder )
    return false;
  act.SetFrontend( this );
  if( !SetPort( port->GetKey( )))
    return false;
  if( !Tune( *transponder ))
    return false;
  return true;
}

bool Frontend::compare( const JSONObject &other, const int &p ) const
{
  const Frontend &b = (const Frontend &) other;
  return GetKey( ) < b.GetKey( );
}

