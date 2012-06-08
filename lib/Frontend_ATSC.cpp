/*
 *  tvdaemon
 *
 *  DVB Frontend_ATSC class
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

#include "Frontend_ATSC.h"

#include "Transponder.h"
#include "Log.h"

Frontend_ATSC::Frontend_ATSC( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  Frontend( adapter, adapter_id, frontend_id, config_id )
{
  type = TVDaemon::Source_ATSC;
  Log( "  Creating Frontend ATSC /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
}

Frontend_ATSC::Frontend_ATSC( Adapter &adapter, std::string configfile ) : Frontend( adapter, configfile )
{
}

Frontend_ATSC::~Frontend_ATSC( )
{
}

bool Frontend_ATSC::SaveConfig( )
{
  return Frontend::SaveConfig( );
}

bool Frontend_ATSC::LoadConfig( )
{
  if( !Frontend::LoadConfig( ))
    return false;
  Log( "  Found configured Frontend ATSC" );
  return true;
}

bool Frontend_ATSC::HandleNIT( struct section *section )
{
  //struct section_ext *section_ext = NULL;
  //if(( section_ext = section_ext_decode( section, 1 )) == NULL )
  //{
    //printf( "unable to extract section_ext of nit\n" );
    //return false;
  //}
  //struct dvb_nit_section *nit = dvb_nit_section_codec(section_ext);
  //if( nit == NULL )
  //{
    //printf( "NIT section decode error\n" );
    //return false;
  //}

  //// switch to the SDT table
  //filter[0] = stag_dvb_service_description_actual;
  //if( dvbdemux_set_section_filter( fd_demux, TRANSPORT_SDT_PID, filter, mask, 1, 1 ))
  //{
    //printf( "failed to set demux filter for sdt\n" );
    //up = false;
    //return false;
  //}
  return true;
}

