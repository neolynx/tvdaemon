/*
 *  tvdaemon
 *
 *  Activity_UpdateEPG class
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

#include "Activity_UpdateEPG.h"

#include "Log.h"
#include "Frontend.h"
#include "Channel.h"

#include <libdvbv5/eit.h>
#include <libdvbv5/mgt.h>
#include <libdvbv5/atsc_eit.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-demux.h>

Activity_UpdateEPG::Activity_UpdateEPG( ) : Activity( )
{
}

Activity_UpdateEPG::~Activity_UpdateEPG( )
{
}

std::string Activity_UpdateEPG::GetName( ) const
{
  std::string t = "Update EPG";
  if( transponder )
    t += " - " + transponder->toString( );
  return t;
}

bool Activity_UpdateEPG::Perform( )
{
  int timeout = 2; // seconds
  int fd_demux;

  if(( fd_demux = frontend->OpenDemux( )) < 0 )
  {
    frontend->LogError( "unable to open adapter demux" );
    goto fail;
  }
  if( transponder->HasMGT( ))
  {
    frontend->Log( "Reading MGT" );
    struct atsc_table_mgt *mgt = NULL;
    dvb_read_section( frontend->GetFE( ), fd_demux, ATSC_TABLE_MGT, ATSC_BASE_PID, (uint8_t **) &mgt, timeout );
    if( mgt )
    {
      atsc_table_mgt_print( frontend->GetFE( ), mgt );
      const struct atsc_table_mgt_table *table = mgt->table;
      while( table )
      {
        switch( table->type )
        {
          case 0x100 ... 0x17F: // EIT
            {
              frontend->Log( "Reading EIT %d\t(pid: %d)", table->type - 0x100, table->pid );
              struct atsc_table_eit *eit = NULL;
              dvb_read_section( frontend->GetFE( ), fd_demux, ATSC_TABLE_EIT, table->pid, (uint8_t **) &eit, timeout );
              if( eit )
              {
                //transponder->ReadEPG( eit->event );
                atsc_table_eit_print( frontend->GetFE( ), eit );
                atsc_table_eit_free( eit );
              }

            }
            break;
          case 0x200 ... 0x27F: // ETT
            {
              frontend->LogWarn( "Reading ETT %d\t(pid: %d)", table->type - 0x200, table->pid );
              //struct dvb_table_eit *eit = NULL;
              //dvb_read_section( frontend->GetFE( ), fd_demux, 0xCB, table->pid, (uint8_t **) &eit, timeout );
              //if( eit )
              //{
              //}
            }
            break;
        }
        table = table->next;
      }
    }
    frontend->CloseDemux( fd_demux );
    return true;
  }
  else // no MGT
  {
    frontend->Log( "Reading EIT" );
    struct dvb_table_eit *eit = NULL;
    dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_EIT_SCHEDULE, DVB_TABLE_EIT_PID, (uint8_t **) &eit, timeout );
    if( eit )
    {
      transponder->ReadEPG( eit->event );
      dvb_table_eit_free( eit );
    }
    else
    {
      frontend->Log( "Reading EIT now/next" );
      dvb_read_section( frontend->GetFE( ), fd_demux, DVB_TABLE_EIT, DVB_TABLE_EIT_PID, (uint8_t **) &eit, timeout );
      if( eit )
      {
        transponder->ReadEPG( eit->event );
        dvb_table_eit_free( eit );
      }
      else
        transponder->SetEPGState( Transponder::EPGState_NotAvailable );
    }

    frontend->CloseDemux( fd_demux );
    return eit != NULL;
  }

fail:
  return false;
}

void Activity_UpdateEPG::Failed( )
{
  if( transponder )
    transponder->SetEPGState( Transponder::EPGState_Missing );
}

