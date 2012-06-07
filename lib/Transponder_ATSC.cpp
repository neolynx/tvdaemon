/*
 *  tvheadend
 *
 *  ATSC Transponder class
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

#include "Transponder_ATSC.h"

Transponder_ATSC::Transponder_ATSC( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id )
{
}

Transponder_ATSC::Transponder_ATSC( Source &source, std::string configfile ) : Transponder( source, configfile )
{
}

Transponder_ATSC::~Transponder_ATSC( )
{
}

void Transponder_ATSC::AddProperty( const struct dtv_property &prop )
{
  Transponder::AddProperty( prop );
  switch( prop.cmd )
  {
  }
}

bool Transponder_ATSC::SaveConfig( )
{
  Lookup( "Modulation", Setting::TypeInt ) = atsc_modulation;

  return Transponder::SaveConfig( );
}

bool Transponder_ATSC::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  atsc_modulation = (int) Lookup( "Modulation", Setting::TypeInt );
  return true;
}

