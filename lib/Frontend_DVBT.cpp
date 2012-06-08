/*
 *  tvdaemon
 *
 *  DVB Frontend_DVBT class
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

#include "Frontend_DVBT.h"

#include "Log.h"

Frontend_DVBT::Frontend_DVBT( Adapter &adapter, int adapter_id, int frontend_id, int config_id ) :
  Frontend( adapter, adapter_id, frontend_id, config_id )
{
  type = TVDaemon::Source_DVB_T;
  Log( "  Creating Frontend DVB-T /dev/dvb/adapter%d/frontend%d", adapter_id, frontend_id );
}

Frontend_DVBT::Frontend_DVBT( Adapter &adapter, std::string configfile ) : Frontend( adapter, configfile )
{
}

Frontend_DVBT::~Frontend_DVBT( )
{
}

bool Frontend_DVBT::SaveConfig( )
{
  return Frontend::SaveConfig( );
}

bool Frontend_DVBT::LoadConfig( )
{
  if( !Frontend::LoadConfig( ))
    return false;
  Log( "  Found configured Frontend DVB-T" );
  return true;
}

bool Frontend_DVBT::HandleNIT( struct section *section )
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
  //dvb_nit_section_descriptors_for_each(nit, curd)
  //{
    //if( curd->tag == dtag_dvb_cell_list)
    //{
      //struct dvb_cell_list_descriptor *dx = dvb_cell_list_descriptor_codec(curd);
      //if( dx == NULL )
        //return false;
      //struct dvb_cell_list_entry *entry = NULL;
      //dvb_cell_list_descriptor_cells_for_each(dx, entry)
      //{
        //printf( "Cell %d info: lat %i, long %i, ext_lat %i, ext_long %i\n",
            //entry->cell_id, entry->cell_latitude, entry->cell_longitude,
            //entry->cell_extend_of_latitude, entry->cell_extend_of_longitude );
        //struct dvb_subcell_list_entry *cur_subcell = NULL;
        //dvb_cell_list_entry_subcells_for_each(entry, cur_subcell)
        //{
          //printf( "Subcell %d info: lat %i, long %i, ext_lat %i, ext_long %i\n",
              //cur_subcell->cell_id_extension, cur_subcell->subcell_latitude, cur_subcell->subcell_longitude,
              //cur_subcell->subcell_extend_of_latitude, cur_subcell->subcell_extend_of_longitude );
        //}
      //}
    //}
  //}
  //struct dvb_nit_section_part2 *part2 = dvb_nit_section_part2(nit);
  //struct dvb_nit_transport *cur_transport = NULL;
  //dvb_nit_section_transports_for_each(nit, part2, cur_transport)
  //{
    //dvb_nit_transport_descriptors_for_each(cur_transport, curd)
    //{
      //switch( curd->tag )
      //{
        //case dtag_dvb_terrestial_delivery_system:
          //{
            //struct dvb_terrestrial_delivery_descriptor *dx = dvb_terrestrial_delivery_descriptor_codec(curd);
            //if( dx == NULL )
              //return false;
            //printf( "TS centre_frequency:%x bandwidth:%x priority:%i "
                //"time_slicing_indicator:%i mpe_fec_indicator:%i constellation:%i "
                //"hierarchy_information:%i code_rate_hp_stream:%i "
                //"code_rate_lp_stream:%i guard_interval:%i transmission_mode:%i "
                //"other_frequency_flag:%i\n",
                //dx->centre_frequency, dx->bandwidth, dx->priority,
                //dx->time_slicing_indicator, dx->mpe_fec_indicator,
                //dx->constellation,
                //dx->hierarchy_information, dx->code_rate_hp_stream,
                //dx->code_rate_lp_stream, dx->guard_interval,
                //dx->transmission_mode, dx->other_frequency_flag );
          //}
          //break;
        //case dtag_dvb_frequency_list:
          //{
            //struct dvb_frequency_list_descriptor *dx = dvb_frequency_list_descriptor_codec(curd);
            //if( dx == NULL )
              //return false;
            //printf( "FREQ list type coding %x\n", dx->coding_type );
            //uint32_t *freqs = dvb_frequency_list_descriptor_centre_frequencies(dx);
            //int count = dvb_frequency_list_descriptor_centre_frequencies_count(dx);;
            //for( int iCount = 0; iCount < count; ++iCount)
            //{
              //printf( "DVB freq found: %d\n",freqs[iCount] );
            //}
          //}
          //break;
        //case dtag_dvb_cell_frequency_link:
          //{
            //struct dvb_cell_frequency_link_descriptor *dx = dvb_cell_frequency_link_descriptor_codec(curd);
            //if( dx == NULL )
              //return false;
            //struct dvb_cell_frequency_link_cell *cur = NULL;
            //struct dvb_cell_frequency_link_cell_subcell *cur_subcell = NULL;

            //dvb_cell_frequency_link_descriptor_cells_for_each(dx, cur) {
              //printf("FREQ cell_id:%04x frequency:%i\n", cur->cell_id, cur->frequency);

              //dvb_cell_frequency_link_cell_subcells_for_each(cur, cur_subcell) {
                //printf("FREQ cell_id_extension:%04x transponder_frequency:%i\n", cur_subcell->cell_id_extension, cur_subcell->transposer_frequency);
              //}
            //}

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

