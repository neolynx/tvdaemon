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
#include "Recorder.h"
#include "Utils.h" // dump

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
#include "descriptors/mpeg_ts.h"
#include "descriptors/mpeg_pes.h"
#include "descriptors/desc_service.h"
#include "descriptors/desc_network_name.h"
#include "descriptors/desc_event_short.h"
#include "descriptors/desc_hierarchy.h"

#include <errno.h> // ETIMEDOUT

Frontend::Frontend( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  ConfigObject( adapter, "frontend", config_id )
  , ThreadBase( )
  , adapter(adapter)
  , fe(NULL)
  , adapter_id(adapter_id)
  , frontend_id(frontend_id)
  , present(false)
  , transponder(NULL)
  , current_port(0)
  , up(true)
{
  ports.push_back( new Port( *this, 0 ));
  Init( );
}

Frontend::Frontend( Adapter &adapter, std::string configfile ) :
  ConfigObject( adapter, configfile )
  , ThreadBase( )
  , adapter(adapter)
  , fe(NULL)
  , present(false)
  , transponder(NULL)
  , current_port(0)
  , up(true)
{
  Init( );
}

Frontend::~Frontend( )
{
  if( fe )
    fe->abort = 1;

  state = Shutdown;

  up = false;
  delete thread_idle;

  Close( );

  for( std::vector<Port *>::iterator it = ports.begin( ); it != ports.end( ); it++ )
  {
    delete *it;
  }
}

void Frontend::Init( )
{
  thread_idle = new Thread( *this, (ThreadFunc) &Frontend::Thread_idle );
  thread_idle->Run( );
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
      ::LogError( "%d.%d Delivery system %s not supported", adapter_id, frontend_id, delivery_system_name[delsys] );
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
    Log( "Closing /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
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
  this->adapter_id  = adapter_id;
  this->frontend_id = frontend_id;
  Log( "  Frontend on /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
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


void Frontend::Untune( )
{
  //if( transponder )
  //{
    //transponder->SetState( Transponder::State_Idle );
    //transponder = NULL;
  //}
  Close( );
}

bool Frontend::TunePID( Transponder &t, uint16_t service_id )
{
  bool ret = true;
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
    LogError( "No service with id %d found", service_id );
    return false;
  }

  if( !Tune( t ))
  {
    LogError( "Transponder tune failed" );
    return false;
  }

  Recorder rec( *fe );
  std::vector<int> fds;

  int videofd = -1;
  std::map<uint16_t, Stream *> &streams = s->GetStreams();
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ) || it->second->IsAudio( ))
    {
      Log( "Adding Stream %d: %s", it->first, it->second->GetTypeName( ));
      int fd = it->second->Open( *this );
      if( fd )
        fds.push_back( fd );
      if( it->second->IsVideo( ))
      {
        rec.AddTrack( );
        videofd = fd;
      }
    }
    else
      LogWarn( "Ignoring Stream %d: %s (%d)", it->first, it->second->GetTypeName( ), it->second->GetType( ));
  }

  if( fds.empty( ))
  {
    LogError( "no audio or video stream for service %d found", service_id );
    return false;
  }

  std::string dumpfile;
  std::string upcoming;

  int fd = OpenDemux( );
  Log( "Reading EIT" );
  struct dvb_table_eit *eit;
  dvb_read_section_with_id( fe, fd, DVB_TABLE_EIT, DVB_TABLE_EIT_PID, service_id, (uint8_t **) &eit, 5 );
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

  if( dumpfile.empty( ))
    dumpfile = "dump";
  else
  {
    std::replace( dumpfile.begin(), dumpfile.end(), '/', '_');
    std::replace( dumpfile.begin(), dumpfile.end(), '`', '_');
    std::replace( dumpfile.begin(), dumpfile.end(), '$', '_');
  }

  {
    char file[256];

    std::string dir = Utils::Expand( TVDaemon::Instance( )->GetDir( ));
    Utils::EnsureSlash( dir );
    std::string filename = dir + dumpfile + ".pes";

    int i = 0;
    while( Utils::IsFile( filename ))
    {
      char num[16];
      if( ++i == 1000 )
      {
        LogError( "Too many files with same name: %s", filename.c_str( ));
        ret = false;
        goto exit;
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
      LogError( "Cannot open file '%s'", filename.c_str( ));
      // FIXME: cleanup
      ret = false;
      goto exit;
    }

    Log( "Recording '%s' ...", filename.c_str( ));

    bool started = false;

    fd_set tmp_fdset;
    fd_set fdset;
    FD_ZERO( &fdset );
    int fdmax = 0;
    for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    {
      FD_SET ( *it, &fdset );
      if( *it > fdmax )
        fdmax = *it;
    }

    uint8_t *data = (uint8_t *) malloc( DMX_BUFSIZE );
    int len;
    up = true;
    int ac, vc;
    ac = vc = 0;
    uint64_t startpts = 0;
    while( up )
    {
      tmp_fdset = fdset;

      struct timeval timeout = { 1, 0 }; // 1 sec
      if( select( FD_SETSIZE, &tmp_fdset, NULL, NULL, &timeout ) == -1 )
      {
        LogError( "select error" );
        up = false;
        continue;
      }

      for( std::vector<int>::iterator it = fds.begin( ); up && it != fds.end( ); it++ )
      {
        int fd = *it;
        if( FD_ISSET( fd, &tmp_fdset ))
        {
          len = read( fd, data, DMX_BUFSIZE );
          if( len < 0 )
          {
            LogError( "Error receiving data... %d", errno );
            continue;
          }

          if( len == 0 )
          {
            Log( "no data received" );
            continue;
          }

          int ret = write( file_fd, data, len );

          if( fd == videofd )
          {
            //Log( "Video Packet: %d bytes", len );
            int packets = 0;
            ssize_t size = 0;
            ssize_t size2 = 0;
            uint8_t buf[188];
            int remaining = len;
            int pos = 0;
            uint8_t *p = data;
            while( up && remaining > 0 )
            {
              int chunk = 188;
              if( remaining < chunk ) chunk = remaining;
              remaining -= chunk;
              dvb_mpeg_ts_init( fe, p, chunk, buf, &size );
              if( size == 0 )
              {
                break;
              }
              p += size;
              chunk -= size;


              dvb_mpeg_ts *ts = (dvb_mpeg_ts *) buf;
              if( ts->payload_start )
                started = true;

              //if( started )
                //rec.record( p, chunk );

              p += chunk;
            }
          }
        }
      }
    }
    close( file_fd );
    free( data );
  }

exit:
  for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    dvb_dmx_close( *it );

  return ret;
}

int Frontend::OpenDemux( )
{
  return dvb_dmx_open( adapter_id, frontend_id );
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
    Close( );
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
    Close( );
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
    LogError( "Tuning failed" );
    Close( );
    return false;
  }

  t.SetState( Transponder::State_Tuned );
  t.SetSignal( signal, noise );
  transponder = &t;
  return true;
}

