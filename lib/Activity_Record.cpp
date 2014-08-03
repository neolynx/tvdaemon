/*
 *  tvdaemon
 *
 *  Activity_Record class
 *
 *  Copyright (C) 2013 André Roth
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

#include "Activity_Record.h"

#include "Log.h"
#include "Channel.h"
#include "Stream.h"
#include "Service.h"
#include "Frontend.h"
#include "TVDaemon.h" // GetDir .. use ref to recorder !
#include "Recorder.h"
#include "RPCObject.h"
#include "CAMClient.h"

#include <libdvbv5/pat.h>
#include <libdvbv5/eit.h>
#include <libdvbv5/desc_event_short.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/mpeg_pes.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/sdt.h>
#include <libdvbv5/desc_service.h>
#include <libdvbv5/pat.h>
#include <libdvbv5/pmt.h>

#include <fcntl.h>
#include <errno.h>
#include <algorithm> // replace
#include <string.h>
#include <sys/ioctl.h>

Activity_Record::Activity_Record( Recorder &recorder, Event &event, int config_id ) :
  Activity( ),
  ConfigObject( recorder, "recording", config_id ),
  recorder(recorder)
{
  SetChannel( &event.GetChannel( ));
  SetState( State_Scheduled );
  start = event.GetStart( );
  end   = event.GetEnd( );
  name  = event.GetName( );
  event_id = event.GetID( );
  duration = event.GetDuration( );
}

Activity_Record::Activity_Record( Recorder &recorder, Channel &channel, int config_id ) :
  Activity( ),
  ConfigObject( recorder, "recording", config_id ),
  recorder(recorder)
{
  SetChannel( &channel ); // FIXME: use channel_id
  SetState( State_Scheduled );
  start = time( NULL );
  end   = start + 130 * 60;
  name  = "Recording";
}

Activity_Record::Activity_Record( Recorder &recorder, std::string configfile ) :
  Activity( ),
  ConfigObject( recorder, configfile ),
  recorder(recorder)
{
}

Activity_Record::~Activity_Record( )
{
}

std::string Activity_Record::GetTitle( ) const
{
  std::string t = "Record";
  t += " - ";
  if( channel )
    t += channel->GetName( ) + " ";
  t += "\"" + name + "\"";
  return t;
}

std::string Activity_Record::GetName( ) const
{
  return name;
}

const std::string &Activity_Record::GetFilename( ) const
{
  return filename;
}

bool Activity_Record::SaveConfig( )
{
  WriteConfig( "Name",     name );
  WriteConfig( "Channel",  channel ? channel->GetKey( ) : -1 );
  WriteConfig( "Start",    start );
  WriteConfig( "End",      end );
  WriteConfig( "State",    GetState( ));
  WriteConfig( "Duration", duration );
  WriteConfig( "EventID",  event_id );
  WriteConfig( "Filename", filename );

  return WriteConfigFile( );
}

bool Activity_Record::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;
  int channel_id;
  ReadConfig( "Name",     name );
  ReadConfig( "Channel",  channel_id );
  ReadConfig( "Start",    start );
  ReadConfig( "End",      end );
  int state;
  ReadConfig( "State",    state );
  SetState( (Activity::State) state );
  ReadConfig( "Duration", duration );
  ReadConfig( "EventID",  event_id );
  ReadConfig( "Filename", filename );

  if( end > time( NULL ))
    SetState( State_Scheduled );
  else if( HasState( State_Scheduled ))
    SetState( State_Missed );

  if( channel_id != -1 )
  {
    channel = TVDaemon::Instance( )->GetChannel( channel_id );
    if( !channel )
    {
      LogError( "Activity_Record: unknown channel %d", channel_id );
      channel_id == -1;
    }
  }
  return true;
}

bool Activity_Record::Perform( )
{
  // FIXME: verify all pointers...

  std::string dir = recorder.GetDir( );
  dir = Utils::Expand( dir.c_str( ));
  size_t df;
  if( !Utils::DiskFree( dir, df ))
  {
    LogError( "error getting free disk space on %s, recording aborted", dir.c_str( ));
    return false;
  }
  if( df < GB( 20 ))
  {
    LogError( "Disk space less than 20G on %s, recording aborted", dir.c_str( ));
    return false;
  }

  bool ret = true;
  std::vector<int> fds;

  // encrypted
  uint16_t ecm_pid = 0;
  int ecm_fd = -1;
  CAMClient *client = NULL;
  if( service->IsScrambled( ))
  {
    frontend->Log( "Scrambled" );
    if( !service->GetECMPID( ecm_pid, &client ))
    {
      LogError( "CA id not found" );
      return false;
    }

    frontend->Log( "Opening ECM pid 0x%04x, cam client %p", ecm_pid, client );
    ecm_fd = service->Open( *frontend, ecm_pid );
    if( ecm_fd < 0 )
    {
      LogError( "Error opening demux" );
      return false;
    }
    fds.push_back( ecm_fd );
  }

  /* SDT */
  struct dvb_table_sdt *sdt = dvb_table_sdt_create( );
  sdt->header.id = 256;
  struct dvb_table_sdt_service *sdt_service = dvb_table_sdt_service_create( sdt, 0x0001 );

  struct dvb_desc_service *desc = (struct dvb_desc_service *) dvb_desc_create( frontend->GetFE( ), 0x48, &sdt_service->descriptor );
  if( !desc )
  {
    frontend->LogError( "cannot create descriptor" );
    return false;
  }
  desc->service_type = 0x1;
  desc->provider = strdup( "tvdaemon" );
  desc->name = strdup( name.c_str( ));

  /* PAT */
  struct dvb_table_pat *pat = dvb_table_pat_create( );
  pat->header.id = 256;
  struct dvb_table_pat_program *program = dvb_table_pat_program_create( pat, 0x1000, 0x0001 );

  /* PMT */
  struct dvb_table_pmt *pmt = dvb_table_pmt_create( 0x0100 );
  pmt->header.id = 256;



  //int fd_pat = frontend->OpenDemux( );
  //frontend->Log( "Opening PAT demux %d", fd_pat );
  //// FIXME check fd
  //struct dmx_sct_filter_params f;
  //memset(&f, 0, sizeof(f));
  //f.pid = DVB_TABLE_PAT_PID;
  //f.filter.filter[0] = DVB_TABLE_PAT;
  //f.filter.mask[0] = 0xff;
  //f.timeout = 0;
  //f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
  //if (ioctl(fd_pat, DMX_SET_FILTER, &f) == -1) {
    //frontend->Log("dvb_read_section: ioctl DMX_SET_FILTER failed");
    //return -1;
  //}

  std::map<uint16_t, Stream *> &streams = service->GetStreams( );
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ) || it->second->IsAudio( ))
    {
      frontend->Log( "Adding Stream %d: %s", it->first, it->second->GetTypeName( ));
      int fd = service->Open( *frontend, it->second->GetKey( ));
      if( fd > 0 )
        fds.push_back( fd );
      else
        LogError( "Error opening demux" );

      struct dvb_table_pmt_stream *stream = dvb_table_pmt_stream_create( pmt, it->second->GetKey( ), it->second->GetTypeMPEG( ));

      if( it->second->IsVideo( ))
      {
        pmt->pcr_pid = it->second->GetKey( );
      ////rec.AddTrack( );
      //videofd = fd;
      //
      }

    }
    else
      frontend->LogWarn( "Ignoring Stream %d: %s (%d)", it->first, it->second->GetTypeName( ), it->second->GetType( ));
  }

  if( fds.empty( ))
  {
    frontend->LogError( "no audio or video stream for service %d found", service->GetKey( ));
    // FIXME: close fd_pat, ...
    return false;
  }

  std::string upcoming;

  bool started = false;
  int fdmax = 0;
  uint8_t *data;
  int len;
  int ac, vc;
  uint64_t startpts = 0;
  time_t idle_since = 0;

  //int fd = frontend->OpenDemux( );
  //Log( "Reading EIT" );
  //struct dvb_table_eit *eit;
  //dvb_read_section_with_id( frontend->GetFE( ), fd, DVB_TABLE_EIT, DVB_TABLE_EIT_PID, service->GetKey( ), (uint8_t **) &eit, 5 );
  //if( eit )
  //{
  //dvb_eit_event_foreach(event, eit)
  //{
  //if( event->running_status == 4 ) // now
  //{
  //dvb_desc_find( struct dvb_desc_event_short, desc, event, short_event_descriptor )
  //{
  //dumpfile = desc->name;
  //break;
  //}
  //}
  //if( event->running_status == 2 ) // starts in a few seconds
  //{
  //dvb_desc_find( struct dvb_desc_event_short, desc, event, short_event_descriptor )
  //{
  //upcoming = desc->name;
  //break;
  //}
  //}
  //}
  //dvb_table_eit_free(eit);
  //}

  //if( !upcoming.empty( ))
  //{
  //dumpfile = upcoming;
  //}

  if( filename.empty( ))
  {
    std::string t = dir;
    Utils::EnsureSlash( t );
    std::string t2 = name;

    std::replace( t2.begin(), t2.end(), '/', '_');
    std::replace( t2.begin(), t2.end(), '`', '_');
    std::replace( t2.begin(), t2.end(), '$', '_');

    t += t2;

    char file[256];

    t2 = t + ".ts";

    int i = 0;
    while( Utils::IsFile( t2 )) // FIXME: race cond
    {
      char num[16];
      snprintf( num, sizeof( num ), " - %d", i++ );
      t2 = t + num + ".ts";
    }
    filename = t2;
  }

  int file_fd = open( filename.c_str( ),
#ifdef O_LARGEFILE
      O_LARGEFILE |
#endif
      O_WRONLY | O_CREAT | O_APPEND, 0664 );

  if( file_fd < 0 )
  {
    frontend->LogError( "Cannot open file '%s'", filename.c_str( ));
    // FIXME: cleanup
    ret = false;
    goto exit;
  }

  frontend->Log( "Recording '%s' ...", filename.c_str( ));

  {
    /* SDT */
    uint8_t *data;
    ssize_t size = dvb_table_sdt_store( frontend->GetFE( ), sdt, &data );
    dvb_table_sdt_free( sdt );
    if( !data )
    {
      frontend->LogError( "cannot store" );
      return -2;
    }

    uint8_t *mpegts;
    size = dvb_mpeg_ts_create( frontend->GetFE( ), data, size, &mpegts, DVB_TABLE_SDT_PID, 0 );
    free( data );
    int r = write( file_fd, mpegts, size );
    free( mpegts );

    /* PAT */
    size = dvb_table_pat_store( frontend->GetFE( ), pat, &data );
    dvb_table_pat_free( pat );
    if( !data )
    {
      frontend->LogError( "cannot store" );
      return -2;
    }

    size = dvb_mpeg_ts_create( frontend->GetFE( ), data, size, &mpegts, DVB_TABLE_PAT_PID, 0 );
    free( data );
    r = write( file_fd, mpegts, size );
    free( mpegts );

    /* PMT */
    size = dvb_table_pmt_store( frontend->GetFE( ), pmt, &data );
    dvb_table_pmt_free( pmt );
    if( !data )
    {
      frontend->LogError( "cannot store" );
      return -2;
    }

    size = dvb_mpeg_ts_create( frontend->GetFE( ), data, size, &mpegts, 0x1000, 0 );
    free( data );
    r = write( file_fd, mpegts, size );
    free( mpegts );
  }


  fd_set tmp_fdset;
  fd_set fdset;
  FD_ZERO( &fdset );
  //FD_SET ( fd_pat, &fdset );
  for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
  {
    FD_SET ( *it, &fdset );
    if( *it > fdmax )
      fdmax = *it;
  }

  data = (uint8_t *) malloc( DMX_BUFSIZE );
  ac = vc = 0;
  while( IsActive( ))
  {
    tmp_fdset = fdset;
    bool idle = true;

    struct timeval timeout = { 1, 0 }; // 1 sec
    if( select( FD_SETSIZE, &tmp_fdset, NULL, NULL, &timeout ) == -1 )
    {
      frontend->LogError( "select error" );
      ret = false;
      break;
    }

    if( !IsActive( ))
      break;

    //if( FD_ISSET( fd_pat, &tmp_fdset ))
    //{
      //len = read( fd_pat, data, DMX_BUFSIZE );
      //if( len < 0 )
      //{
        //frontend->LogError( "Error receiving data... %d", errno );
        //continue;
      //}

      //if( len == 0 )
      //{
        //frontend->Log( "no data received" );
        //continue;
      //}

      //struct dvb_table_pat *pat = NULL;
      //ssize_t pat_len = 0;
      //dvb_table_pat_init ( frontend->GetFE( ), data, len, &pat );
      ////dvb_table_pat_print( frontend->GetFE( ), pat );
      //dvb_table_pat_free( pat );
    //}

    for( std::vector<int>::iterator it = fds.begin( ); IsActive( ) && it != fds.end( ); it++ )
    {
      int fd = *it;
      if( FD_ISSET( fd, &tmp_fdset ))
      {
        if( ecm_fd != -1 && fd == ecm_fd )
        {
          len = read( fd, data, DMX_BUFSIZE );
          if( len < 0 )
          {
            frontend->LogError( "Error receiving data... %d", errno );
            continue;
          }

          if( len == 0 )
          {
            frontend->Log( "no data received" );
            continue;
          }

          int remaining = len;
          uint8_t *p = data;
          while( IsActive( ) && remaining > 0 )
          {
            int chunk = 188;

            if( remaining < chunk ) chunk = remaining;
            remaining -= chunk;

            if( client )
              client->HandleECM( p, chunk );

            p += chunk;
          }

          continue;
        }

        idle = false;
        idle_since = 0;
        len = read( fd, data, DMX_BUFSIZE );
        if( len < 0 )
        {
          frontend->LogError( "Error receiving data... %d", errno );
          continue;
        }

        if( len == 0 )
        {
          frontend->Log( "no data received" );
          continue;
        }

        int remaining = len;
        uint8_t *p = data;
        while( IsActive( ) && remaining > 0 )
        {
          int chunk = 188;
          if( remaining < chunk ) chunk = remaining;
          remaining -= chunk;

          if( client )
            client->Decrypt( p, chunk );

          int ret = write( file_fd, p, chunk );
          if( ret != chunk )
          {
            LogError( "Error writing to %s", filename.c_str( ));
            Stop( );
            break;
          }

          p += chunk;
        }


        //if( fd == videofd )
        //{
        ////Log( "Video Packet: %d bytes", len );
        //int packets = 0;
        //ssize_t size = 0;
        //ssize_t size2 = 0;
        //uint8_t buf[188];
        //int remaining = len;
        //int pos = 0;
        //uint8_t *p = data;
        //while( IsActive( ) && remaining > 0 )
        //{
        //int chunk = 188;
        //if( remaining < chunk ) chunk = remaining;
        //remaining -= chunk;
        //dvb_mpeg_ts_init( frontend->GetFE( ), p, chunk, buf, &size );
        //if( size == 0 )
        //{
        //break;
        //}
        //p += size;
        //chunk -= size;


        //dvb_mpeg_ts *ts = (dvb_mpeg_ts *) buf;
        //if( ts->payload_start )
        //started = true;

        ////if( started )
        ////rec.record( p, chunk );

        //p += chunk;
        //}
        //}
      }
    }

    if( idle )
    {
      if( idle_since == 0 )
        idle_since = time( NULL );
      else if( difftime( time( NULL ), idle_since ) > 10.0 )
      {
        frontend->LogError( "No data received" );
        ret = false;
        break;
      }
    }
  }
  close( file_fd );
  free( data );

exit:
  //frontend->CloseDemux( fd_pat );
  for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    frontend->CloseDemux( *it );

  return ret;
}

void Activity_Record::json( json_object *j ) const
{
  json_object_object_add( j, "id",      json_object_new_int( GetKey( )));
  json_object_object_add( j, "name",    json_object_new_string( name.c_str( )));
  json_object_object_add( j, "channel_id", json_object_new_int( channel ? channel->GetKey( ) : -1 ));
  json_object_time_add  ( j, "start",   start );
  json_object_object_add( j, "end",     json_object_new_int( end ));
  json_object_object_add( j, "state",   json_object_new_int( GetState( )));
  if( channel )
    json_object_object_add( j, "channel", json_object_new_string( channel->GetName( ).c_str( )));
}

bool Activity_Record::compare( const JSONObject &other, const int &p ) const
{
  const Activity_Record &b = (const Activity_Record &) other;
  double delta = difftime( start, b.start );
  if( delta < -0.9 || delta > 0.9 )
    return delta > 0.0;
  if( channel && b.channel )
    return channel->compare((const JSONObject &) *b.channel, p );
  return false;
}

