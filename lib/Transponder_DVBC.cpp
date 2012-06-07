/*
 *  tvheadend
 *
 *  DVB-C Transponder class
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

#include "Transponder_DVBC.h"

#include "dvb-file.h"

Transponder_DVBC::Transponder_DVBC( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id )
{
}

Transponder_DVBC::Transponder_DVBC( Source &source, std::string configfile ) : Transponder( source, configfile )
{
}

Transponder_DVBC::~Transponder_DVBC( )
{
}

void Transponder_DVBC::AddProperty( const struct dtv_property &prop )
{
  Transponder::AddProperty( prop );
  switch( prop.cmd )
  {
  }
}

bool Transponder_DVBC::SaveConfig( )
{
  Lookup( "Symbol-Rate", Setting::TypeInt ) = dvbc_symbol_rate;
  Lookup( "FEC-Inner",   Setting::TypeInt ) = dvbc_fec_inner;
  Lookup( "Modulation",  Setting::TypeInt ) = dvbc_modulation;

  return Transponder::SaveConfig( );
}

bool Transponder_DVBC::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  dvbc_symbol_rate = (int) Lookup( "Symbol-Rate",  Setting::TypeInt );
  dvbc_fec_inner   = (int) Lookup( "FEC-Inner",    Setting::TypeInt );
  dvbc_modulation  = (int) Lookup( "Modulation",   Setting::TypeInt );
  return true;
}

