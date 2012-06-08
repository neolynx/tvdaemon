/*
 *  tvdaemon
 *
 *  DVB Frontend_DVBS class
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

#include "Frontend_DVBS.h"

#include "Port.h"
#include "Transponder_DVBS.h"
#include "Log.h"
#include "Source.h"

#include "dvb-fe.h"
#include "dvb-frontend.h"
#include "descriptors/nit.h"
#include "descriptors/desc_network_name.h"
#include "descriptors/desc_sat.h"

Frontend_DVBS::Frontend_DVBS( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  Frontend( adapter, adapter_id, frontend_id, config_id )
{
  type = TVDaemon::Source_DVB_S;
  Log( "  Creating Frontend DVB-S /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
  LNB = "UNIVERSAL";
}

Frontend_DVBS::Frontend_DVBS( Adapter &adapter, std::string configfile ) : Frontend( adapter, configfile )
{
}

Frontend_DVBS::~Frontend_DVBS( )
{
}

bool Frontend_DVBS::SaveConfig( )
{
  Lookup( "LNB", Setting::TypeString ) = LNB;
  return Frontend::SaveConfig( );
}

bool Frontend_DVBS::LoadConfig( )
{
  if( !Frontend::LoadConfig( ))
    return false;
  const char *t = Lookup( "LNB", Setting::TypeString );
  if( t )
    LNB = t;
  Log( "  Found configured Frontend DVB-S" );
  return true;
}

bool Frontend_DVBS::Tune( const Transponder &t, int timeout )
{
  if( !Open( ))
    return false;

  int satpos = GetCurrentPort( )->GetID( );
  int lnb = dvb_sat_search_lnb( LNB.c_str( ));
  if( lnb < 0 )
  {
    LogError( "Unknown LNB '%s'", LNB.c_str( ));
    return false;
  }

  fe->sat_number = satpos % 3;
  fe->lnb = dvb_sat_get_lnb( lnb ); // FIXME: put to ptroperties in v4l
  fe->diseqc_wait = 0;
  fe->freq_bpf = 0;

  dvb_set_compat_delivery_system( fe, t.GetDelSys( ));
  t.GetParams( fe );

  dvb_fe_prt_parms( fe );

  int r = dvb_fe_set_parms( fe );
  if( r != 0 )
  {
    LogError( "dvb_fe_set_parms failed." );
    return false;
  }

  uint32_t freq;
  dvb_fe_retrieve_parm( fe, DTV_FREQUENCY, &freq );
  Log( "tuning to %d", freq );

  if( !GetLockStatus( ))
    return false;

  transponder = &t;
  return true;
}




bool Frontend_DVBS::HandleNIT( struct dvb_table_nit *nit )
{
  dvb_desc_find( struct dvb_desc_network_name, desc, nit, network_name_descriptor )
  if( desc )
  {
    Log( "got network name %s", desc->network_name );
    //transponder.SetNetwork( desc->network_name );
  }
  dvb_nit_transport_foreach( tr, nit )
  {
    dvb_desc_find( struct dvb_desc_sat, desc, tr, satellite_delivery_system_descriptor )
    {
      fe_delivery_system delsys = desc->modulation_system ? SYS_DVBS2 : SYS_DVBS;

      fe_code_rate fec = FEC_NONE;
      switch( desc->fec )
      {
        case 0:
          break;
        case 1:
          fec = FEC_1_2;
          break;
        case 2:
          fec = FEC_2_3;
          break;
        case 3:
          fec = FEC_3_4;
          break;
        case 4:
          fec = FEC_5_6;
          break;
        case 5:
          fec = FEC_7_8;
          break;
        case 6:
          fec = FEC_8_9;
          break;
        case 7:
          fec = FEC_3_5;
          break;
        case 8:
          fec = FEC_4_5;
          break;
        case 9:
          fec = FEC_9_10;
          break;
        default:
          LogWarn( "got unknown fec: %d", desc->fec );
          break;
      }

      dvb_sat_polarization polarization = POLARIZATION_OFF;
      switch( desc->polarization )
      {
        case 0:
          polarization = POLARIZATION_H;
          break;
        case 1:
          polarization = POLARIZATION_V;
          break;
        case 2:
          polarization = POLARIZATION_L;
          break;
        case 3:
          polarization = POLARIZATION_R;
          break;
      }

      Source &source = transponder->GetSource( );
      Transponder_DVBS *t = new Transponder_DVBS( source,
						  delsys,
						  desc->frequency,
						  polarization,
						  desc->symbol_rate,
						  fec,
						  desc->roll_off,
						  source.GetTransponderCount( ));
      if( !source.AddTransponder( t ))
        delete t;

    }
  }

  return true;
}

