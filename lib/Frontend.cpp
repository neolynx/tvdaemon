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

#include <sys/ioctl.h>
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
{
  pthread_mutex_init( &mutex, NULL );
  pthread_cond_init( &cond, NULL );
}

Frontend::~Frontend( )
{
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

bool Frontend::Tune( const Transponder &t, int timeoutms )
{
  if( !Open( ))
    return false;

  int setSysReturn = dvb_set_compat_delivery_system( fe, t.GetDelSys( ));
  if( setSysReturn != 0 )
  {
    LogError( "dvb_set_compat_delivery_system return %d", setSysReturn );
    return false;
  }
  t.GetParams( fe );

  dvb_fe_prt_parms( fe );

  int r = dvb_fe_set_parms( fe );
  if( r < 0 )
  {
    LogError( "dvb_fe_set_parms failed with %d.", r );
    return false;
  }

  uint32_t freq;
  dvb_fe_retrieve_parm( fe, DTV_FREQUENCY, &freq );
  Log( "tuning to %d", freq );

  if( !GetLockStatus( ))
    return false;

  this->transponder = const_cast<Transponder*>(&t);
  this->state = Tuned;
  return true;
}

void Frontend::Untune()
{
  if( this->state != Tuned)
    return;
  this->state = Opened;
  this->transponder = NULL;
}

bool Frontend::Scan( Transponder &transponder, int timeoutms )
{
  if( !CreateDemuxThread( ))
    return false;
  struct timespec abstime;
  int ret, count = 0;
  do
  {
    clock_gettime( CLOCK_REALTIME, &abstime );
    abstime.tv_sec += 1;
    pthread_mutex_lock( &mutex );
    ret = pthread_cond_timedwait( &cond, &mutex, &abstime );
    pthread_mutex_unlock( &mutex );
  } while( up & ret == ETIMEDOUT && ++count < 60 );
  LogError( "ret %d", ret );
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

bool Frontend::TunePID( Transponder &t, uint16_t pno )
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
  Service *s = t.GetService( pno );
  if( !s )
  {
    LogError( "No service for pno %d", pno);
    return false;
  }
  std::map<uint16_t, Stream *> streams = s->GetStreams();
  std::map<uint16_t, Stream *>::iterator iter = streams.begin();
  uint16_t streamid = 0;
  for( ; iter != streams.end(); iter++)
  {
    if (iter->second->GetType() == Stream::Video)
    {
      streamid = iter->first;
      break;
    }
  }
  if( streamid == 0)
  {
    LogError( "no video stream found for pno %d", pno );
    return false;
  }

  // tune transponder
  if( !Tune( t ))
  {
    LogError( "Transponder tune failed" );
    return false;
  }
  if( !GetLockStatus() )
  {
    LogError( "Unable to get lock for tune pno %d", pno );
    return false;
  }

  //
  // open the demux device
  //
  int fd_demux = dvb_dmx_open( adapter_id, frontend_id, 0 );
  if( fd_demux < 0 )
  {
    printf( "unable to open adapter demux for pat\n" );
    return false;
  }
  //dvbdemux_set_buffer( fd_demux, DVB_MAX_SECTION_BYTES );

  // set filter for pid
  if( 0 != set_pesfilter(fd_demux, streamid, DMX_PES_OTHER, 0 ))
  {
    LogWarn( "failed to set the pid filter for pno %d\n", pno);
  }
  else
  {
    // start streaming/recoding
    FILE *f = fopen("dump.pes", "w");

    uint8_t data[DVB_MAX_PAYLOAD_PACKET_SIZE];
    int bytes = read(fd_demux, data, sizeof(data));
    for (int count = 0; count < 1000; ++count)
    {
      bytes = read(fd_demux, data, sizeof(data));
      if (bytes > 0)
        fwrite(data, 1, bytes, f);
    }
    fflush(f);
    fclose(f);
  }

  //
  // closing demux
  //
  dvb_dmx_close( fd_demux );

  return true;
}

bool Frontend::GetLockStatus( int timeout )
{
  if( !fe )
    return false;
  uint32_t status;
  uint32_t snr = 0, _signal = 0;
  uint32_t ber = 0, uncorrected_blocks = 0;

  for( int i = 0; i < timeout; i++ )
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

      Log("status %02x | signal %3u%% | snr %3u%% | ber %d | unc %d", status, (_signal * 100) / 0xffff, (snr * 100) / 0xffff, ber, uncorrected_blocks);

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
  int fd_demux = dvb_dmx_open( adapter_id, frontend_id, 0 );
  if( fd_demux < 0 )
  {
    LogError( "unable to open adapter demux" );
    up = false;
    return;
  }

  Log( "Reading PAT" );

  uint32_t length;
  struct dvb_table_pat *pat;
  dvb_read_section( fd_demux, DVB_TABLE_PAT, DVB_TABLE_PAT_PID, (uint8_t **) &pat, &length, 1 );
  if( !pat )
  {
    LogError( "Error reading PAT table" );
    up = false;
    return;
  }

  dvb_table_pat_print( fe, pat );

  for( int i = 0; ( i < pat->programs ) && up; i++)
  {
    if( pat->program[i].program_id == 0 )
      continue;
    Log( "Reading PMT for pid %d", pat->program[i].pid );
    struct dvb_table_pmt *pmt;

    dvb_read_section(fd_demux, DVB_TABLE_PMT, pat->program[i].pid, (uint8_t **) &pmt, &length, 1 );

    if( !pmt )
    {
      Log( "No PMT received" );
      continue;
    }

    dvb_table_pmt_print( fe, pmt );
    free( pmt );
  }
  if( pat )
    free( pat );

  struct dvb_table_nit *nit;
  dvb_read_section( fd_demux, DVB_TABLE_NIT, DVB_TABLE_NIT_PID, (uint8_t **) &nit, &length, 5 );
  if( nit )
  {
    dvb_table_nit_print( fe, nit );
    free( nit );
  }

  struct dvb_table_sdt *sdt;
  dvb_read_section( fd_demux, DVB_TABLE_SDT, DVB_TABLE_SDT_PID, (uint8_t **) &sdt, &length, 5 );
  if( sdt )
  {
    dvb_table_sdt_print( fe, sdt );
    free( sdt );
  }

  dvb_dmx_close( fd_demux );
  up = false;
  return;
}

    //printf( "\n" );
    //uint8_t data[DVB_MAX_SECTION_BYTES];
    //struct section *section = NULL;
    //int bytes = read( fd_demux, data, sizeof(data) );
    //if( bytes < 0 )
    //{
    //printf( "read error\n" );
    //up = false;
    //continue;
    //}

    //if(( section = section_codec( data, bytes )) == NULL )
    //{
    //printf( "unable to extract section header\n" );
    //continue;
    //}

    //switch( section->table_id )
    //{
    //case stag_mpeg_program_association:       // PAT
    //if( !HandlePAT( section ))
    //continue;
    //break;

    //case stag_dvb_network_information_actual: // NIT
    //case stag_dvb_network_information_other:
    //if( !HandleNIT( section ))
    //continue;
    //break;

    //case stag_dvb_service_description_actual: // SDT
    //case stag_dvb_service_description_other:
    //if( !HandleSDT( section ))
    //continue;
    //break;

    //case stag_mpeg_program_map:               // PMT
    //if( !HandlePMT( section, curPid ))
    //continue;
    //break;

    //default:
    //printf( "unknown section received\n" );
    //break;
    //}
  //} // while( up )

  //printf( "Stopping demuxer\n" );
  //pthread_mutex_lock( &mutex );
  //pthread_cond_signal( &cond );
  //pthread_mutex_unlock( &mutex );

  //dvbdemux_stop( fd_demux );

//bool Frontend::HandlePAT( struct section *section )
//{
//struct section_ext *section_ext = NULL;
//if(( section_ext = section_ext_decode( section, 1 )) == NULL )
//{
//printf( "unable to extract section_ext of pat\n" );
//return false;
//}

//struct mpeg_pat_section *pat = mpeg_pat_section_codec( section_ext );
//if( NULL == pat )
//{
//printf( "mpeg_pat_section codec error\n" );
//return false;
//}
//struct mpeg_pat_program *cur = NULL;
//mpeg_pat_section_programs_for_each( pat, cur )
//{
//if (0 != cur->program_number)
//transponder->UpdateProgram( cur->program_number, cur->pid ); // FIXME: do not create service here, only in SDT with correct type
//pid_map[cur->program_number] = cur->pid;
//}

//std::map<uint16_t, uint16_t>::iterator iter = pid_map.find(0);
//if( iter != pid_map.end( ))
//{
//// switch to the NIT table
//filter[0] = stag_dvb_network_information_actual;
////if( dvbdemux_set_section_filter( fd_demux, TRANSPORT_NIT_PID, filter, mask, 1, 1 ))   //FIXME: NIT id or the id define in the pat?
//if( dvbdemux_set_section_filter( fd_demux, (*iter).second, filter, mask, 1, 1 ))
//{
//printf( "failed to set demux filter for nit\n" );
//up = false;
//return false;
//}
//pid_map.erase(iter);
//}
//else
//{
//// switch to the SDT table
//filter[0] = stag_dvb_service_description_actual;
//if( dvbdemux_set_section_filter( fd_demux, TRANSPORT_SDT_PID, filter, mask, 1, 1 ))
//{
//printf( "failed to set demux filter for sdt\n" );
//up = false;
//return false;
//}
//}
//return true;
//}

//bool Frontend::HandleSDT( struct section *section )
//{
//struct section_ext *section_ext = NULL;
//if(( section_ext = section_ext_decode( section, 1 )) == NULL )
//{
//printf( "unable to extract section_ext of sdt\n" );
//return false;
//}

//struct dvb_sdt_section *sdt = dvb_sdt_section_codec( section_ext );
//if( NULL == sdt )
//{
//printf( "sdt_section codec error\n" );
//return false;
//}
//struct dvb_sdt_service *service = NULL;
//dvb_sdt_section_services_for_each( sdt, service )
//{
//struct descriptor *descriptor = NULL;
//struct dvb_service_list_descriptor *dx = NULL;
//dvb_sdt_service_descriptors_for_each( service, descriptor )
//{
//if( descriptor->tag == dtag_dvb_service )
//{
//struct dvb_service_descriptor *dx          = dvb_service_descriptor_codec( descriptor );
//switch( dx->service_type )
//{
//case DVB_SERVICE_TYPE_DIGITAL_TV:
//case DVB_SERVICE_TYPE_DIGITAL_RADIO:
//case DVB_SERVICE_TYPE_TELETEXT:
//break;

//case DVB_SERVICE_TYPE_DATA_BCAST:
//default:
//continue;
//}

//struct dvb_service_descriptor_part2 *part2 = dvb_service_descriptor_part2( dx );
//char *tmpName     = new char[part2->service_name_length + 1];
//char *tmpProvider = new char[dx->service_provider_name_length + 1];
//memcpy( tmpName,     dvb_service_descriptor_service_name( part2 ), part2->service_name_length );
//memcpy( tmpProvider, dvb_service_descriptor_service_provider_name( dx ), dx->service_provider_name_length );
//tmpName[part2->service_name_length] = 0;
//tmpProvider[dx->service_provider_name_length] = 0;

////printf( "pno %d service '%s' provider '%s'\n", service->service_id, tmpName, tmpProvider );
//transponder->UpdateProgram( service->service_id, tmpName, tmpProvider );
////printf( "pno %d service %s provider %s\n", service->service_id, tmpName, tmpProvider );
//pno_list.push_back( service->service_id );

//delete [] tmpName;
//delete [] tmpProvider;
//}
//}
//}

//// switch to the PMT table
//filter[0] = stag_mpeg_program_map;
//if( 0 == pno_list.size( ))
//{
//up = false;
//return false;
//}
//curPid = pid_map[pno_list.front( )]; // FIXME: verify existing
//pno_list.pop_front();
//if( dvbdemux_set_section_filter( fd_demux, curPid, filter, mask, 1, 1 ) )
//{
//printf( "failed to set demux filter for pmt %d\n", curPid );
//up = false;
//return false;
//}
////printf( "filtering for PMT pid %d\n", curPid );
//return true;
//}



//bool Frontend::HandlePMT( struct section *section, uint16_t pid )
//{
//struct section_ext *section_ext = NULL;
////printf( "   found pmt for pid %d\n", pid );
//if(( section_ext = section_ext_decode( section, 1 )) == NULL )
//{
//printf( "unable to extract section_ext of pmt for %d\n", pid );
//return false;
//}

//struct mpeg_pmt_section *pmt = mpeg_pmt_section_codec( section_ext );
//if( NULL == pmt )
//{
//printf( "pmt_section codec error for %d\n", pid );
//return false;
//}
//struct mpeg_pmt_stream *cur_stream = NULL;
//mpeg_pmt_section_streams_for_each( pmt, cur_stream )
//{
//switch( cur_stream->stream_type )
//{
//case dtag_mpeg_video_stream:
//case dtag_mpeg_4_video:
////printf( "PMT ID %d: video stream with id %d found\n", pid, cur_stream->pid );
//transponder->UpdateStream( pid, cur_stream->pid, cur_stream->stream_type );
//break;
//case dtag_mpeg_audio_stream:
//case dtag_mpeg_4_audio:
////printf( "PMT ID %d: audio stream with id %d found\n", pid, cur_stream->pid );
//transponder->UpdateStream( pid, cur_stream->pid, cur_stream->stream_type );
//break;
//case dtag_mpeg_data_stream_alignment:
//case dtag_mpeg_private_data_indicator:
//{
//struct descriptor *curd = NULL;
//mpeg_pmt_stream_descriptors_for_each(cur_stream, curd)
//{
//switch( curd->tag )
//{
//case dtag_dvb_ac3:
////printf( "PMT ID %d: AC3 audio stream with id %d found\n", pid, cur_stream->pid );
//transponder->UpdateStream( pid, cur_stream->pid, curd->tag );
//break;
//case dtag_dvb_enhanced_ac3_descriptor:
////printf( "PMT ID %d: enhanced AC3 audio stream with id %d found\n", pid, cur_stream->pid );
//transponder->UpdateStream( pid, cur_stream->pid, curd->tag );
//break;
//case dtag_dvb_aac_descriptor:
////printf( "PMT ID %d: AAC audio stream with id %d found\n", pid, cur_stream->pid );
//transponder->UpdateStream( pid, cur_stream->pid, curd->tag );
//break;
//case dtag_dvb_dts_descriptor:
////printf( "PMT ID %d: DTS audio stream with id %d found\n", pid, cur_stream->pid );
//transponder->UpdateStream( pid, cur_stream->pid, curd->tag );
//break;
//}
//}
//}
//}
//}

//// switch to the next PMT
//filter[0] = stag_mpeg_program_map;
//if( 0 == pno_list.size( ))
//{
//up = false;
//return false;
//}
//curPid = pid_map[pno_list.front( )]; // FIXME: verify existing
//pno_list.pop_front();
//if( dvbdemux_set_section_filter( fd_demux, curPid, filter, mask, 1, 1 ))
//{
//printf( "failed to set demux filter for pmt %d\n", curPid );
//up = false;
//return false;
//}
////printf( "filtering for PMT pid %d\n", curPid );
//return true;
//}