bool Frontend::Scan( Transponder &transponder )
{
  struct timeval tv;
  int ret;
  const char *err;
  int time = 5;
  std::vector<uint16_t> services;
  int fd_demux;
  struct dvb_table_pat *pat = NULL;

  if( !Tune( transponder ))
    return false;

  transponder.SetState( Transponder::State_Scanning );

  if(( fd_demux = OpenDemux( )) < 0 )
  {
    LogError( "unable to open adapter demux" );
    goto scan_failed;
  }

  Log( "Reading PAT" );
  dvb_read_section( fe, fd_demux, DVB_TABLE_PAT, DVB_TABLE_PAT_PID, (uint8_t **) &pat, time );
  if( !pat )
  {
    LogError( "Error reading PAT table" );
    goto scan_failed;
  }

  Log( "  Setting TSID %d", pat->header.id );
  transponder.SetTSID( pat->header.id );
  if( transponder.GetState( ) == Transponder::State_Duplicate )
    goto scan_aborted;

  if( transponder.HasVCT( ))
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

        transponder.UpdateService( channel->program_number, type, name, provider, channel->access_control );
        services.push_back( channel->program_number );
      }
    }
    if( vct )
      dvb_table_vct_free( vct );
  }

  if( transponder.HasSDT( ))
  {
    Log( "Reading SDT" );
    struct dvb_table_sdt *sdt;
    dvb_read_section( fe, fd_demux, DVB_TABLE_SDT, DVB_TABLE_SDT_PID, (uint8_t **) &sdt, time );
    if( sdt )
    {
      //dvb_table_sdt_print( fe, sdt );
      dvb_sdt_service_foreach( service, sdt )
      {
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
        transponder.UpdateService( service->service_id, type, name, provider, service->free_CA_mode );
        services.push_back( service->service_id );
      }

      dvb_table_sdt_free( sdt );
    }
  }

  //dvb_table_pat_print( fe, pat );
  Log( "Reading PMT's" );
  dvb_pat_program_foreach( program, pat )
  {
    for( std::vector<uint16_t>::iterator it = services.begin( ); it != services.end( ); it++ )
    {
      if( *it == program->service_id )
      {
        if( program->service_id == 0 )
        {
          LogWarn( "  Ignoring PMT of service 0" );
          break;
        }

        transponder.UpdateProgram( program->service_id, program->pid );

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
            transponder.UpdateStream( program->service_id, stream->elementary_pid, type );
        }

        dvb_table_pmt_free( pmt );
        break;
      }
    }
  }
  if( pat )
    dvb_table_pat_free( pat );

  if( transponder.HasNIT( ))
  {
    Log( "Reading NIT" );
    struct dvb_table_nit *nit;
    dvb_read_section( fe, fd_demux, DVB_TABLE_NIT, DVB_TABLE_NIT_PID, (uint8_t **) &nit, time );
    if( nit )
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
  Untune( );
  state = Closing;
  transponder.SetState( Transponder::State_Scanned );
  return true;

scan_failed:
  transponder.SetState( Transponder::State_ScanningFailed );
scan_aborted:
  dvb_dmx_close( fd_demux );
  Untune( );
  state = Closing;
  return false;
}


void Frontend::json( json_object *entry ) const
{
  char name[32];
  snprintf( name, sizeof( name ), "Frontend%d", GetKey( ));
  json_object_object_add( entry, "name", json_object_new_string( name ));
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

  request.NotFound( "RPC transponder: unknown action" );
  return false;
}

void Frontend::Thread_idle( )
{
  while( up )
  {
    for( std::vector<Port *>::const_iterator it = ports.begin( ); it != ports.end( ); it++ )
    {
      if( (*it)->Scan( ))
        continue;
    }

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

