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

#ifndef _MPEGTS_
#define _MPEGTS_

#include <string>
#include <stdint.h>

class Frame;

class MPEGTS
{
  public:
    MPEGTS( std::string file );
    virtual ~MPEGTS( );

    bool Open( );
    Frame *ReadFrame( );

    double GetDuration( );

  private:

    std::string filename;
    int fd;
    uint64_t seq;

};

#endif

