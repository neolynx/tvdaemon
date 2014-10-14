/*
 *  tvdaemon
 *
 *  DVB Frame class
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

#ifndef _Frame_
#define _Frame_

#include <stdint.h> // uint8_t
#include <libdvbv5/mpeg_ts.h>

class Frame
{
    public:
      Frame( );
      virtual ~Frame( );

      uint8_t *GetBuffer( );

      uint64_t GetSequence( ) const;
      void     SetSequence( uint64_t seq );

      uint64_t GetTimestamp( ) const;
      void     SetTimestamp( uint64_t ts );

      uint16_t GetPID( ) const;

      bool ParseHeaders( uint64_t *timestamp, bool &got_timestamp, struct dvb_mpeg_ts **ts );

      class Comp
      {
        public:
          bool operator()( Frame *a, Frame *b );
      };

    private:
      uint16_t pid;
      uint64_t ts;
      uint64_t seq;
      uint8_t  data[DVB_MPEG_TS_PACKET_SIZE];

};

#endif
