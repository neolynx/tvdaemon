/*
 *  tvdaemon
 *
 *  DVB-T Transponder class
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

#include "Transponder_DVBT.h"

#include "dvb-fe.h"

Transponder_DVBT::Transponder_DVBT( Source &source, const fe_delivery_system_t delsys, int config_id ) : Transponder( source, delsys, config_id )
{
}

Transponder_DVBT::Transponder_DVBT( Source &source, std::string configfile ) : Transponder( source, configfile )
{
}

Transponder_DVBT::~Transponder_DVBT( )
{
}

void Transponder_DVBT::AddProperty( const struct dtv_property &prop )
{
  Transponder::AddProperty( prop );
  switch( prop.cmd )
  {
    case DTV_MODULATION:
      modulation = prop.u.data;
      break;
    case DTV_BANDWIDTH_HZ:
      bandwidth = prop.u.data;
      break;
    case DTV_CODE_RATE_HP:
      code_rate_HP = prop.u.data;
      break;
    case DTV_CODE_RATE_LP:
      code_rate_LP = prop.u.data;
      break;
    case DTV_GUARD_INTERVAL:
      guard_interval = prop.u.data;
      break;
    case DTV_TRANSMISSION_MODE:
      transmission_mode = prop.u.data;
      break;
    case DTV_HIERARCHY:
      hierarchy = prop.u.data;
      break;
    case DTV_DVBT2_PLP_ID:
      plp_id = prop.u.data;
      break;
  }
}

bool Transponder_DVBT::SaveConfig( )
{
  Lookup( "Modulation",        Setting::TypeInt ) = modulation;
  Lookup( "Bandwidth",         Setting::TypeInt ) = bandwidth;
  Lookup( "Code-Rate-HP",      Setting::TypeInt ) = code_rate_HP;
  Lookup( "Code-Rate-LP",      Setting::TypeInt ) = code_rate_LP;
  Lookup( "Guard-Interval",    Setting::TypeInt ) = guard_interval;
  Lookup( "Transmission-Mode", Setting::TypeInt ) = transmission_mode;
  Lookup( "Hierarchy",         Setting::TypeInt ) = hierarchy;
  if( delsys == SYS_DVBT2 )
    Lookup( "PLP ID",          Setting::TypeInt ) = plp_id;

  return Transponder::SaveConfig( );
}

bool Transponder_DVBT::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  modulation        = (int) Lookup( "Modulation",        Setting::TypeInt );
  bandwidth         = (int) Lookup( "Bandwidth",         Setting::TypeInt );
  code_rate_HP      = (int) Lookup( "Code-Rate-HP",      Setting::TypeInt );
  code_rate_LP      = (int) Lookup( "Code-Rate-LP",      Setting::TypeInt );
  transmission_mode = (int) Lookup( "Transmission-Mode", Setting::TypeInt );
  guard_interval    = (int) Lookup( "Guard-Interval",    Setting::TypeInt );
  hierarchy         = (int) Lookup( "Hierarchy",         Setting::TypeInt );
  if( delsys == SYS_DVBT2 )
    plp_id          = (int) Lookup( "PLP ID",            Setting::TypeInt );
  return true;
}

bool Transponder_DVBT::GetParams( struct dvb_v5_fe_parms *params ) const
{
  Transponder::GetParams( params );
  dvb_fe_store_parm( params, DTV_MODULATION, modulation );
  dvb_fe_store_parm( params, DTV_BANDWIDTH_HZ, bandwidth );
  dvb_fe_store_parm( params, DTV_CODE_RATE_HP, code_rate_HP );
  dvb_fe_store_parm( params, DTV_CODE_RATE_LP, code_rate_LP );
  dvb_fe_store_parm( params, DTV_GUARD_INTERVAL, guard_interval );
  dvb_fe_store_parm( params, DTV_TRANSMISSION_MODE, transmission_mode );
  dvb_fe_store_parm( params, DTV_HIERARCHY, hierarchy );
  if( delsys == SYS_DVBT2 )
    dvb_fe_store_parm( params, DTV_DVBT2_PLP_ID, plp_id );
  return true;
}

std::string Transponder_DVBT::toString( ) const
{
  char tmp[256];
  snprintf(tmp, sizeof(tmp), "DVB-T  %d %s %s %s %s", frequency, fe_code_rate_name[code_rate_HP],
      fe_modulation_name[modulation],
      fe_guard_interval_name[guard_interval],
      fe_transmission_mode_name[transmission_mode]
      );
  return tmp;
}

bool Transponder_DVBT::IsSame( const Transponder &t )
{
  return true;
}

