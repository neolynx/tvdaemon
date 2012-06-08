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
#include "Transponder.h"
#include "Log.h"

#include "dvb-fe.h"

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
  if( r < 0 )
  {
    LogError( "dvb_fe_set_parms failed." );
    return false;
  }

  uint32_t freq;
  dvb_fe_retrieve_parm( fe, DTV_FREQUENCY, &freq );
  Log( "tuning to %d", freq );

  if( !GetLockStatus( ))
    return false;

  return true;
}

bool Frontend_DVBS::HandleNIT( struct section *section )
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
  //case dtag_dvb_satellite_delivery_system:
  //{
  //struct dvb_satellite_delivery_descriptor *dx = dvb_satellite_delivery_descriptor_codec(curd);
  //if( dx == NULL )
  //return false;

  //dx->frequency = Utils::decihex( dx->frequency ) * 10;
  //dx->symbol_rate = Utils::decihex( dx->symbol_rate ) * 100;

  //char pol;
  //switch( dx->polarization )
  //{
  //case 0:
  //pol = 'h';
  //break;
  //case 1:
  //pol = 'v';
  //break;
  //default:
  //printf( "unknown polarization %d\n", dx->polarization );
  //break;
  //}

  //enum ModType
  //{
  //MOD_NONE = 0,
  //MOD_QPSK,
  //MOD_8PSK,
  //MOD_16QAM
  //} modulation;

  //modulation = (ModType) dx->modulation_type;

  //const char *mod;
  //switch( modulation )
  //{
  //case MOD_NONE:
  //mod = "NONE";
  //break;
  //case MOD_QPSK:
  //mod = "QPSK";
  //break;
  //case MOD_8PSK:
  //mod = "8PSK";
  //break;
  //case MOD_16QAM:
  //mod = "16QAM";
  //break;
  //}

  ////printf( "  Got Transponder %d %c roll_off:%i %s modulation: %s symbol_rate:%d fec_inner:%i\n",
  ////dx->frequency,
  ////pol,
  ////dx->roll_off,
  ////dx->modulation_system == 1 ? "DVB-S2" : "DVB-S ",
  ////mod,
  ////dx->symbol_rate,
  ////dx->fec_inner);

  //if( dx->modulation_system == 1 )
  //{
  //printf( "Warning: DVB-S2 not supported yet\n" );
  //break;
  //}
  //if( modulation == MOD_NONE )
  //{
  //printf( "Warning: unknown modulation\n" );
  //break;
  //}

  ////dvbfe_fec fec;
  ////switch( dx->fec_inner )
  ////{
  ////case 0:
  ////fec = DVBFE_FEC_NONE;
  ////break;
  ////case 1:
  ////fec = DVBFE_FEC_1_2;
  ////break;
  ////case 2:
  ////fec = DVBFE_FEC_2_3;
  ////break;
  ////case 3:
  ////fec = DVBFE_FEC_3_4;
  ////break;
  ////// not used: fec = DVBFE_FEC_4_5;
  ////case 4:
  ////fec = DVBFE_FEC_5_6;
  ////break;
  ////// not used: fec = DVBFE_FEC_6_7;
  ////case 5:
  ////fec = DVBFE_FEC_7_8;
  ////break;
  ////case 6:
  ////fec = DVBFE_FEC_8_9;
  ////break;
  ////case 7:
  ////fec = DVBFE_FEC_AUTO; // no fec auto ?
  ////break;
  ////default:
  ////printf( "Warning: unknown fec %d\n", dx->fec_inner );
  ////break;
  ////}

  ////struct dvbcfg_scanfile info;
  ////info.fe_type                      = (dvbfe_type) ( dx->modulation_system == 1 ? Transponder::Type_DVB_S2 : Transponder::Type_DVB_S );
  ////info.fe_params.frequency          = dx->frequency;
  ////info.fe_params.inversion          = (dvbfe_spectral_inversion) DVBFE_INVERSION_AUTO;
  ////info.fe_params.u.dvbs.symbol_rate = dx->symbol_rate;
  ////info.fe_params.u.dvbs.fec_inner   = fec;
  ////info.polarization                 = pol;
  ////transponder->GetSource( ).CreateTransponder( &info, dx->modulation_type, dx->roll_off );

  //}
  //break;
  //case dtag_dvb_s2_satellite_delivery_descriptor:
  //{
  //printf( "Warning: dtag_dvb_s2_satellite_delivery_descriptor not implemented\n" );
  //struct dvb_s2_satellite_delivery_descriptor *dx = dvb_s2_satellite_delivery_descriptor_codec(curd);
  //if( dx == NULL )
  //continue;

  //}
  //break; ///FIXME: needs full implementation of libucsi
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

