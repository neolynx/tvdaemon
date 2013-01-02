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

#ifndef _RingBuffer_
#define _RingBuffer_

#include <unistd.h> // size_t
#include <stdint.h> // uint8_t

class RingBuffer
{
  public:
    RingBuffer( size_t size, size_t overlap = 0 );
    ~RingBuffer( );

    bool append( uint8_t *data, size_t length );
    bool read( uint8_t *data, size_t length );

    bool GetFrame( uint8_t * &data, size_t &length );

    uint8_t *Data( ) { return buffer; }
    size_t Count( ) { return count; }
    size_t Remaining( ) { return size - readpos; }
    size_t ReadPos( ) { return readpos; }

  private:
    uint8_t *buffer;
    size_t size, overlap, readpos, writepos, count;
};

#endif
