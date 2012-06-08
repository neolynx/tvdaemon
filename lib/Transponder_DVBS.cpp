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

Transponder_DVBS::Transponder_DVBS( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id )
{
}

Transponder_DVBS::Transponder_DVBS( Source &source, std::string configfile ) : Transponder( source, configfile )
{
}

Transponder_DVBS::Transponder_DVBS( Source &source,
		    fe_delivery_system_t delsys, uint32_t frequency,
		    dvb_sat_polarization polarization,
		    uint32_t symbol_rate,
		    fe_code_rate fec,
		    int roll_off,
		    int config_id ) :
  Transponder( source, delsys, config_id ), polarization(polarization), symbol_rate(symbol_rate), fec(fec)
{
  this->frequency = frequency;
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
      fec = prop.u.data;
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
      Lookup( "FEC",          Setting::TypeInt ) = fec;
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
      fec          =                        (int) Lookup( "FEC",          Setting::TypeInt );
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
  dvb_fe_store_parm( params, DTV_INNER_FEC, fec );
  return true;
}

std::string Transponder_DVBS::toString( )
{
  char tmp[256];
  const char *sys;
  switch( delsys )
  {
    case SYS_DVBS2:
      sys = "DVB-S2";
      break;
    case SYS_DVBS:
      sys = "DVB-S";
      break;
    default:
      sys = "???";
      break;
  }
  snprintf( tmp, sizeof(tmp), "%s %d %C %d %d", sys, frequency, dvb_sat_pol_name[polarization][0], symbol_rate, roll_off );
  return tmp;
}

bool Transponder_DVBS::IsSame( const Transponder &t )
{
  if( t.GetDelSys( ) != delsys )
    return false;
  if( t.GetFrequency( ) != frequency )
    return false;
  const Transponder_DVBS &t2 = (const Transponder_DVBS &) t;
  if( t2.polarization != polarization )
    return false;
  if( t2.symbol_rate != symbol_rate )
    return false;
  if( t2.fec != fec )
    return false;
  if( t2.roll_off != roll_off )
    return false;
  return true;
}

