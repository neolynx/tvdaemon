/*
 *  tvdaemon
 *
 *  Activity_Stream class
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

#include "Activity_Stream.h"

#include "Log.h"
#include "Channel.h"
#include "Stream.h"
#include "Service.h"
#include "Frontend.h"
#include "RingBuffer.h"
#include "Frame.h"

#include <libdvbv5/pat.h>
#include <libdvbv5/eit.h>
#include <libdvbv5/desc_event_short.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/dvb-demux.h>
#include <errno.h>
#include <sys/ioctl.h>

// libav
extern "C" {
#include <libavformat/avformat.h>
#include "libavcodec/avcodec.h"
}

#ifdef	CCXX_NAMESPACES
using namespace ost;
#endif

Activity_Stream::Activity_Stream( Channel &channel ) :
  Activity( ),
  channel(channel),
  session(NULL)
{
  SetChannel( &channel ); // FIXME: use channel_id
}

Activity_Stream::~Activity_Stream( )
{
  delete session;
}

std::string Activity_Stream::GetName( ) const
{
  return "Streaming " + channel.GetName( );
}

void Activity_Stream::AddStream( RTPSession *session )
{
  this->session = session;
  if( GetState( ) == State_New )
    Start( );
}

bool Activity_Stream::Perform( )
{
  // FIXME: verify all pointers...

  bool ret = true;
  std::map<int, Stream *> fds;

  int fd_pat = frontend->OpenDemux( );
  frontend->Log( "Opening PAT demux %d", fd_pat );
  // FIXME check fd
  struct dmx_sct_filter_params f;
  memset(&f, 0, sizeof(f));
  f.pid = DVB_TABLE_PAT_PID;
  f.filter.filter[0] = DVB_TABLE_PAT;
  f.filter.mask[0] = 0xff;
  f.timeout = 0;
  f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
  if( ioctl( fd_pat, DMX_SET_FILTER, &f) == -1 )
  {
    frontend->Log("dvb_read_section: ioctl DMX_SET_FILTER failed");
    return false;
  }

  int videofd = -1;
  std::map<uint16_t, Stream *> &streams = service->GetStreams( );
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ) || it->second->IsAudio( ))
    {
      frontend->Log( "Adding Stream %d: %s", it->first, it->second->GetTypeName( ));
      int fd = service->Open( *frontend, it->second->GetKey( ));
      if( fd )
        fds[fd] = it->second;
      if( it->second->IsVideo( ))
      {
        ////rec.AddTrack( );
        videofd = fd;
        if( !it->second->Init( session ))
        {
          // FIXME: free all stuff
          return false;
        }
      }
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


  frontend->Log( "Streaming ..." );

  int stage = 0;

  fd_set tmp_fdset;
  fd_set fdset;
  FD_ZERO( &fdset );
  int fdmax = fd_pat;
  FD_SET ( fd_pat, &fdset );
  for( std::map<int, Stream *>::iterator it = fds.begin( ); it != fds.end( ); it++ )
  {
    FD_SET ( it->first, &fdset );
    if( it->first > fdmax )
      fdmax = it->first;
  }

  //RingBuffer *buffer = new RingBuffer( 64 * 1024 );
  //Frame *frame = new Frame( *frontend->GetFE( ));

  uint8_t *data = (uint8_t *) malloc( DMX_BUFSIZE );
  int ac, vc;
  ac = vc = 0;
  uint64_t startpts = 0;
  time_t idle_since = 0;
  uint32 timestamp = 0;
  int len;
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

      struct dvb_table_pat *pat = NULL;
      ssize_t pat_len = 0;
      dvb_table_pat_init( frontend->GetFE( ), data, len, &pat );
      //dvb_table_pat_print( frontend->GetFE( ), pat );
      dvb_table_pat_free( pat );
    }

    for( std::map<int, Stream *>::iterator it = fds.begin( ); IsActive( ) && it != fds.end( ); it++ )
    {
      int fd = it->first;
      if( FD_ISSET( fd, &tmp_fdset ))
      {
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

        if( fd == videofd )
        {
          it->second->HandleData( data, len );
          //Utils::dump( data, len );

          //int remaining = len;
          //int pos = 0;
          //uint8_t *p = data;
          //uint8_t buf[188];
          //ssize_t size = 0;
          //while( IsActive( ) && remaining > 0 )
          //{
            //int chunk = 188;

            //if( remaining < chunk ) chunk = remaining;
            //remaining -= chunk;

            ////Utils::dump( p, chunk );
            //size = 0;
            //dvb_mpeg_ts_init( frontend->GetFE( ), p, chunk, buf, &size );
            //if( size == 0 )
            //{
              //LogError( "Error parsing TS" );
              //break;
            //}

            //p += size;
            //chunk -= size;

            //dvb_mpeg_ts *ts = (dvb_mpeg_ts *) buf;

            //if( ts->payload_start )
            //{
              //Log( "payload start" );
              //stage = 1;
            //}

            //if( stage < 1 )
            //{
              //p += chunk;
              //continue;
            //}

            //av_free_packet( pkt );

            //buffer->append( p, chunk );

            //uint8_t *buf2;
            //size_t buflen2;
            //if( buffer->GetFrame( buf2, buflen2 ))
            //{

              ////if( frame->ReadFrame( buf2, buflen2, session ))
              ////Log( "stream !" );

              ////Utils::dump( p, chunk );


              //if( timestamp == 0 )
              //{
                //timestamp = session->getCurrentTimestamp( );
                //session->putData( timestamp, (const unsigned char *) "test", 4 );
                //timestamp = 1;
              //}
              //else
                //timestamp += session->getCurrentRTPClockRate();
              ////session->putData( timestamp, buf2, buflen2 );
            //}

            //p += chunk;
          //}
        }

        //int ret = write( file_fd, data, len );

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
  free( data );

exit:
  frontend->CloseDemux( fd_pat );
  for( std::map<int, Stream *>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    frontend->CloseDemux( it->first );

  return ret;
}

