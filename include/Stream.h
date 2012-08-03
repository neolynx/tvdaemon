/*
 *  tvdaemon
 *
 *  DVB Stream class
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

#ifndef _Stream_
#define _Stream_

#include "ConfigObject.h"

#include <stdint.h>

#include "descriptors.h"

class Service;

class Stream : public ConfigObject
{
  public:
    enum Type
    {
      Video      = video_stream_descriptor,
      VideoMPEG4 = mpeg4_video_descriptor,
      Audio      = audio_stream_descriptor,
      AudioMPEG4 = mpeg4_audio_descriptor,
      //AudioAC3   = ,
      //AudioAAC   =
    };

    Stream( Service &service, uint16_t id, enum Type type, int config_id );
    Stream( Service &service, std::string configfile );
    virtual ~Stream( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    virtual int GetKey( ) const { return id; }

    bool Update( Type type );
    Type GetType() { return type; }

  private:
    Service &service;
    uint16_t id;
    Type type;
};

#endif
