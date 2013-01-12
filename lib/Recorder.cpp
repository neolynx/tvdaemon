/*
 *  tvdaemon
 *
 *  DVB Recorder class
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

#include "Recorder.h"

//#include "Matroska.h"
#include "Log.h"
//#include "Utils.h"
#include "Channel.h"
#include "Service.h"
#include "Transponder.h"
#include "Source.h"
#include "Frontend.h"
#include "TVDaemon.h"
//#include "RingBuffer.h"
//#include "Frame.h"

#include "descriptors/eit.h"
#include "descriptors/desc_event_short.h"
#include "dvb-scan.h"
#include "descriptors/mpeg_ts.h"
#include "dvb-demux.h"
#include <fcntl.h>
#include <errno.h>
#include <algorithm> // replace

Recorder::Recorder( ) : up(true)
{
  rec_thread = new Thread( *this, (ThreadFunc) &Recorder::Rec_Thread );
  rec_thread->Run( );
}

Recorder::~Recorder( )
{
  up = false;
  delete rec_thread;
}

bool Recorder::RecordNow( Channel &channel )
{
  Lock( );
  Recording *rec = new Recording( channel );
  recordings.push_back( rec );
  rec->Schedule( );
  Unlock( );
  return true;
  return false;
}


void Recorder::Rec_Thread( )
{
  while( up )
  {
    Lock( );
    for( std::vector<Recording *>::const_iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    {
      if( (*it)->HasState( Recording::State_Start ))
      {
        Log( "starting recording" );
        if( !(*it)->Start( ))
          LogError( "Error starting recording" );
      }
    }
    Unlock( );
    sleep( 1 );
  }
}

Recording::Recording( Channel &channel ) : channel(channel), state(State_New), transponder(NULL), service(NULL), up(true)
{
  Log( "Recording created" );
}

Recording::~Recording( )
{
  up = false;
}

bool Recording::Start( )
{
  state = State_Started;
  bool ret = channel.Record( *this );
  if( !ret )
    state = State_Failed;
  return ret;
}

bool Recording::Record( Frontend &frontend )
{
  LogError( "Recording::Record" );

  bool ret = true;
  std::vector<int> fds;

  int videofd = -1;
  std::map<uint16_t, Stream *> &streams = service->GetStreams( );
  for( std::map<uint16_t, Stream *>::iterator it = streams.begin( ); it != streams.end( ); it++)
  {
    if( it->second->IsVideo( ) || it->second->IsAudio( ))
    {
      Log( "Adding Stream %d: %s", it->first, it->second->GetTypeName( ));
      int fd = it->second->Open( frontend );
      if( fd )
        fds.push_back( fd );
      if( it->second->IsVideo( ))
      {
        //rec.AddTrack( );
        videofd = fd;
      }
    }
    else
      LogWarn( "Ignoring Stream %d: %s (%d)", it->first, it->second->GetTypeName( ), it->second->GetType( ));
  }

  if( fds.empty( ))
  {
    LogError( "no audio or video stream for service %d found", service->GetKey( ));
    return false;
  }

  std::string dumpfile;
  std::string upcoming;

  int fd = frontend.OpenDemux( );
  Log( "Reading EIT" );
  struct dvb_table_eit *eit;
  dvb_read_section_with_id( frontend.GetFE( ), fd, DVB_TABLE_EIT, DVB_TABLE_EIT_PID, service->GetKey( ), (uint8_t **) &eit, 5 );
  if( eit )
  {
    dvb_table_eit_print( frontend.GetFE( ), eit );
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
              dvb_mpeg_ts_init( frontend.GetFE( ), p, chunk, buf, &size );
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

