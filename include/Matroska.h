/*
 *  tvdaemon
 *
 *  DVB Matroska class
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

#ifndef _Matroska_
#define _Matroska_

#include <ebml/StdIOCallback.h>
#include <ebml/EbmlHead.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxSeekHead.h>
#include "matroska/KaxCues.h"
#include "matroska/KaxCluster.h"


using namespace LIBEBML_NAMESPACE;
using namespace LIBMATROSKA_NAMESPACE;

template<typename A, typename B> B &GetChildAs( EbmlMaster &m )
{
  return GetChild<A>( m );
}

class Matroska
{
  public:
    Matroska( std::string name );
    ~Matroska( );

    void WriteHeader( );
    void AddTrack( );
    void AddCluster( uint64_t ts );
    void CloseCluster( );
    void AddFrame( uint8_t *data, size_t size );

  private:
    std::string name;
    uint64_t timecode_scale;

    StdIOCallback *out;
    EbmlHead header;
    KaxSegment segment;
    EbmlVoid dummy;
    KaxSeekHead seek;
    KaxCues cues;
    KaxCluster *cluster;

    KaxTrackEntry *track;
    KaxBlockGroup *last_block;

    uint64_t segment_size;
    uint64_t curr_seg_size;

    int track_count;

};

#endif
