/*
 *  tvdaemon
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

Transponder_DVBC::Transponder_DVBC( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id ),
  inner_fec(FEC_AUTO),
  modulation(QAM_AUTO),
  symbol_rate(0),
  inversion(INVERSION_AUTO)
{
}

Transponder_DVBC::Transponder_DVBC( Source &source, std::string configfile ) : Transponder( source, configfile ),
  inner_fec(FEC_AUTO),
  modulation(QAM_AUTO),
  symbol_rate(0),
  inversion(INVERSION_AUTO)
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
    case DTV_SYMBOL_RATE:
      symbol_rate = prop.u.data;
      break;
    case DTV_MODULATION:
      modulation = prop.u.data;
      break;
    case DTV_INNER_FEC:
      inner_fec = prop.u.data;
      break;
    case DTV_INVERSION:
      inversion = prop.u.data;
      break;
  }
}

bool Transponder_DVBC::SaveConfig( )
{
  Lookup( "Symbol-Rate", Setting::TypeInt ) = (int) symbol_rate;
  Lookup( "FEC-Inner",   Setting::TypeInt ) = (int) inner_fec;
  Lookup( "Modulation",  Setting::TypeInt ) = (int) modulation;
  Lookup( "Inversion",   Setting::TypeInt ) = (int) inversion;

  return Transponder::SaveConfig( );
}

bool Transponder_DVBC::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  symbol_rate = (uint32_t) Lookup( "Symbol-Rate", Setting::TypeInt );
  inner_fec   = (uint32_t) Lookup( "FEC-Inner",   Setting::TypeInt );
  modulation  = (uint32_t) Lookup( "Modulation",  Setting::TypeInt );
  inversion   = (uint32_t) Lookup( "Inversion",   Setting::TypeInt );
  return true;
}

bool Transponder_DVBC::GetParams( struct dvb_v5_fe_parms* params ) const
{
  if ( !Transponder::GetParams(params))
    return false;

  dvb_fe_store_parm( params, DTV_SYMBOL_RATE, symbol_rate );
  dvb_fe_store_parm( params, DTV_INNER_FEC, inner_fec );
  dvb_fe_store_parm( params, DTV_MODULATION, modulation );
  dvb_fe_store_parm( params, DTV_INVERSION, inversion );
  return true;
}

std::string Transponder_DVBC::toString( )
{
  char tmp[256];
  snprintf(tmp, sizeof(tmp), "%d", frequency );
  return tmp;
}

bool Transponder_DVBC::IsSame( const Transponder &t )
{
  return true;
}

