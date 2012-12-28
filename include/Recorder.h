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

#ifndef _Recorder_
#define _Recorder_

#include "ebml/EbmlMaster.h"

using namespace LIBEBML_NAMESPACE;

class Matroska;

template<typename A, typename B> B & GetChildAs( EbmlMaster *m )
{
    return GetChild<A>( *m );
}

class Recorder
{
  public:
    Recorder( );
    ~Recorder( );

    void AddTrack( );
    void AddCluster( uint64_t ts );
    void record( uint8_t *data, int size );

  private:
    Matroska *mkv;

    uint8_t buffer[16348];
    int readpos, writepos;
};

#endif
