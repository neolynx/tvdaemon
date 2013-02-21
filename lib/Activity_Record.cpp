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

#include "descriptors/pat.h"
#include "descriptors/eit.h"
#include "descriptors/desc_event_short.h"
#include "dvb-scan.h"
#include "descriptors/mpeg_ts.h"
#include "dvb-demux.h"
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
  state = State_Scheduled;
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
  state = State_Scheduled;
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

std::string Activity_Record::GetName( ) const
{
  std::string t = "Record";
  t += " - " + channel->GetName( ) + " \"" + name + "\"";
  return t;
}

bool Activity_Record::SaveConfig( )
{
  WriteConfig( "Name",     name );
  WriteConfig( "Channel",  channel->GetKey( ));
  WriteConfig( "Start",    start );
  WriteConfig( "End",      end );
  WriteConfig( "State",    state );
  WriteConfig( "Duration", duration );
  WriteConfig( "EventID",  event_id );

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
  ReadConfig( "State",    (int &) state );
  ReadConfig( "Duration", duration );
  ReadConfig( "EventID",  event_id );

  if( end > time( NULL ))
    state = State_Scheduled;

  if( channel_id != -1 )
  {
    channel = TVDaemon::Instance( )->GetChannel( channel_id );
    if( !channel )
      LogError( "Activity_Record: unknown channel" );
  }
  else
    LogError( "Activity_Record: unknown channel" );
  return true;
}

bool Activity_Record::Perform( )
{
  // FIXME: verify all pointers...

  bool ret = true;
  std::vector<int> fds;

  int fd_pat = frontend->OpenDemux( );
  // FIXME check fd
  struct dmx_sct_filter_params f;
  memset(&f, 0, sizeof(f));
  f.pid = DVB_TABLE_PAT_PID;
  f.filter.filter[0] = DVB_TABLE_PAT;
  f.filter.mask[0] = 0xff;
  f.timeout = 0;
  f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
  if (ioctl(fd_pat, DMX_SET_FILTER, &f) == -1) {
    frontend->Log("dvb_read_section: ioctl DMX_SET_FILTER failed");
    return -1;
  }

  std::map<uint16_t, Stream *> &streams = service->GetStreams( );
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ) || it->second->IsAudio( ))
    {
      frontend->Log( "Adding Stream %d: %s", it->first, it->second->GetTypeName( ));
      int fd = it->second->Open( *frontend );
      if( fd )
        fds.push_back( fd );
      //if( it->second->IsVideo( ))
      //{
      ////rec.AddTrack( );
      //videofd = fd;
      //}
    }
    else
      frontend->LogWarn( "Ignoring Stream %d: %s (%d)", it->first, it->second->GetTypeName( ), it->second->GetType( ));
  }

  if( fds.empty( ))
  {
    frontend->LogError( "no audio or video stream for service %d found", service->GetKey( ));
    return false;
  }

  std::string dumpfile;
  std::string upcoming;

  int fd = frontend->OpenDemux( );
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

  if( dumpfile.empty( ))
    dumpfile = name;

  std::replace( dumpfile.begin(), dumpfile.end(), '/', '_');
  std::replace( dumpfile.begin(), dumpfile.end(), '`', '_');
  std::replace( dumpfile.begin(), dumpfile.end(), '$', '_');

  {
    char file[256];

    std::string dir = recorder.GetDir( );
    Utils::EnsureSlash( dir );
    std::string filename = dir + dumpfile + ".pes";

    int i = 0;
    while( Utils::IsFile( filename ))
    {
      char num[16];
      if( ++i == 65535 )
      {
        frontend->LogError( "Too many files with same name: %s", filename.c_str( ));
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
      frontend->LogError( "Cannot open file '%s'", filename.c_str( ));
      // FIXME: cleanup
      ret = false;
      goto exit;
    }

    frontend->Log( "Recording '%s' ...", filename.c_str( ));

    bool started = false;

    fd_set tmp_fdset;
    fd_set fdset;
    FD_ZERO( &fdset );
    int fdmax = fd_pat;
    FD_SET ( fd_pat, &fdset );
    for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    {
      FD_SET ( *it, &fdset );
      if( *it > fdmax )
        fdmax = *it;
    }

    uint8_t *data = (uint8_t *) malloc( DMX_BUFSIZE );
    int len;
    int ac, vc;
    ac = vc = 0;
    uint64_t startpts = 0;
    while( IsActive( ))
    {
      tmp_fdset = fdset;

      struct timeval timeout = { 1, 0 }; // 1 sec
      if( select( FD_SETSIZE, &tmp_fdset, NULL, NULL, &timeout ) == -1 )
      {
        frontend->LogError( "select error" );
        ret = false;
        break;
      }

      if( FD_ISSET( fd_pat, &tmp_fdset ))
      {
        len = read( fd_pat, data, DMX_BUFSIZE );
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

        struct dvb_table_pat *pat = (struct dvb_table_pat *) malloc( MAX_TABLE_SIZE );
        ssize_t pat_len = 0;
        dvb_table_pat_init ( frontend->GetFE( ), data, len, (uint8_t *) pat, &pat_len );
        //dvb_table_pat_print( frontend->GetFE( ), pat );
        dvb_table_pat_free( pat );
      }

      for( std::vector<int>::iterator it = fds.begin( ); IsActive( ) && it != fds.end( ); it++ )
      {
        int fd = *it;
        if( FD_ISSET( fd, &tmp_fdset ))
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

          int ret = write( file_fd, data, len );

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
    }
    close( file_fd );
    free( data );
  }

exit:
  dvb_dmx_close( fd_pat );
  for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    dvb_dmx_close( *it );

  return ret;
}

void Activity_Record::json( json_object *j ) const
{
  Log( "Activity_Record::json" );
  json_object_object_add( j, "name",   json_object_new_string( name.c_str( )));
  json_object_object_add( j, "channel",json_object_new_int( channel->GetKey( )));
  json_object_time_add  ( j, "start",  start );
  json_object_object_add( j, "end",    json_object_new_int( end ));
  json_object_object_add( j, "state",  json_object_new_int( state ));
}

bool Activity_Record::SortByStart( const Activity_Record *a, const Activity_Record *b )
{
  return difftime( a->start, b->start ) < 0.0;
}

//Recorder::Recorder( struct dvb_v5_fe_parms &fe ): fe(fe), mkv(NULL)
//{
//mkv = new Matroska( "test" );
//mkv->WriteHeader( );

//buffer = new RingBuffer( 64 * 1024 );
//frame = new Frame( fe, *mkv );
//}

//Recorder::~Recorder( )
//{
//delete mkv;
//delete buffer;
//delete frame;
//}

//void Recorder::AddTrack( )
//{
//mkv->AddTrack( );
//}

//void Recorder::record( uint8_t *data, int size )
//{
////Utils::dump( data, size );
//buffer->append( data, size );

//uint8_t *p;
//size_t framelen;
//if( buffer->GetFrame( p, framelen ))
//{
//frame->ReadFrame( p, framelen );

////Utils::dump( frame, framelen );
////mkv->AddFrame( frame, framelen );
//delete[] p;
////buffer->FreeFrame( frame );
//}
//return;
//}
