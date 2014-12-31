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

#include <libdvbv5/dvb-file.h>

Transponder_DVBC::Transponder_DVBC( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id ),
  inner_fec(FEC_AUTO),
  modulation(QAM_AUTO),
  symbol_rate(0),
  inversion(INVERSION_AUTO)
{
  has_sdt = true;
  has_nit = true;
}

Transponder_DVBC::Transponder_DVBC( Source &source, std::string configfile ) : Transponder( source, configfile ),
  inner_fec(FEC_AUTO),
  modulation(QAM_AUTO),
  symbol_rate(0),
  inversion(INVERSION_AUTO)
{
  has_sdt = true;
  has_nit = true;
}

Transponder_DVBC::Transponder_DVBC( Source &source,
		    uint32_t frequency,
		    uint32_t symbol_rate,
		    fe_code_rate fec,
		    fe_modulation modulation,
		    int config_id ) :
  Transponder( source, SYS_DVBC_ANNEX_A, config_id ),
  symbol_rate(symbol_rate),
  inner_fec(fec),
  modulation(modulation),
  inversion(INVERSION_AUTO)
{
  this->frequency = frequency;
  has_sdt = true;
  has_nit = true;
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
  WriteConfig( "Symbol-Rate", symbol_rate );
  WriteConfig( "FEC-Inner",   inner_fec );
  WriteConfig( "Modulation",  modulation );
  WriteConfig( "Inversion",   inversion );

  return Transponder::SaveConfig( );
}

bool Transponder_DVBC::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  ReadConfig( "Symbol-Rate", symbol_rate );
  ReadConfig( "FEC-Inner",   inner_fec );
  ReadConfig( "Modulation",  modulation );
  ReadConfig( "Inversion",   inversion );
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

std::string Transponder_DVBC::toString( ) const
{
  char tmp[256];
  snprintf(tmp, sizeof(tmp), "%.2f MHz %s %dK %s %d", frequency / 1000000.0, fe_modulation_name[modulation],
		  symbol_rate / 1000, fe_code_rate_name[inner_fec], inversion );
  return tmp;
}

bool Transponder_DVBC::IsSame( const Transponder &t )
{
  const Transponder_DVBC &other = (const Transponder_DVBC &) t;
  if( other.delsys != delsys )
    return false;
  if( other.frequency != frequency )
    return false;
  if( other.symbol_rate != symbol_rate )
    return false;
  if( other.inner_fec != inner_fec )
    return false;
  return true;
}

