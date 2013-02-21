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
#include "Log.h"

#include "dvb-fe.h"
#include "descriptors/desc_sat.h"

Transponder_DVBS::Transponder_DVBS( Source &source, const fe_delivery_system_t delsys, int config_id ) :
  Transponder( source, delsys, config_id ),
  polarization(POLARIZATION_OFF),
  symbol_rate(0),
  fec(FEC_NONE),
  modulation(QPSK),
  roll_off(ROLLOFF_35)
{
  Init( );
}

Transponder_DVBS::Transponder_DVBS( Source &source, std::string configfile ) :
  Transponder( source, configfile ),
  polarization(POLARIZATION_OFF),
  symbol_rate(0),
  fec(FEC_NONE),
  modulation(QPSK),
  roll_off(ROLLOFF_35)
{
  Init( );
}

Transponder_DVBS::Transponder_DVBS( Source &source,
		    fe_delivery_system_t delsys, uint32_t frequency,
		    dvb_sat_polarization polarization,
		    uint32_t symbol_rate,
		    fe_code_rate fec,
		    fe_modulation modulation,
		    fe_rolloff roll_off,
		    int config_id ) :
  Transponder( source, delsys, config_id ),
  polarization(polarization),
  symbol_rate(symbol_rate),
  fec(fec),
  modulation(modulation),
  roll_off(roll_off)
{
  this->frequency = frequency;
  Init( );
}

Transponder_DVBS::~Transponder_DVBS( )
{
}

void Transponder_DVBS::Init( )
{
  has_sdt = true;
  has_nit = true;
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
      fec = (fe_code_rate) prop.u.data;
      break;
  }
}

bool Transponder_DVBS::SaveConfig( )
{
  switch( delsys )
  {
    case SYS_DVBS2:
      WriteConfig( "Modulation",  modulation );
      WriteConfig( "Roll-Off",    roll_off );
      // fall through
    case SYS_DVBS:
      WriteConfig( "Symbol-Rate",  symbol_rate );
      WriteConfig( "FEC",          fec );
      WriteConfig( "Polarization", polarization );
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
      ReadConfig( "Modulation",  (int &) modulation );
      ReadConfig( "Roll-Off",    (int &) roll_off );
      // fall through
    case SYS_DVBS:
      ReadConfig( "Symbol-Rate",  symbol_rate );
      ReadConfig( "FEC",          (int &) fec );
      ReadConfig( "Polarization", (int &) polarization );
      break;
  }

  return true;
}

bool Transponder_DVBS::GetParams( struct dvb_v5_fe_parms *params ) const
{
  Transponder::GetParams( params );
  dvb_fe_store_parm( params, DTV_INVERSION, INVERSION_AUTO );
  dvb_fe_store_parm( params, DTV_POLARIZATION, polarization );
  dvb_fe_store_parm( params, DTV_SYMBOL_RATE, symbol_rate );
  dvb_fe_store_parm( params, DTV_INNER_FEC, fec );
  if( delsys == SYS_DVBS2 )
  {
    dvb_fe_store_parm( params, DTV_MODULATION, modulation );
    dvb_fe_store_parm( params, DTV_ROLLOFF, roll_off );
  }
  return true;
}

std::string Transponder_DVBS::toString( ) const
{
  char tmp[256];
  switch( delsys )
  {
    case SYS_DVBS2:
      snprintf( tmp, sizeof(tmp), "DVB-S2 %.3f GHz %c %d %s %s roll off: %s", frequency / 1000000.0, dvb_sat_pol_name[polarization][0], symbol_rate, fe_code_rate_name[fec], fe_modulation_name[modulation], fe_rolloff_name[roll_off] );
      break;
    case SYS_DVBS:
      snprintf( tmp, sizeof(tmp), "DVB-S  %.3f GHz %c %d %s", frequency / 1000000.0, dvb_sat_pol_name[polarization][0], symbol_rate, fe_code_rate_name[fec] );
      break;
    default:
      strcpy( tmp, "Unknown Transponder type" );
      break;
  }
  return tmp;
}

bool Transponder_DVBS::IsSame( const Transponder &t )
{
  const Transponder_DVBS &other = (const Transponder_DVBS &) t;
  if( other.delsys != delsys )
    return false;
  if( other.frequency != frequency )
    return false;
  if( other.polarization != polarization )
    return false;
  if( other.symbol_rate != symbol_rate )
    return false;
  if( other.fec != fec )
    return false;
  if( other.roll_off != roll_off )
    return false;
  return true;
}

