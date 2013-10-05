/*
 *  tvdaemon
 *
 *  DVB Frame class
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

#include "Frame.h"

#include "Log.h"
//#include "Matroska.h"

#include <stdlib.h> // malloc
#include <string.h> // memcpy

#ifdef	CCXX_NAMESPACES
using namespace ost;
#endif

Frame::Frame( struct dvb_v5_fe_parms &fe ) : fe(fe)//, mkv(mkv)
{
  buffer_size = 1024;
  buffer = (uint8_t *) malloc( buffer_size );
  buffer_length = 0;

  started = false;
  slices = false;
  pts_start = 0;
  pts = 0;
  frame_type = DVB_MPEG_ES_FRAME_UNKNOWN;

  timestamp = 0;
}

Frame::~Frame( )
{
  free( buffer );
}

bool Frame::ReadFrame( uint8_t *data, size_t length, RTPSession *session )
{
  bool got_slice = false;
  bool payload = false;
  last_pts = pts;
  last_frame_type = frame_type;
  uint8_t type = data[3];
  switch( type )
  {
    case DVB_MPEG_ES_PIC_START:
      if( dvb_mpeg_es_pic_start_init( data, length, &pic_start ) == 0 )
      {
        //dvb_mpeg_es_pic_start_print( &fe, &pic_start );
        frame_type = (dvb_mpeg_es_frame_t) pic_start.coding_type;
      }
      payload = true;
      break;
    case DVB_MPEG_ES_USER_DATA:
      break;
    case DVB_MPEG_ES_SEQ_START:
      if( dvb_mpeg_es_seq_start_init( data, length, &seq_start ) == 0 )
        dvb_mpeg_es_seq_start_print( &fe, &seq_start );
      started = true;
      payload = true;
      break;
    case DVB_MPEG_ES_SEQ_EXT:
      payload = true;
      break;
    case DVB_MPEG_ES_GOP:
      payload = true;
      break;
    case DVB_MPEG_ES_SLICES:
      got_slice = true;
      payload = true;
      break;
    case DVB_MPEG_PES_VIDEO:
      {
        uint8_t buf2[184];
        ssize_t size2 = 0;
        dvb_mpeg_pes_init( &fe, data, length, buf2, &size2 );
        dvb_mpeg_pes *pes = (dvb_mpeg_pes *) buf2;
        //dvb_mpeg_pes_print( &fe, pes );
        if( pes->optional->PTS_DTS & 0x02 )
        {
          //Log( "pts: %ld", pes->optional->pts );
          if( pts_start == 0 )
            pts_start = pes->optional->pts;
          pts = (pes->optional->pts - pts_start) / 90;
          //Log( "pts set to %ld", pts );
        }
      }
      break;
    default:
      LogWarn( "MPEG ES unknown packet: %02x", type );
      break;
  }

  if( got_slice && !slices )
    slices = true;
  else if( !got_slice && slices )
  {
    slices = false;
    if( started )
    {
      // mkv.AddFrame( last_pts, last_frame_type, buffer, buffer_length );
      Log( "RTP: Sending %d bytes @", buffer_length );
      if( timestamp == 0 )
        timestamp = session->getCurrentTimestamp( );
      else
        timestamp += session->getCurrentRTPClockRate();
      session->putData( timestamp, buffer, buffer_length );

      buffer_length = 0;
    }
  }

  if( started && payload )
  {
    if( buffer_length + length > buffer_size )
    {
      buffer_size = buffer_length + length;
      buffer = (uint8_t *) realloc( buffer, buffer_size );
    }
    memcpy( buffer + buffer_length, data, length );
    buffer_length += length;
  }

}

