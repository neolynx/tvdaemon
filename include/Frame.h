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

#ifndef _Frame_
#define _Frame_

#include <stdint.h> // uint8_t
#include <unistd.h> // size_t
#include "descriptors/mpeg_pes.h"
#include "descriptors/mpeg_es.h"

//class Matroska;
#include <ccrtp/rtp.h>

class Frame
{
  public:
    Frame( struct dvb_v5_fe_parms &fe );
    ~Frame( );

    bool ReadFrame( uint8_t *data, size_t size, ost::RTPSession *session );

  private:
    struct dvb_v5_fe_parms &fe;
    //Matroska &mkv;

    uint8_t *buffer;
    size_t   buffer_size;
    size_t   buffer_length;

    bool started;
    bool slices;
    struct dvb_mpeg_pes pes;
    struct dvb_mpeg_es_seq_start seq_start;
    struct dvb_mpeg_es_pic_start pic_start;

    uint64_t pts_start;
    uint64_t pts;
    uint64_t last_pts;

    dvb_mpeg_es_frame_t frame_type;
    dvb_mpeg_es_frame_t last_frame_type;

    uint32_t timestamp;
};

#endif
