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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm> // replace
#include <json/json.h>

#include "dvb-demux.h"
#include "dvb-fe.h"
#include "dvb-scan.h"
#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/eit.h"
#include "descriptors/vct.h"
#include "descriptors/desc_service.h"
#include "descriptors/desc_network_name.h"
#include "descriptors/desc_event_short.h"
#include "descriptors/desc_hierarchy.h"

#include <errno.h> // ETIMEDOUT

Frontend::Frontend( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  ConfigObject( adapter, "frontend", config_id )
  , adapter(adapter)
  , fe(NULL)
  , adapter_id(adapter_id)
  , frontend_id(frontend_id)
  , present(false)
  , transponder(NULL)
  , current_port(0)
  , up(false)
{
  ports.push_back( new Port( *this, 0 ));
  pthread_mutex_init( &mutex, NULL );
  pthread_cond_init( &cond, NULL );

}

Frontend::Frontend( Adapter &adapter, std::string configfile ) :
  ConfigObject( adapter, configfile )
  , adapter(adapter)
  , fe(NULL)
  , present(false)
  , transponder(NULL)
  , current_port(0)
  , up(false)
{
  pthread_mutex_init( &mutex, NULL );
  pthread_cond_init( &cond, NULL );
}

Frontend::~Frontend( )
{
  if( fe )
    fe->abort = 1;

  state = Shutdown;

  if( up )
  {
    up = false;
    pthread_join( thread, NULL );
  }

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
  TVDaemon::SourceType type;
  cfg.ReadConfig( "Type", (int &) type );
  switch( type )
  {
    case TVDaemon::Source_DVB_S:
      return new Frontend_DVBS( adapter, configfile );
    case TVDaemon::Source_DVB_C:
      return new Frontend_DVBC( adapter, configfile );
    case TVDaemon::Source_DVB_T:
      return new Frontend_DVBT( adapter, configfile );
    case TVDaemon::Source_ATSC:
      return new Frontend_ATSC( adapter, configfile );
    case TVDaemon::Source_ANY:
      return NULL;
  }
  return NULL;
}

Frontend *Frontend::Create( Adapter &adapter, int adapter_id, int frontend_id, int config_id )
{
  fe_delivery_system_t delsys;
  if( !GetInfo( adapter_id, frontend_id, &delsys ))
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
      LogError( "Delivery system %s not supported", delivery_system_name[delsys] );
      return NULL;
    case SYS_ATSC:
    case SYS_ATSCMH:
      return new Frontend_ATSC( adapter, adapter_id, frontend_id, config_id );
      break;
    case SYS_DVBT:
    case SYS_DVBT2:
      return new Frontend_DVBT( adapter, adapter_id, frontend_id, config_id );
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_B:
    case SYS_DVBC_ANNEX_C:
      return new Frontend_DVBC( adapter, adapter_id, frontend_id, config_id );
    case SYS_DVBS:
    case SYS_DVBS2:
      return new Frontend_DVBS( adapter, adapter_id, frontend_id, config_id );
  }
  return NULL;
}

Port *Frontend::AddPort( std::string name, int port_id )
{
  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    if( (*it)->GetID( ) == port_id )
    {
      LogError( "Port %d already exists", port_id );
      return NULL;
    }
  }
  Port *port = new Port( *this, ports.size( ), name, port_id );
  ports.push_back( port );
  return port;
}

