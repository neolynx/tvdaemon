/*
 *  tvdaemon
 *
 *  MPEGTS class
 *
 *  Copyright (C) 2014 André Roth
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

#include "MPEGTS.h"

#include "Log.h"
#include "Frame.h"

#include <fcntl.h>
#include <math.h>

MPEGTS::MPEGTS( std::string filename ) : filename(filename), fd(-1), seq(0)
{
}

MPEGTS::~MPEGTS( )
{
}

bool MPEGTS::Open( )
{
  fd = open( filename.c_str( ), O_RDONLY );
  if( fd < 0 )
  {
    LogError( "Error: cannot open '%s'", filename.c_str( ));
    return false;
  }
  return true;
}


Frame *MPEGTS::ReadFrame( )
{
  if( fd < 0 )
    return NULL;

  Frame *p = new Frame( );
  p->SetSequence( seq++ );

  int len = read( fd, p->GetBuffer( ), DVB_MPEG_TS_PACKET_SIZE );
  if( len != DVB_MPEG_TS_PACKET_SIZE )
  {
    LogWarn( "Streaming: error reading 188 bytes" );
    delete p;
    return NULL;
  }
  return p;
}

double MPEGTS::GetDuration( )
{
  if( fd < 0 )
    return NAN;

  double duration = NAN;
  off_t curpos = lseek( fd, 0, SEEK_CUR );

  bool got_first = false;
  bool got_last  = false;
  uint64_t first_timestamp, last_timestamp;
  off_t end;

  lseek( fd, 0, SEEK_SET );
  for( int i = 0; i < 200; i++ )
  {
    Frame *f = ReadFrame( );
    if( !f )
    {
      LogError( "Could not read first timestamp" );
      goto exit;
    }

    if( f->ParseHeaders( &first_timestamp, got_first, NULL ) and got_first )
      break;
  }

  if( !got_first )
  {
    LogError( "could not find first timestamp" );
    goto exit;
  }

  end = lseek( fd, 0, SEEK_END );
  if( end < DVB_MPEG_TS_PACKET_SIZE )
  {
    LogError( "not enough packets" );
    goto exit;
  }

  end -= DVB_MPEG_TS_PACKET_SIZE;
  end /= DVB_MPEG_TS_PACKET_SIZE;
  end *= DVB_MPEG_TS_PACKET_SIZE;

  for( int i = 0; i < 2000; i++ )
  {
    lseek( fd, end, SEEK_SET );
    end -= DVB_MPEG_TS_PACKET_SIZE;
    if( end < 0 )
      goto exit;

    Frame *f = ReadFrame( );
    if( !f )
    {
      LogError( "Could not read last timestamp" );
      goto exit;
    }

    if( f->ParseHeaders( &last_timestamp, got_last, NULL ) and got_last )
      break;
  }

  if( got_last )
  {
    duration = last_timestamp - first_timestamp;
    duration /= 90000.0;
    if( duration < 0.0 )
    {
      LogError( "invalid duration %fs", duration );
      duration = NAN;
    }
  }

exit:
  lseek( fd, curpos, SEEK_SET );
  return duration;
}


