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

#include "dvb-demux.h"
#include "dvb-fe.h"
#include "dvb-scan.h"
#include "descriptors/pat.h"
#include "descriptors/pmt.h"
#include "descriptors/nit.h"
#include "descriptors/sdt.h"
#include "descriptors/eit.h"
#include "descriptors/desc_service.h"
#include "descriptors/desc_network_name.h"
#include "descriptors/desc_event_short.h"

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
  cfg.ReadConfig( );
  TVDaemon::SourceType type = (TVDaemon::SourceType) (int) cfg.Lookup( "Type", Setting::TypeInt );
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
  Lookup( "Type", Setting::TypeInt ) = type;

  WriteConfig( );

  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    (*it)->SaveConfig( );
  }
  return true;
}

bool Frontend::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  type = (TVDaemon::SourceType) (int) Lookup( "Type", Setting::TypeInt );

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

  Log( "Tuning %s", t.toString( ).c_str( ));
  int r = dvb_set_compat_delivery_system( fe, t.GetDelSys( ));
  if( r != 0 )
  {
    LogError( "dvb_set_compat_delivery_system return %d", r );
    return false;
  }
  t.GetParams( fe );

  //dvb_fe_prt_parms( fe );

  r = dvb_fe_set_parms( fe );
  if( r != 0 )
  {
    LogError( "dvb_fe_set_parms failed with %d.", r );
    dvb_fe_prt_parms( fe );
    return false;
  }

  //uint32_t freq;
  //dvb_fe_retrieve_parm( fe, DTV_FREQUENCY, &freq );
  //Log( "tuning to %d", freq );

  state = Tuning;

  if( !GetLockStatus( ))
    return false;

  transponder = &t;
  return true;
}

void Frontend::Untune()
{
  if( state != Tuned)
    return;
  state = Opened;
  transponder = NULL;
}

bool Frontend::Scan( Transponder &transponder, int timeoutms )
{
  if( !CreateDemuxThread( ))
    return false;
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
    return true;
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
    if( it->second->GetType( ) == Stream::Video )
    {
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
    if( it->second->GetType( ) == Stream::Audio )
    {
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
          dumpfile += ".pes";
          break;
        }
      }
    }
    dvb_table_eit_free(eit);
  }

  //if( dumpfile.empty( ))
  //{
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
    dumpfile = "dump.pes";

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
    Log( "Recording %s ...", dumpfile.c_str( ));

    char file[256];

    dumpfile = "/home/bay/tv/" + dumpfile;
    int file_fd = open( dumpfile.c_str( ),
#ifdef O_LARGEFILE
        O_LARGEFILE |
#endif
        O_WRONLY | O_CREAT, 0644);

    fd_set tmp_fds;
    fd_set fds;
    FD_ZERO( &fds );
    int fdmax = fd_v > fd_a ? fd_v : fd_a;
    FD_SET ( fd_v, &fds );
    FD_SET ( fd_a, &fds );

    uint8_t *data = (uint8_t *) malloc( bufsize << 1 );
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

      if( FD_ISSET( fd_v, &tmp_fds ))
      {
        len = read( fd_v, data, bufsize << 1 );
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

        write( file_fd, data, len );

        if( vc++ % 100 == 0 )
        {
          printf( "v" );
          fflush( stdout );
        }
      }

      if( FD_ISSET( fd_a, &tmp_fds ))
      {
        len = read( fd_a, data, bufsize << 1 );
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

        write( file_fd, data, len );

        if( ac++ % 100 == 0 )
        {
          printf( "a" );
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

bool Frontend::GetLockStatus( int timeout )
{
  if( !fe )
    return false;
  uint32_t status;
  uint32_t snr = 0, _signal = 0;
  uint32_t ber = 0, uncorrected_blocks = 0;

  for( int i = 0; i < timeout && state == Tuning; i++ )
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
      dvb_fe_retrieve_stats( fe, DTV_SIGNAL_STRENGTH, &_signal );
      dvb_fe_retrieve_stats( fe, DTV_UNCORRECTED_BLOCKS, &uncorrected_blocks );
      dvb_fe_retrieve_stats( fe, DTV_SNR, &snr );

      Log( "Tuned: signal %3u%% | snr %3u%% | ber %d | unc %d", status, (_signal * 100) / 0xffff, (snr * 100) / 0xffff, ber, uncorrected_blocks );

      return true;
    }
    usleep( 1000000 );
  }
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
    up = false;
    return;
  }

  int time = 5;

  std::vector<uint16_t> services;

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
        name = desc->name;
        provider = desc->provider;
        break;
      }
      if( service_type == -1 )
      {
        LogWarn( "  No service descriptor found" );
        continue;
      }

      const char *type = NULL;
      switch( service_type )
      {
        case 0x01:
        case 0x16:
          type = "TV";
          break;
        case 0x02:
          type = "Radio";
          break;
        case 0x19:
          type = "HD TV";
          break;
        case 0x0c:
          // Data ignored
          continue;

      }

      if( type == NULL )
      {
        LogWarn( "  Unknown Service %d type %d", service->service_id, service_type );
        continue;
      }

      Log( "  Found %s service %5d%s: '%s'", type, service->service_id, service->free_CA_mode ? " §" : "", name );
      transponder->UpdateProgram( service->service_id, name, provider );
      services.push_back( service->service_id );
    }

    dvb_table_sdt_free( sdt );
  }

  Log( "Reading PAT" );
  struct dvb_table_pat *pat = NULL;
  dvb_read_section( fe, fd_demux, DVB_TABLE_PAT, DVB_TABLE_PAT_PID, (uint8_t **) &pat, time );
  if( !pat )
  {
    LogError( "Error reading PAT table" );
    up = false;
  }
  if( !up )
    return;

  //dvb_table_pat_print( fe, pat );
  Log( "  Reading PMT's" );
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
          LogWarn( "Ignoring PMT of service 0" );
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
          switch( stream->type )
          {
            case video_stream_descriptor:
            case mpeg4_video_descriptor:
            case audio_stream_descriptor:
            case mpeg4_audio_descriptor:
              transponder->UpdateStream(program->service_id, stream->elementary_pid, stream->type );
              break;
            default:
              LogWarn( "Ignoring stream type %s", dvb_descriptors[stream->type].name );
              break;
          }
        }

        dvb_table_pmt_free( pmt );
        break;
      }
    }
  }
  if( pat )
    dvb_table_pat_free( pat );
  if( !up )
    return;

  Log( "Reading NIT" );
  struct dvb_table_nit *nit;
  dvb_read_section( fe, fd_demux, DVB_TABLE_NIT, DVB_TABLE_NIT_PID, (uint8_t **) &nit, time );
  if( nit && up )
  {
    dvb_desc_find( struct dvb_desc_network_name, desc, nit, network_name_descriptor )
      if( desc )
      {
        Log( "  Network Name: %s", desc->network_name );
        //transponder->SetNetwork( desc->network_name );
      }
    //dvb_table_nit_print( fe, nit );
    HandleNIT( nit );
  }
  if( nit )
    dvb_table_nit_free( nit );
  if( !up )
    return;

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


