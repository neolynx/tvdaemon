/*
 *  tvdaemon
 *
 *  DVB-S Transponder class
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

#include "Transponder_DVBS.h"

#include "dvb-fe.h"

Transponder_DVBS::Transponder_DVBS( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id )
{
}

Transponder_DVBS::Transponder_DVBS( Source &source, std::string configfile ) : Transponder( source, configfile )
{
}

Transponder_DVBS::~Transponder_DVBS( )
{
}

void Transponder_DVBS::AddProperty( const struct dtv_property &prop )
{
  Transponder::AddProperty( prop );
  switch( prop.cmd )
  {
    case DTV_POLARIZATION:
      polarization = (dvb_sat_polarization) prop.u.data;
      break;
    case DTV_SYMBOL_RATE:
      symbol_rate = prop.u.data;
      break;
    case DTV_INNER_FEC:
      fec_inner = prop.u.data;
      break;
  }
}

bool Transponder_DVBS::SaveConfig( )
{
  switch( delsys )
  {
    case SYS_DVBS2:
      Lookup( "Modulation",  Setting::TypeInt )  = modulation;
      Lookup( "Roll-Off",    Setting::TypeInt )  = roll_off;
      // fall through
    case SYS_DVBS:
      Lookup( "Symbol-Rate",  Setting::TypeInt ) = (int) symbol_rate;
      Lookup( "FEC-Inner",    Setting::TypeInt ) = fec_inner;
      Lookup( "Polarization", Setting::TypeInt ) = polarization;
      break;
  }

  return Transponder::SaveConfig( );
}

bool Transponder_DVBS::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  switch( delsys )
  {
    case SYS_DVBS2:
      modulation = (int) Lookup( "Modulation",  Setting::TypeInt );
      roll_off   = (int) Lookup( "Roll-Off",    Setting::TypeInt );
      // fall through
    case SYS_DVBS:
      symbol_rate  =             (uint32_t) (int) Lookup( "Symbol-Rate",  Setting::TypeInt );
      fec_inner    =                        (int) Lookup( "FEC-Inner",    Setting::TypeInt );
      polarization = (dvb_sat_polarization) (int) Lookup( "Polarization", Setting::TypeInt );
      break;
  }

  return true;
}

bool Transponder_DVBS::GetParams( struct dvb_v5_fe_parms *params ) const
{
  Transponder::GetParams( params );
  dvb_fe_store_parm( params, DTV_POLARIZATION, polarization );
  dvb_fe_store_parm( params, DTV_SYMBOL_RATE, symbol_rate );
  dvb_fe_store_parm( params, DTV_INNER_FEC, fec_inner );
  return true;
}