bool Frontend::Open()
{
  if( fe )
    return true;
// FIXME: handle adapter_id == -1
  if( adapter_id == -1 )
  {
    LogError( "adapter_id not set" );
    return false;
  }
  Log( "Opening /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
  fe = dvb_fe_open2( adapter_id, frontend_id, 0, 0, TVD_Log );
  if( !fe )
  {
    LogError( "Error opening /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
    return false;
  }
  state = Opened;
  return true;
}

void Frontend::Close()
{
  if( fe )
  {
    dvb_fe_close( fe );
    fe = NULL;
    state = Closed;
  }
}

bool Frontend::GetInfo( int adapter_id, int frontend_id, fe_delivery_system_t *delsys, std::string *name )
{
  //FIXME: verify this->fe not open
  struct dvb_v5_fe_parms *fe = dvb_fe_open( adapter_id, frontend_id, 0, 0 );
  if( !fe )
  {
    LogError( "Error opening /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
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
  WriteConfig( "Type", type );

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

  ReadConfig( "Type", (int &) type );

  if( !CreateFromConfig<Port, Frontend>( *this, "port", ports ))
    return false;
  return true;
}

void Frontend::SetIDs( int adapter_id, int frontend_id )
{
  Log( "  Frontend on /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
  this->adapter_id  = adapter_id;
  this->frontend_id = frontend_id;
}

bool Frontend::SetPort( int id )
{
  if( current_port == id )
    return true;
  if( id >= 0 && id < ports.size( )) // FIXME: verify not in use
  {
    current_port = id;
    return true;
  }
  return false;
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

bool Frontend::Tune( Transponder &t, int timeoutms )
{
  if( !Open( ))
    return false;

  t.SetState( Transponder::State_Tuning );
  Log( "Tuning %s", t.toString( ).c_str( ));
  int r = dvb_set_compat_delivery_system( fe, t.GetDelSys( ));
  if( r != 0 )
  {
    LogError( "dvb_set_compat_delivery_system return %d", r );
    t.SetState( Transponder::State_TuningFailed );
    return false;
  }
  t.GetParams( fe );

  //dvb_fe_prt_parms( fe );

  r = dvb_fe_set_parms( fe );
  if( r != 0 )
  {
    LogError( "dvb_fe_set_parms failed with %d.", r );
    t.SetState( Transponder::State_TuningFailed );
    dvb_fe_prt_parms( fe );
    return false;
  }

  //uint32_t freq;
  //dvb_fe_retrieve_parm( fe, DTV_FREQUENCY, &freq );
  //Log( "tuning to %d", freq );

  state = Tuning;

  uint8_t signal, noise;
  if( !GetLockStatus( signal, noise, 1500 ))
  {
    t.SetState( Transponder::State_TuningFailed );
    return false;
  }

  t.SetState( Transponder::State_Tuned );
  t.SetSignal( signal, noise );
  transponder = &t;
  return true;
}

void Frontend::Untune( )
{
  if( state != Tuned)
    return;
  state = Opened;
  transponder->SetState( Transponder::State_Idle );
  transponder = NULL;
}

bool Frontend::Scan( int timeoutms )
{
  if( !transponder )
    return false;
  transponder->SetState( Transponder::State_Scanning );
  if( !CreateDemuxThread( ))
  {
    transponder->SetState( Transponder::State_ScanningFailed );
    return false;
  }

  state = Scanning;
  struct timespec abstime;
  int ret, count = 0;
  do
  {
    clock_gettime( CLOCK_REALTIME, &abstime );
    abstime.tv_sec += 1;
    pthread_mutex_lock( &mutex );
    ret = pthread_cond_timedwait( &cond, &mutex, &abstime );
    pthread_mutex_unlock( &mutex );
  } while( state == Scanning & ret == ETIMEDOUT && ++count < 60 );
  if( ret == 0 || !up )
  {
    transponder->SetState( Transponder::State_Scanned );
    return true;
  }
  transponder->SetState( Transponder::State_ScanningFailed );
  if( ret == ETIMEDOUT )
    LogError( "Scanning timeouted (%d seconds)", count );
  else
    LogError( "Error waiting for scan to terminate" );
  if( up ) // FIXME: mutex
  {
    up = false;
    pthread_join( thread, NULL );
  }
  return false;
}

bool Frontend::TunePID( Transponder &t, uint16_t service_id )
{
  if( !Open( ))
    return false;
  if( this->transponder != NULL )
  {
    if( this->transponder->GetFrequency() != t.GetFrequency() )
    {
      LogError( "Transponder already tuned to another frequency" );
      return false;
    }
  }
  Service *s = t.GetService( service_id );
  if( !s )
  {
    LogError( "No service with id %d found", service_id);
    return false;
  }
  std::map<uint16_t, Stream *> &streams = s->GetStreams();
  uint16_t vpid = 0;
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ))
    {
      Log( "Video Stream: %s", it->second->GetTypeName( ));
      vpid = it->first;
      break;
    }
  }
  if( vpid == 0)
  {
    LogError( "no video stream for service %d found", service_id );
    return false;
  }

  uint16_t apid = 0;
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsAudio( ))
    {
      Log( "Audio Stream: %s", it->second->GetTypeName( ));
      apid = it->first;
      break;
    }
  }
  if( apid == 0)
  {
    LogError( "no audio stream for service %d found", service_id );
    return false;
  }

  if( !Tune( t ))
  {
    LogError( "Transponder tune failed" );
    return false;
  }

  int bufsize = 2 * 1024 * 1024;

  int fd_v = dvb_dmx_open( adapter_id, frontend_id );
  if( fd_v < 0 )
  {
    LogError( "unable to open adapter demux for recording" );
    return false;
  }
  int fd_a = dvb_dmx_open( adapter_id, frontend_id );
  if( fd_a < 0 )
  {
    LogError( "unable to open adapter demux for recording" );
    close( fd_v );
    // FIXME: cleanup
    return false;
  }

  std::string dumpfile;
  std::string upcoming;

  Log( "Reading EIT" );
  struct dvb_table_eit *eit;
  dvb_read_section_with_id( fe, fd_v, DVB_TABLE_EIT, DVB_TABLE_EIT_PID, service_id, (uint8_t **) &eit, 5 );
  if( eit )
  {
    dvb_table_eit_print( fe, eit );
    dvb_eit_event_foreach(event, eit)
    {
      if( event->running_status == 4 ) // now
      {
        dvb_desc_find( struct dvb_desc_event_short, desc, event, short_event_descriptor )
        {
          dumpfile = desc->name;
          break;
        }
      }
      if( event->running_status == 2 ) // starts in a few seconds
      {
        dvb_desc_find( struct dvb_desc_event_short, desc, event, short_event_descriptor )
        {
          upcoming = desc->name;
          break;
        }
      }
    }
    dvb_table_eit_free(eit);
  }

  if( !upcoming.empty( ))
  {
    dumpfile = upcoming;
  }
    //dvb_read_section_with_id( fe, fd_v, DVB_TABLE_EIT_SCHEDULE, DVB_TABLE_EIT_PID, service_id, (uint8_t **) &eit, &eitlen, 5 );
    //if( eit )
    //{
      //dvb_table_eit_print( fe, eit );
      //dvb_eit_event_foreach(event, eit)
      //{
        //if( event->running_status == 4 ) // now
        //{
          //dvb_desc_find( struct dvb_desc_event_short, desc, event, short_event_descriptor )
          //{
            //dumpfile = desc->name;
            //dumpfile += ".pes";
            //break;
          //}
        //}
      //}
      //free( eit );
    //}
  //}

  if( dumpfile.empty( ))
    dumpfile = "dump";
  else
  {
    std::replace( dumpfile.begin(), dumpfile.end(), '/', '_');
    std::replace( dumpfile.begin(), dumpfile.end(), '`', '_');
    std::replace( dumpfile.begin(), dumpfile.end(), '$', '_');
  }

  Log( "Setting PES filter for vpid %d", vpid );
  if( 0 != dvb_set_pesfilter( fd_v, vpid, DMX_PES_OTHER, DMX_OUT_TSDEMUX_TAP, bufsize ))
  {
    LogWarn( "failed to set the pid filter for pno %d", service_id );
    return false;
  }

  Log( "Setting PES filter for apid %d", apid );
  if( 0 != dvb_set_pesfilter( fd_a, apid, DMX_PES_OTHER, DMX_OUT_TSDEMUX_TAP, bufsize ))
  {
    LogWarn( "failed to set the pid filter for pno %d", service_id );
    return false;
  }

  {
    char file[256];

    std::string dir = "../";
    std::string filename = dir + dumpfile + ".pes";

    int i = 0;
    while( Utils::IsFile( filename ))
    {
      char num[16];
      if( ++i == 1000 )
      {
        LogError( "Too many files with same name: %s", filename.c_str( ));
        return false;
      }
      snprintf( num, sizeof( num ), "_%d", i );
      filename = dir + dumpfile + num + ".pes";
    }

    int file_fd = open( filename.c_str( ),
#ifdef O_LARGEFILE
        O_LARGEFILE |
#endif
        O_WRONLY | O_CREAT | O_TRUNC, 0644 );

    if( file_fd < 0 )
    {
      LogError( "Cannot open file '%s'", dumpfile.c_str( ));
      // FIXME: cleanup
      return false;
    }

    Log( "Recording '%s' ...", filename.c_str( ));

    fd_set tmp_fds;
    fd_set fds;
    FD_ZERO( &fds );
    int fdmax = fd_v > fd_a ? fd_v : fd_a;
    FD_SET ( fd_v, &fds );
    FD_SET ( fd_a, &fds );

    uint8_t *data = (uint8_t *) malloc( bufsize );
    int len;
    up = true;
    int ac, vc;
    ac = vc = 0;
    while( up )
    {
      tmp_fds = fds;

      struct timeval timeout = { 1, 0 }; // 1 sec
      if( select( FD_SETSIZE, &tmp_fds, NULL, NULL, &timeout ) == -1 )
      {
        LogError( "select error" );
        up = false;
        continue;
      }

      //printf( "available a:%d v:%d\n", FD_ISSET( fd_a, &tmp_fds ), FD_ISSET( fd_v, &tmp_fds ));

      if( FD_ISSET( fd_a, &tmp_fds ))
      {
        len = read( fd_a, data, bufsize );
        if( len < 0 )
        {
          LogError( "Audio: Error receiving data... %d", errno );
          //up = false;
          continue;
        }

        if( len == 0 )
        {
          Log( "Audio: no data received" );
          //up = false;
          continue;
        }

        int ret = write( file_fd, data, len );

        if( ac++ % 100 == 0 )
        {
          printf( "a" );
          fflush( stdout );
        }
      }

      if( FD_ISSET( fd_v, &tmp_fds ))
      {
        len = read( fd_v, data, bufsize );
        if( len < 0 )
        {
          LogError( "Video: Error receiving data... %d", errno );
          //up = false;
          continue;
        }

        if( len == 0 )
        {
          Log( "Video: no data received" );
          //up = false;
          continue;
        }

        int ret = write( file_fd, data, len );

        if( vc++ % 100 == 0 )
        {
          printf( "v" );
          fflush( stdout );
        }
      }

    }
    close( file_fd );
    free( data );
  }

  dvb_dmx_close( fd_a );
  dvb_dmx_close( fd_v );

  return true;
}

bool Frontend::GetLockStatus( uint8_t &signal, uint8_t &noise, int retries )
{
  if( !fe )
    return false;
  uint32_t status;
  uint32_t snr = 0, sig = 0;
  uint32_t ber = 0, unc = 0;

  for( int i = 0; i < retries && state == Tuning; i++ )
  {
    int r = dvb_fe_get_stats( fe );
    if( r < 0 )
    {
      LogError( "Error getting frontend status" );
      return false;
    }

    dvb_fe_retrieve_stats( fe, DTV_STATUS, &status );
    if( status & FE_HAS_LOCK )
    {
      dvb_fe_retrieve_stats( fe, DTV_BER, &ber );
      dvb_fe_retrieve_stats( fe, DTV_SIGNAL_STRENGTH, &sig );
      dvb_fe_retrieve_stats( fe, DTV_UNCORRECTED_BLOCKS, &unc );
      dvb_fe_retrieve_stats( fe, DTV_SNR, &snr );

      //if( i > 0 )
        //printf( "\n" );
      Log( "Tuned: signal %3u%% | snr %3u%% | ber %d | unc %d", (sig * 100) / 0xffff, (snr * 100) / 0xffff, ber, unc );

      signal = (sig * 100) / 0xffff;
      noise  = (snr * 100) / 0xffff;
      return true;
    }
    //usleep( 1 );
    //printf( "." );
    //fflush( stdout );
  }
  //printf( "\n" );
  return false;
}

bool Frontend::CreateDemuxThread( )
{
  if( up )
  {
    LogError( "demux thread already running" );
    return false;
  }
  if( pthread_create( &thread, NULL, run, (void *) this ) != 0 )
  {
    LogError( "failed to create thread" );
    return false;
  }
  return true;
}

void *Frontend::run( void *ptr )
{
  Frontend *t = (Frontend *) ptr;
  t->Thread( );
  pthread_exit( NULL );
  return NULL;
}

void Frontend::Thread( )
{
  fd_set fds;
  struct timeval tv;
  int ret;
  const char *err;

  up = true;

  //
  // open the demux device
  //
  int fd_demux = dvb_dmx_open( adapter_id, frontend_id );
  if( fd_demux < 0 )
  {
    LogError( "unable to open adapter demux" );
    state = Closing;
    up = false;
    return;
  }

  int time = 5;

  std::vector<uint16_t> services;

  Log( "Reading PAT" );
  struct dvb_table_pat *pat = NULL;
  dvb_read_section( fe, fd_demux, DVB_TABLE_PAT, DVB_TABLE_PAT_PID, (uint8_t **) &pat, time );
  if( !pat )
  {
    LogError( "Error reading PAT table" );
    up = false;
  }
  if( !up )
  {
    state = Closing;
    up = false;
    return;
  }

  Log( "  Setting TSID %d", pat->header.id );
  transponder->SetTSID( pat->header.id );
  Source &source = transponder->GetSource( );
  const std::vector<Transponder *> &transponders = source.GetTransponders( );
  for( std::vector<Transponder *>::const_iterator it = transponders.begin( ); it != transponders.end( ); it++ )
  {
    if( *it != transponder && (*it)->Enabled( ) && (*it)->GetTSID( ) == transponder->GetTSID( ))
    {
      LogWarn( "Disabling dupplicate transponder %s: same as %s", transponder->toString( ).c_str( ),
                                                                        (*it)->toString( ).c_str( ));
      transponder->Disable( );
      state = Closing;
      up = false;
      return;
    }
  }

  if( transponder->HasVCT( ))
  {
    Log( "Reading VCT" );
    struct dvb_table_vct *vct;
    dvb_read_section( fe, fd_demux, DVB_TABLE_VCT, DVB_TABLE_VCT_PID, (uint8_t **) &vct, time );
    if( vct && up )
    {
      dvb_table_vct_print( fe, vct );
      dvb_vct_channel_foreach( vct, channel )
      {
        std::string name = channel->short_name;
        std::string provider = "unknown";
        Service::Type type = Service::Type_Unknown;
        switch( channel->service_type )
        {
          default:
            type = Service::Type_TV;
            break;
        }

        if( type == Service::Type_Unknown )
        {
          LogWarn( "  Service %5d: %s '%s': unknown type: %d", channel->program_number, channel->access_control ? "§" : " ", name.c_str( ), channel->service_type );
          continue;
        }

        transponder->UpdateService( channel->program_number, type, name, provider, channel->access_control );
        services.push_back( channel->program_number );
      }
    }
    if( vct )
      dvb_table_vct_free( vct );
  }

  if( transponder->HasSDT( ))
  {
    Log( "Reading SDT" );
    struct dvb_table_sdt *sdt;
    dvb_read_section( fe, fd_demux, DVB_TABLE_SDT, DVB_TABLE_SDT_PID, (uint8_t **) &sdt, time );
    if( sdt )
    {
      //dvb_table_sdt_print( fe, sdt );
      dvb_sdt_service_foreach( service, sdt )
      {
        if( !up )
          break;
        const char *name = "", *provider = "";
        int service_type = -1;
        dvb_desc_find( struct dvb_desc_service, desc, service, service_descriptor )
        {
          service_type = desc->service_type;
          if( desc->name )
            name = desc->name;
          if( desc->provider )
            provider = desc->provider;
          break;
        }
        if( service_type == -1 )
        {
          LogWarn( "  No service descriptor found for service %d", service->service_id );
          continue;
        }

        Service::Type type = Service::Type_Unknown;
        switch( service_type )
        {
          case 0x01:
          case 0x16:
            type = Service::Type_TV;
            break;
          case 0x02:
            type = Service::Type_Radio;
            break;
          case 0x19:
            type = Service::Type_TVHD;
            break;
          case 0x0c:
            // Data ignored
            continue;

        }

        if( type == Service::Type_Unknown )
        {
          LogWarn( "  Service %5d: %s '%s': unknown type: %d", service->service_id, service->free_CA_mode ? "§" : " ", name, service_type );
          continue;
        }

        Log( "  Service %5d: %s %-6s '%s'", service->service_id, service->free_CA_mode ? "§" : " ", Service::GetTypeName( type ), name );
        transponder->UpdateService( service->service_id, type, name, provider, service->free_CA_mode );
        services.push_back( service->service_id );
      }

      dvb_table_sdt_free( sdt );
    }
  }

  //dvb_table_pat_print( fe, pat );
  Log( "Reading PMT's" );
  dvb_pat_program_foreach( program, pat )
  {
    if( !up )
      break;
    for( std::vector<uint16_t>::iterator it = services.begin( ); it != services.end( ); it++ )
    {
      if( *it == program->service_id )
      {
        if( program->service_id == 0 )
        {
          LogWarn( "  Ignoring PMT of service 0" );
          break;
        }

        transponder->UpdateProgram( program->service_id, program->pid );

        struct dvb_table_pmt *pmt;
        dvb_read_section( fe, fd_demux, DVB_TABLE_PMT, program->pid, (uint8_t **) &pmt, time );
        if( !pmt )
        {
          LogWarn( "  No PMT for pid %d", program->pid );
          break;
        }

        //dvb_table_pmt_print( fe, pmt );

        dvb_pmt_stream_foreach( stream, pmt )
        {
          if( !up )
            break;
          Stream::Type type = Stream::Type_Unknown;
          switch( stream->type )
          {
            case stream_video:            // 0x01
              type = Stream::Type_Video;
              break;
            case stream_video_h262:       // 0x02
              type = Stream::Type_Video_H262;
              break;
            case 0x1b:                    // 0x1b H.264 AVC
              type = Stream::Type_Video_H264;
              break;

            case stream_audio:            // 0x03
              type = Stream::Type_Audio;
              break;
            case stream_audio_13818_3:    // 0x04
              type = Stream::Type_Audio_13818_3;
              break;
            case stream_audio_adts:       // 0x0F
              type = Stream::Type_Audio_ADTS;
              break;
            case stream_audio_latm:       // 0x11
              type = Stream::Type_Audio_LATM;
              break;
            case stream_private + 1:      // 0x81  user private - in general ATSC Dolby - AC-3
              type = Stream::Type_Audio_AC3;
              break;

            case stream_private_sections: // 0x05
            case stream_private_data:     // 0x06
              dvb_desc_find( struct dvb_desc, desc, stream, AC_3_descriptor )
              {
                type = Stream::Type_Audio_AC3;
                break;
              }
              dvb_desc_find( struct dvb_desc, desc, stream, enhanced_AC_3_descriptor )
              {
                type = Stream::Type_Audio_AC3;
                LogWarn( "  Found AC3 enhanced" );
                break;
              }
              break;

            default:
              LogWarn( "  Ignoring stream type %d: %s", stream->type, pmt_stream_name[stream->type] );
              break;
          }
          if( type != Stream::Type_Unknown )
            transponder->UpdateStream( program->service_id, stream->elementary_pid, type );
        }

        dvb_table_pmt_free( pmt );
        break;
      }
    }
  }
  if( pat )
    dvb_table_pat_free( pat );
  if( !up )
  {
    state = Closing;
    up = false;
    return;
  }

  if( transponder->HasSDT( ))
  {
    Log( "Reading NIT" );
    struct dvb_table_nit *nit;
    dvb_read_section( fe, fd_demux, DVB_TABLE_NIT, DVB_TABLE_NIT_PID, (uint8_t **) &nit, time );
    if( nit && up )
    {
      dvb_desc_find( struct dvb_desc_network_name, desc, nit, network_name_descriptor )
      {
        Log( "  Network Name: %s", desc->network_name );
        //transponder->SetNetwork( desc->network_name );
        break;
      }
      //dvb_table_nit_print( fe, nit );
      HandleNIT( nit );
    }
    if( nit )
      dvb_table_nit_free( nit );
  }

  if( !up )
  {
    state = Closing;
    up = false;
    return;
  }

  //Log( "Reading EIT" );
  //struct dvb_table_eit *eit;
  //dvb_read_section_with_id( fe, fd_demux, DVB_TABLE_EIT_SCHEDULE, DVB_TABLE_EIT_PID, 1, (uint8_t **) &eit, time );
  //if( eit && up )
  //{
    //dvb_table_eit_print( fe, eit );
  //}
  //if( eit )
    //dvb_table_eit_free( eit );

  dvb_dmx_close( fd_demux );
  state = Closing;
  up = false;
  return;
}


void Frontend::json( json_object *entry ) const
{
  char name[32];
  snprintf( name, sizeof( name ), "Frontend%d", GetKey( ));
  json_object_array_add( entry, json_object_new_string( name ));
  json_object_array_add( entry, json_object_new_int( GetKey( )));
}

bool Frontend::RPC( HTTPServer *httpd, const int client, std::string &cat, const std::map<std::string, std::string> &parameters )
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

  if( cat == "port" )
  {
    if( action->second == "list" )
    {
      int count = ports.size( );
      json_object *h = json_object_new_object();
      //std::string echo =  parameters["sEcho"];
      int echo = 1; //atoi( parameters[std::string("sEcho")].c_str( ));
      json_object_object_add( h, "sEcho", json_object_new_int( echo ));
      json_object_object_add( h, "iTotalRecords", json_object_new_int( count ));
      json_object_object_add( h, "iTotalDisplayRecords", json_object_new_int( count ));
      json_object *a = json_object_new_array();

      for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
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
  }

  HTTPServer::HTTPResponse *response = new HTTPServer::HTTPResponse( );
  response->AddStatus( HTTP_NOT_FOUND );
  response->AddTimeStamp( );
  response->AddMime( "html" );
  response->AddContents( "RPC transponder: unknown action" );
  httpd->SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
  return false;
}

