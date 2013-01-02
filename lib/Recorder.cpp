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

#include "Matroska.h"
#include "Log.h"
#include "Utils.h"
#include "RingBuffer.h"
#include "Frame.h"

Recorder::Recorder( struct dvb_v5_fe_parms &fe ): fe(fe), mkv(NULL)
{
  mkv = new Matroska( "test" );
  mkv->WriteHeader( );

  buffer = new RingBuffer( 64 * 1024 );
  frame = new Frame( fe, *mkv );
}

Recorder::~Recorder( )
{
  delete mkv;
  delete buffer;
  delete frame;
}

void Recorder::AddTrack( )
{
  mkv->AddTrack( );
}

void Recorder::record( uint8_t *data, int size )
{
  //Utils::dump( data, size );
  buffer->append( data, size );

  uint8_t *p;
  size_t framelen;
  if( buffer->GetFrame( p, framelen ))
  {
    frame->ReadFrame( p, framelen );

    //Utils::dump( frame, framelen );
    //mkv->AddFrame( frame, framelen );
    delete[] p;
    //buffer->FreeFrame( frame );
  }
  return;
}

