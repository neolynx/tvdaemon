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

#include "Service.h"
#include "Log.h"

Stream::Stream( Service &service, uint16_t id, Type type, int config_id ) :
  ConfigObject( service, "stream", config_id ),
  service(service),
  id(id),
  type(type)
{
  Log( "Created Stream with id %d", id );
}

Stream::Stream( Service &service, std::string configfile ) :
  ConfigObject( service, configfile ),
  service(service)
{
}

Stream::~Stream( )
{
}

bool Stream::SaveConfig( )
{
  Lookup( "ID",          Setting::TypeInt )    = id;
  Lookup( "Type",        Setting::TypeInt )    = (int) type;
  return WriteConfig( );
}

bool Stream::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  id   =        (int) Lookup( "ID",   Setting::TypeInt );
  type = (Type) (int) Lookup( "Type", Setting::TypeInt );

  return true;
}

bool Stream::Update( Type type )
{
  if( type != this->type )
  {
    LogWarn( "Warning: Stream %d type changed from %d to %d", id, this->type, type );
    this->type = type;
    return false;
  }
  return true;
}

