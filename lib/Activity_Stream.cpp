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
#include "CAMClient.h"

#include <libdvbv5/pat.h>
#include <libdvbv5/eit.h>
#include <libdvbv5/desc_event_short.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/sdt.h>
#include <libdvbv5/desc_service.h>
#include <libdvbv5/pat.h>
#include <libdvbv5/pmt.h>

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

Activity_Stream::Activity_Stream( Channel &channel, ost::RTPSession *session ) :
  Activity( ),
  channel(channel),
  session(session)
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

bool Activity_Stream::Perform( )
{
  // FIXME: verify all pointers...

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
  desc->name = strdup( channel.GetName( ).c_str( ));

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
  //if( ioctl( fd_pat, DMX_SET_FILTER, &f) == -1 )
  //{
    //frontend->Log("dvb_read_section: ioctl DMX_SET_FILTER failed");
    //return false;
  //}

  int videofd = -1;
  std::map<uint16_t, Stream *> &streams = service->GetStreams( );
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ) || it->second->IsAudio( ))
    {
      frontend->Log( "Adding Stream %d: %s", it->first, it->second->GetTypeName( ));
      int fd = service->Open( *frontend, it->second->GetKey( ));
      if( fd )
        fds.push_back( fd );
      else
        LogError( "Error opening demux" );

      struct dvb_table_pmt_stream *stream = dvb_table_pmt_stream_create( pmt, it->second->GetKey( ), it->second->GetTypeMPEG( ));

      if( it->second->IsVideo( ))
      {
        pmt->pcr_pid = it->second->GetKey( );
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

  {
    UpdateRTPTimestamp( );

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
    SendRTP( mpegts, size );
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
    SendRTP( mpegts, size );
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
    SendRTP( mpegts, size );
    free( mpegts );
  }

  int stage = 0;

  fd_set tmp_fdset;
  fd_set fdset;
  FD_ZERO( &fdset );
  int fdmax = 0;
  //FD_SET ( fd_pat, &fdset );
  for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
  {
    FD_SET ( *it, &fdset );
    if( *it > fdmax )
      fdmax = *it;
  }

  //RingBuffer *buffer = new RingBuffer( 64 * 1024 );
  //Frame *frame = new Frame( *frontend->GetFE( ));

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
      //dvb_table_pat_init( frontend->GetFE( ), data, len, &pat );
      ////dvb_table_pat_print( frontend->GetFE( ), pat );
      //dvb_table_pat_free( pat );
    //}


    uint8_t *data = (uint8_t *) malloc( DMX_BUFSIZE );
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


          p += chunk;
        }

        UpdateRTPTimestamp( );
        //SendRTP( data, len );
        p = data;
        while( len > 0 )
        {
          int chunk = 7 * 188;
          if( len < chunk ) chunk = len;
          len -= chunk;

          SendRTP( p, chunk );
          p += chunk;
        }

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
    free( data );
  }

exit:
  //frontend->CloseDemux( fd_pat );
  for( std::vector<int>::iterator it = fds.begin( ); it != fds.end( ); it++ )
    frontend->CloseDemux( *it );

  return ret;
}

void Activity_Stream::UpdateRTPTimestamp( )
{
  timestamp = session->getCurrentTimestamp( );
}

void Activity_Stream::SendRTP( const uint8_t *data, int length )
{
  session->putData( timestamp, data, length );
}
