/*
 *  tvdaemon
 *
 *  DVB RingBuffer class
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

#include "RingBuffer.h"

#include "Log.h"

#include <string.h> // memcpy

RingBuffer::RingBuffer( size_t size, size_t overlap ): size(size), overlap(overlap)
{
  buffer = new uint8_t[size + overlap];
  count = readpos = writepos = 0;
}

RingBuffer::~RingBuffer( )
{
  delete[] buffer;
}

bool RingBuffer::append( uint8_t *data, size_t length )
{
  if( writepos + length > size )
  {
    int chunk = size - writepos;
    memcpy( buffer + writepos, data, chunk + ( length - chunk > overlap ? overlap : length - chunk ));
    memcpy( buffer, data + chunk, length - chunk );
    writepos = length - chunk;
  }
  else
  {
    memcpy( buffer + writepos, data, length );
    if( writepos < overlap )
      memcpy( buffer + length + writepos, data, overlap - writepos );
    writepos += length;
  }
  count += length;
  if( count > size )
  {
    LogWarn( "RingBuffer: buffer overflow: %d > %d", count, size );
    readpos += count - size;
    if( readpos >= size) readpos -= size;
    count = size;
    return false;
  }
  return true;
}

bool RingBuffer::read( uint8_t *data, size_t length )
{
  if( length > count )
    return false;
  if( length > size - readpos )
  {
    int chunk = size - readpos;
    memcpy( data, buffer + readpos, chunk );
    memcpy( data + chunk, buffer, length - chunk );
    readpos = length - chunk;
  }
  else
  {
    memcpy( data, buffer + readpos, length );
    readpos += length;
  }
  count -= length;
  return true;
}

bool RingBuffer::GetFrame( uint8_t * &data, size_t &length )
{
  if( count < 5 )
    return false;
  size_t j = readpos + 1;
  for( size_t i = 1; i < count; i++ )
  {
    if( j == size) j = 0;
    if(( *(uint32_t *)( buffer + j ) & 0xFFFFFF ) == 0x010000 )
    {
      length = i;
      data = new uint8_t[length];
      if( length > size - readpos )
      {
        size_t remaining = size - readpos;
        memcpy( data, buffer + readpos, remaining );
        memcpy( data + remaining, buffer, length - remaining );
      }
      else
        memcpy( data, buffer + readpos, length );
      readpos += length;
      if( readpos >= size) readpos -= size;
      count -= length;
      return true;
    }
    j++;
  }
  return false;
}

