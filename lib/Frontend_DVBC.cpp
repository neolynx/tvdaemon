/*
 *  tvdaemon
 *
 *  DVB Frontend_DVBC class
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

#include "Frontend_DVBC.h"

#include "descriptors/nit.h"
#include "descriptors/desc_cable_delivery.h"

#include "Log.h"
#include "Transponder_DVBC.h"

Frontend_DVBC::Frontend_DVBC( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  Frontend( adapter, adapter_id, frontend_id, config_id )
{
  type = Source::Type_DVBC;
  Log( "  Creating Frontend DVB-C /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
}

Frontend_DVBC::Frontend_DVBC( Adapter &adapter, std::string configfile ) : Frontend( adapter, configfile )
{
}

Frontend_DVBC::~Frontend_DVBC( )
{
}

bool Frontend_DVBC::SaveConfig( )
{
  return Frontend::SaveConfig( );
}

bool Frontend_DVBC::LoadConfig( )
{
  Log( "  Loading Frontend DVB-C" );
  if( !Frontend::LoadConfig( ))
    return false;
  return true;
}

bool Frontend_DVBC::HandleNIT( struct dvb_table_nit *nit )
{
  dvb_nit_transport_foreach( tr, nit )
  {
    dvb_desc_find( struct dvb_desc_cable_delivery, desc, tr, cable_delivery_system_descriptor )
    {
//      LogWarn("TS frequency:%x fec_outer:%i modulation:%i symbol_rate:%x fec_inner:%i",
//              desc->frequency, desc->fec_outer, desc->modulation,
//              desc->symbol_rate, desc->fec_inner);

      fe_modulation modulation = QAM_16;
      switch( desc->modulation )
      {
        case 0:
          modulation = QPSK; // FIXME: should be INVALID
          break;
        case 1:
          modulation = QAM_16;
          break;
        case 2:
          modulation = QAM_32;
          break;
        case 3:
          modulation = QAM_64;
          break;
        case 4:
          modulation = QAM_128;
          break;
        case 5:
          modulation = QAM_256;
          break;
      }

      fe_code_rate fec = FEC_NONE;
      switch( desc->fec_inner )
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
        case 15:
          fec = FEC_NONE;
          break;
        default:
          LogWarn( "got unknown fec: %d", desc->fec_inner );
          break;
      }
      Source &source = transponder->GetSource( );
      Transponder_DVBC *t = new Transponder_DVBC( source,
						  desc->frequency,
						  desc->symbol_rate,
						  fec,
                                                  modulation,
						  source.GetTransponderCount( ));
      if( !source.AddTransponder( t ))
        delete t;
      else
        Log( "  Added transponder %s", t->toString( ).c_str( ));
    }
  }

  return true;
}

