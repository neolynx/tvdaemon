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

#include "Log.h"

Frontend_DVBC::Frontend_DVBC( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  Frontend( adapter, adapter_id, frontend_id, config_id )
{
  type = TVDaemon::Source_DVB_C;
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
  //struct descriptor *curd = NULL;
  //struct dvb_nit_section_part2 *part2 = dvb_nit_section_part2(nit);
  //struct dvb_nit_transport *cur_transport = NULL;
  //dvb_nit_section_transports_for_each(nit, part2, cur_transport)
  //{
    //dvb_nit_transport_descriptors_for_each(cur_transport, curd)
    //{
      //switch( curd->tag )
      //{
        //case dtag_dvb_cable_delivery_system:
          //{
            //struct dvb_cable_delivery_descriptor *dx = dvb_cable_delivery_descriptor_codec(curd);
            //if( dx == NULL )
              //return false;
            //printf("TS frequency:%x fec_outer:%i modulation:%i symbol_rate:%x fec_inner:%i\n",
                //dx->frequency, dx->fec_outer, dx->modulation,
                //dx->symbol_rate, dx->fec_inner);
          //}
          //break;
      //}
    //}
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

