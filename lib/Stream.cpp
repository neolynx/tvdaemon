/*
 *  transpondereadend
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

#include "Stream.h"

#include "ConfigObject.h"
#include "Service.h"
#include "Log.h"
#include "Frontend.h"
#include "dvb-demux.h"

#include <RPCObject.h>

Stream::Stream( Service &service, uint16_t id, Type type, int config_id ) :
  service(service),
  id(id),
  type(type)
{
}

Stream::Stream( Service &service ) :
  service(service)
{
}

Stream::~Stream( )
{
}

bool Stream::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "ID",   id );
  config.WriteConfig( "Type", type );
  return true;
}

bool Stream::LoadConfig( ConfigBase &config )
{
  config.ReadConfig( "ID",   id );
  config.ReadConfig( "Type", (int &) type );
  return true;
}

const char *Stream::GetTypeName( Type type )
{
  switch( type )
  {
    case Type_Video:
      return "Video";
    case Type_Video_H262:
      return "Video H262";

    case Type_Video_H264:
      return "Video MPEG4";

    case Type_Audio:
    case Type_Audio_13818_3:
    case Type_Audio_ADTS:
    case Type_Audio_LATM:
      return "Audio";

    case Type_Audio_AC3:
      return "Audio AC3";

    case Type_Audio_AAC:
      return "Audio AAC";

    default:
      return "Unknown";
  }
}

bool Stream::Update( Type type )
{
  if( type != this->type )
  {
    LogWarn( "Stream %d type changed from %s (%d) to %s (%d)", id, GetTypeName( this->type ), this->type, GetTypeName( type ), type );
    this->type = type;
    return false;
  }
  return true;
}

void Stream::json( json_object *entry ) const
{
  json_object_object_add( entry, "id",        json_object_new_int( GetKey( )));
  json_object_object_add( entry, "pid",       json_object_new_int( id ));
  json_object_object_add( entry, "type",      json_object_new_int( type ));
}

int Stream::Open( Frontend &frontend )
{
  int fd = frontend.OpenDemux( );
  if( fd > 0 )
  {
    if( dvb_set_pesfilter( fd, id, DMX_PES_OTHER, DMX_OUT_TSDEMUX_TAP, DMX_BUFSIZE ) != 0 )
    {
      LogError( "failed to set the pes filter for %d", id );
      frontend.CloseDemux( fd );
      return -1;
    }
  }
  return fd;
}

