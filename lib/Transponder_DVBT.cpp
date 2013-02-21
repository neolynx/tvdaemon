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
  has_sdt = true;
  has_nit = true;
}

Transponder_DVBT::Transponder_DVBT( Source &source, std::string configfile ) : Transponder( source, configfile )
{
  has_sdt = true;
  has_nit = true;
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
    case DTV_DVBT2_PLP_ID_LEGACY:
      plp_id = prop.u.data;
      break;
  }
}

bool Transponder_DVBT::SaveConfig( )
{
  WriteConfig( "Modulation",        modulation );
  WriteConfig( "Bandwidth",         bandwidth );
  WriteConfig( "Code-Rate-HP",      code_rate_HP );
  WriteConfig( "Code-Rate-LP",      code_rate_LP );
  WriteConfig( "Guard-Interval",    guard_interval );
  WriteConfig( "Transmission-Mode", transmission_mode );
  WriteConfig( "Hierarchy",         hierarchy );
  if( delsys == SYS_DVBT2 )
    WriteConfig( "PLP ID",          plp_id );

  return Transponder::SaveConfig( );
}

bool Transponder_DVBT::LoadConfig( )
{
  if( !Transponder::LoadConfig( ))
    return false;

  ReadConfig( "Modulation",        modulation );
  ReadConfig( "Bandwidth",         bandwidth );
  ReadConfig( "Code-Rate-HP",      code_rate_HP );
  ReadConfig( "Code-Rate-LP",      code_rate_LP );
  ReadConfig( "Transmission-Mode", transmission_mode );
  ReadConfig( "Guard-Interval",    guard_interval );
  ReadConfig( "Hierarchy",         hierarchy );
  if( delsys == SYS_DVBT2 )
    ReadConfig( "PLP ID",          plp_id );
  return true;
}

bool Transponder_DVBT::GetParams( struct dvb_v5_fe_parms *params ) const
{
  Transponder::GetParams( params );
  dvb_fe_store_parm( params, DTV_INVERSION, INVERSION_AUTO );
  dvb_fe_store_parm( params, DTV_MODULATION, modulation );
  dvb_fe_store_parm( params, DTV_BANDWIDTH_HZ, bandwidth );
  dvb_fe_store_parm( params, DTV_CODE_RATE_HP, code_rate_HP );
  dvb_fe_store_parm( params, DTV_CODE_RATE_LP, code_rate_LP );
  dvb_fe_store_parm( params, DTV_GUARD_INTERVAL, guard_interval );
  dvb_fe_store_parm( params, DTV_TRANSMISSION_MODE, transmission_mode );
  dvb_fe_store_parm( params, DTV_HIERARCHY, hierarchy );
  if( delsys == SYS_DVBT2 )
    dvb_fe_store_parm( params, DTV_DVBT2_PLP_ID_LEGACY, plp_id );
  return true;
}

std::string Transponder_DVBT::toString( ) const
{
  char tmp[256];
  snprintf(tmp, sizeof(tmp), "DVB-T  %.2f MHz %s %s %s %s", frequency / 1000000.0, fe_code_rate_name[code_rate_HP],
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

