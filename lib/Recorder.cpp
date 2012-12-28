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

Recorder::Recorder( ): mkv(NULL)
{
  mkv = new Matroska( "test" );
  mkv->WriteHeader( );
  readpos = writepos = 0;
}

Recorder::~Recorder( )
{
  delete mkv;
}

void Recorder::AddTrack( )
{
  mkv->AddTrack( );
}

void Recorder::AddCluster( uint64_t ts )
{
  mkv->AddCluster( ts );
}

void Recorder::record( uint8_t *data, int size )
{
  mkv->AddFrame( data, size );
  for( int i = 0; i < size - 4; i++ )
  {
    if(( *(uint32_t *)( data + i ) & 0xFFFFFF ) == 0x010000 )
    {
      uint8_t *type = data + i + 3;
      //Log( "packet: %02x", *type );
      //Utils::dump( data, size );
    }
  }
}

