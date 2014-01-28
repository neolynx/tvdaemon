/*
 *  tvdaemon
 *
 *  Activity_Scan class
 *
 *  Copyright (C) 2013 André Roth
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

#include "Activity_Scan.h"
#include "Log.h"
#include "Transponder.h"
#include "Frontend.h"

#include <time.h>
#include <libdvbv5/dvb-fe.h> // FIXME: remove
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/pat.h>
#include <libdvbv5/pmt.h>
#include <libdvbv5/nit.h>
#include <libdvbv5/sdt.h>
#include <libdvbv5/vct.h>
#include <libdvbv5/cat.h>
#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/mpeg_pes.h>
#include <libdvbv5/desc_service.h>
#include <libdvbv5/desc_network_name.h>
#include <libdvbv5/desc_event_short.h>
#include <libdvbv5/desc_hierarchy.h>
#include <libdvbv5/desc_ca.h>
#include <vector>


Activity_Scan::Activity_Scan( ) : Activity( )
{
}

Activity_Scan::~Activity_Scan( )
{
}

std::string Activity_Scan::GetName( ) const
{
  std::string t = "Scan";
  if( transponder )
    t += " - " + transponder->toString( );
  return t;
}

bool Activity_Scan::Perform( )
{
  struct timeval tv;
  int ret;
  const char *err;
  int time = 5;
  std::vector<uint16_t> services;
  int fd_demux;
  struct dvb_table_pat *pat = NULL;

  if(( fd_demux = frontend->OpenDemux( )) < 0 )
  {
    frontend->LogError( "unable to open adapter demux" );
    goto open_failed;
  }

  transponder->SetState( Transponder::State_Scanning );

  frontend->Log( "Reading PAT" );
  dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_PAT, DVB_TABLE_PAT_PID, (uint8_t **) &pat, time );
  if( !pat )
  {
    frontend->LogError( "Error reading PAT table" );
    goto scan_failed;
  }
  dvb_table_pat_print( frontend->GetFE( ), pat );

  transponder->SetTSID( pat->header.id );
  if( transponder->GetState( ) == Transponder::State_Duplicate )
    goto scan_aborted;

  if( transponder->HasVCT( ))
  {
    frontend->Log( "Reading VCT" );
    struct atsc_table_vct *vct;
    dvb_read_section( frontend->GetFE( ), fd_demux, ATSC_TABLE_TVCT, ATSC_BASE_PID, (uint8_t **) &vct, time );
    if( vct && IsActive( ))
    {
      atsc_table_vct_print( frontend->GetFE( ), vct );
      atsc_vct_channel_foreach( ch, vct )
      {
        std::string name = ch->short_name;
        std::string provider = "unknown";
        Service::Type type = Service::Type_Unknown;
        switch( ch->service_type )
        {
          default:
            type = Service::Type_TV;
            break;
        }

        if( type == Service::Type_Unknown )
        {
          frontend->LogWarn( "  Service %5d: %s '%s': unknown type: %d", ch->program_number, ch->access_controlled ? "§" : " ", name.c_str( ), ch->service_type );
          continue;
        }

        transponder->UpdateService( ch->program_number, type, name, provider );
        services.push_back( ch->program_number );
      }
    }
    if( vct )
      atsc_table_vct_free( vct );
  }

  if( transponder->HasSDT( ))
  {
    frontend->Log( "Reading SDT" );
    struct dvb_table_sdt *sdt;
    dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_SDT, DVB_TABLE_SDT_PID, (uint8_t **) &sdt, time );
    if( sdt )
    {
      dvb_table_sdt_print( frontend->GetFE( ), sdt );
      dvb_sdt_service_foreach( service, sdt )
      {
        const char *name = "", *provider = "";
        int service_type = -1;
        dvb_desc_find( struct dvb_desc_service, desc, service, service_descriptor )
        {
          service_type = desc->service_type;
          if( desc->name )
            name = desc->name;
          if( desc->provider )
            provider = desc->provider;
          break;
        }
        if( service_type == -1 )
        {
          frontend->LogWarn( "  No service descriptor found for service %d", service->service_id );
          continue;
        }

        Service::Type type = Service::Type_Unknown;
        switch( service_type )
        {
          case 0x01:
          case 0x16:
            type = Service::Type_TV;
            break;
          case 0x02:
            type = Service::Type_Radio;
            break;
          case 0x19:
            type = Service::Type_TVHD;
            break;
          case 0x0c:
            // Data ignored
            continue;

        }

        if( type == Service::Type_Unknown )
        {
          frontend->LogWarn( "  Service %5d: %s '%s': unknown type: %d", service->service_id, service->free_CA_mode ? "§" : " ", name, service_type );
          continue;
        }

        frontend->Log( "  Service %5d: %s %-6s '%s'", service->service_id, service->free_CA_mode ? "§" : " ", Service::GetTypeName( type ), name );
        transponder->UpdateService( service->service_id, type, name, provider );
        services.push_back( service->service_id );
      }

      dvb_table_sdt_free( sdt );
    }
  }

  frontend->Log( "Reading PMT's" );
  dvb_pat_program_foreach( program, pat )
  {
    for( std::vector<uint16_t>::iterator it = services.begin( ); it != services.end( ); it++ )
    {
      if( !IsActive( ))
        break;
      if( *it == program->service_id )
      {
        if( program->service_id == 0 )
        {
          frontend->LogWarn( "  Ignoring PMT of service 0" );
          break;
        }

        transponder->UpdateProgram( program->service_id, program->pid );

        frontend->Log( "Reading PMT %04x", program->pid );
        struct dvb_table_pmt *pmt;
        dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_PMT, program->pid, (uint8_t **) &pmt, time );
        if( !pmt )
        {
          frontend->LogWarn( "  No PMT for pid %d", program->pid );
          break;
        }

        dvb_desc_find( struct dvb_desc_ca, desc, pmt, conditional_access_descriptor )
        {
          transponder->SetCA( program->service_id, desc->ca_id, desc->ca_pid );
        }

        dvb_table_pmt_print( frontend->GetFE( ), pmt );

        dvb_pmt_stream_foreach( stream, pmt )
        {
          if( !IsActive( ))
            break;
          Stream::Type type = Stream::Type_Unknown;
          switch( stream->type )
          {
            case stream_video:            // 0x01
              type = Stream::Type_Video;
              break;
            case stream_video_h262:       // 0x02
              type = Stream::Type_Video_H262;
              break;
            case 0x1b:                    // 0x1b H.264 AVC
              type = Stream::Type_Video_H264;
              break;

            case stream_audio:            // 0x03
              type = Stream::Type_Audio;
              break;
            case stream_audio_13818_3:    // 0x04
              type = Stream::Type_Audio_13818_3;
              break;
            case stream_audio_adts:       // 0x0F
              type = Stream::Type_Audio_ADTS;
              break;
            case stream_audio_latm:       // 0x11
              type = Stream::Type_Audio_LATM;
              break;
            case stream_private + 1:      // 0x81  user private - in general ATSC Dolby - AC-3
              type = Stream::Type_Audio_AC3;
              break;

            case stream_private_sections: // 0x05
            case stream_private_data:     // 0x06
              dvb_desc_find( struct dvb_desc, desc, stream, AC_3_descriptor )
              {
                type = Stream::Type_Audio_AC3;
                break;
              }
              dvb_desc_find( struct dvb_desc, desc, stream, enhanced_AC_3_descriptor )
              {
                type = Stream::Type_Audio_AC3;
                frontend->LogWarn( "  Found AC3 enhanced" );
                break;
              }
              break;

            default:
              frontend->LogWarn( "  Ignoring stream type %d: %s", stream->type, pmt_stream_name[stream->type] );
              break;
          }
          if( type != Stream::Type_Unknown )
            transponder->UpdateStream( program->service_id, stream->elementary_pid, type );
        }

        dvb_table_pmt_free( pmt );
        break;
      }
    }
  }
  if( pat )
    dvb_table_pat_free( pat );

  if( IsActive( ) && transponder->HasNIT( ))
  {
    frontend->Log( "Reading NIT" );
    struct dvb_table_nit *nit;
    dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_NIT, DVB_TABLE_NIT_PID, (uint8_t **) &nit, time );
    if( nit )
    {
      dvb_desc_find( struct dvb_desc_network_name, desc, nit, network_name_descriptor )
      {
        frontend->Log( "  Network Name: %s", desc->network_name );
        //transponder->SetNetwork( desc->network_name );
        break;
      }
      dvb_table_nit_print( frontend->GetFE( ), nit );
      frontend->HandleNIT( nit );
    }
    if( nit )
      dvb_table_nit_free( nit );
  }


  if( IsActive( ) ) // FIXME: && transponder->HasNIT( ))
  {
    frontend->Log( "Reading CAT" );
    struct dvb_table_cat *cat;
    dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_CAT, DVB_TABLE_CAT_PID, (uint8_t **) &cat, time );
    if( cat )
    {
      dvb_desc_find( struct dvb_desc_ca, desc, cat, conditional_access_descriptor )
      {
        transponder->SetCA( desc->ca_id, desc->ca_pid );
      }
      dvb_table_cat_print( frontend->GetFE( ), cat );
      dvb_table_cat_free( cat );
    }
  }

  frontend->CloseDemux( fd_demux );
  transponder->SetState( Transponder::State_Scanned );
  transponder->SaveConfig( );
  return true;

scan_failed:
  transponder->SetState( Transponder::State_ScanningFailed );
  transponder->SaveConfig( );
scan_aborted:
  frontend->CloseDemux( fd_demux );
open_failed:
  return false;
}

