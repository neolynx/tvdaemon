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

#include <libdvbv5/dvb-fe.h>
#include <libdvbv5/crc32.h>
#include <libdvbv5/mpeg_pes.h>

Frame::Frame( ) : pid(0), ts(0), seq(0)
{
}

Frame::~Frame( )
{
}

void Frame::SetSequence( uint64_t seq )
{
  this->seq = seq;
}

uint64_t Frame::GetSequence( ) const
{
  return seq;
}

void Frame::SetTimestamp( uint64_t ts )
{
  this->ts = ts;
}

uint64_t Frame::GetTimestamp( ) const
{
  return ts;
}

uint8_t *Frame::GetBuffer( )
{
  return data;
}

uint16_t Frame::GetPID( ) const
{
  return pid;
}

bool Frame::ParseHeaders( uint64_t *timestamp, bool &got_timestamp, struct dvb_mpeg_ts **table )
{
  ssize_t size;
  int pos = 0;
  struct dvb_v5_fe_parms *fe = dvb_fe_dummy();
  struct dvb_mpeg_pes *pes = NULL;
  struct dvb_mpeg_ts *ts = NULL;

  got_timestamp = false;

  // TS
  size = dvb_mpeg_ts_init( fe, data + pos, DVB_MPEG_TS_PACKET_SIZE - pos, &ts );
  if( size < 0 )
    return false;

  if( pos + size > DVB_MPEG_TS_PACKET_SIZE)
  {
    LogError( "buffer overrun" );
    goto errorexit;
  }
  pos += size;

  pid = ts->pid;

  if( !ts->payload_start )
    goto errorexit;

  if( !(( ts->pid >= 0x0020 and ts->pid <= 0x1FFA ) or
        ( ts->pid >= 0x1FFC and ts->pid <= 0x1FFE )))
    goto errorexit;

  if(( *((uint32_t *)( data + pos )) & 0x00FFFFFF ) != 0x00010000 ) // PES marker
    goto errorexit;

  size = dvb_mpeg_pes_init( fe, data + pos, sizeof( data ) - pos, &pes );
  if( size < 0 )
  {
    LogError( "cannot parse pes packet" );
    goto errorexit;
  }

  switch( pes->stream_id )
  {
    case DVB_MPEG_STREAM_PADDING:
    case DVB_MPEG_STREAM_MAP:
    case DVB_MPEG_STREAM_PRIVATE_2:
    case DVB_MPEG_STREAM_ECM:
    case DVB_MPEG_STREAM_EMM:
    case DVB_MPEG_STREAM_DIRECTORY:
    case DVB_MPEG_STREAM_DSMCC:
    case DVB_MPEG_STREAM_H222E:
      goto errorexit;
    default:
      if( pes->optional->PTS_DTS & 1 )
      {
        *timestamp = pes->optional->dts;
        got_timestamp = true;
        //Log( "%d: Got DTS: %ld", ts->pid, pes->optional->dts );
      }
      else if( pes->optional->PTS_DTS & 2 )
      {
        *timestamp = pes->optional->pts;
        got_timestamp = true;
        //Log( "%d: Got PTS: %ld", ts->pid, pes->optional->pts );
      }
      break;
  }

  if( !got_timestamp )
    goto errorexit;

  this->ts = *timestamp;

  //{
  //struct timespec now;
  //clock_gettime( CLOCK_REALTIME, &now );
  //double seconds = *timestamp - bigbang;
  //double t = now.tv_sec + now.tv_nsec / 1000000000.0 - start_time.tv_sec + start_time.tv_nsec / 1000000000.0;
  //Log( "%04x: now %fs, play %fs, diff %fs", ts->pid, t, seconds, t - seconds );
  //}

  if( table )
    *table = ts;

  return true;

errorexit:
  if( pes )
    dvb_mpeg_pes_free( pes );
  if( ts )
    dvb_mpeg_ts_free( ts );

  return false;
}

bool Frame::Comp::operator( )( Frame *a, Frame *b )
{
  //if( a->ts == b->ts )
    return a->seq < b->seq;
  //return a->ts < b->ts;
}

