/*
 *  tvheadend
 *
 *  DVB Port class
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

#include "Port.h"

#include "Frontend.h"
#include "Log.h"

Port::Port( Frontend &frontend, int config_id, std::string name, int id ) :
  ConfigObject( frontend, "port", config_id ),
  frontend(frontend),
  name(name),
  id(id)
{
}

Port::Port( Frontend &frontend, std::string configfile ) :
  ConfigObject( frontend, configfile ),
  frontend(frontend)
{
}

Port::~Port( )
{
}

bool Port::SaveConfig( )
{
  Lookup( "Name", Setting::TypeString ) = name;
  Lookup( "ID",   Setting::TypeInt )    = id;

  return WriteConfig( );
}

bool Port::LoadConfig( )
{
  if( !ReadConfig( ))
    return false;

  name = (const char *) Lookup( "Name", Setting::TypeString );
  id   = (int)          Lookup( "ID",   Setting::TypeInt );

  Log( "    Found configured port '%s'", name.c_str( ));
  return true;
}

bool Port::Tune( Transponder &transponder )
{
  if( frontend.SetPort( id ))
    return frontend.Tune( transponder );
  return false;
}

bool Port::Scan( Transponder &transponder )
{
  if( !frontend.SetPort( id ))
  {
    LogError( "Error setting port %d on frontend", id );
    return false;
  }

  if( !frontend.Tune( transponder ))
    return false;

  if( !frontend.Scan( transponder ))
  {
    LogError( "Frontend Scan failed." );
    return false;
  }

  frontend.Untune();
  return true;
}

bool Port::Tune( Transponder &transponder, uint16_t pno )
{
  if( frontend.SetPort( id ))
    return frontend.TunePID( transponder, pno );
  return false;
}

void Port::Untune( )
{
  frontend.Untune();
}
