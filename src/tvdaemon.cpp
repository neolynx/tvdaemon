/*
 *  tvdaemon
 *
 *  TVDaemon main
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

#include <signal.h>

#include "TVDaemon.h"
#include "Channel.h"
#include "Adapter.h"
#include "Frontend.h"
#include "Source.h"
#include "Transponder.h"
#include "Service.h"
#include "Log.h"

#include <stdio.h>

bool up = true;

void termination_handler( int signum )
{
  if( up )
  {
    printf( "\nSignal received, terminating ...\n" );
    up = false;
  }
}

int create_stuff( TVDaemon &tvd )
{
  // Test tuning/scanning
  int source_id = 1;
  int adapter_id = 0;
  int frontend_id = 0;
  int port_id = 0;
  int transponder_id = 4;
  int service_id = 2;

  // sat hotbird
  source_id = 2;
  adapter_id = 1;
  frontend_id = 0;
  port_id = 1;
  transponder_id = 17;
  service_id = 8004;

  // sat astra
  source_id = 0;
  adapter_id = 1;
  frontend_id = 0;
  port_id = 0;
  transponder_id = 16;
  service_id = 20006;

  Source *s = tvd.GetSource( source_id );
  if( s == NULL )
  {
    printf( "Source %d not found\n", source_id );
    return -1;
  }
  printf( "Got Source %s\n", s->GetName( ).c_str( ));

  Adapter *a = tvd.GetAdapter( adapter_id );
  if( !a )
  {
    printf( "Adapter %d not found\n", adapter_id );
    return -1;
  }
  printf( "Got Adapter %d: %s\n", adapter_id, a->GetName( ).c_str( ));

  Frontend *f = a->GetFrontend( frontend_id );
  if( !f )
  {
    printf( "Frontend %d not found\n", frontend_id );
    return -1;
  }
  printf( "Got Frontend %d\n", frontend_id );

  if( port_id != 0 )
    f->AddPort( "Hotbird", port_id );
  s->AddPort( f->GetPort( port_id ));

  int count = 0, total = 0;
  if( transponder_id == -1 )
  {
    for( int id = 0; up && id < s->GetTransponderCount(); ++id )
    {
      if( s->GetTransponder( id )->Disabled( ))
      {
        printf( "Transponder %d/%d disabled\n", id + 1, s->GetTransponderCount( ));
        continue;
      }
      if( s->GetTransponder( id )->GetState( ) == Transponder::State_Scanned )
      {
        printf( "Transponder %d/%d already scanned\n", id + 1, s->GetTransponderCount( ));
        continue;
      }
      printf( "Scanning Transponder %d/%d\n", id + 1, s->GetTransponderCount( ));
      if( !s->ScanTransponder( id ))
        printf( "Scan failed\n\n" );
      else
        count++;
      total++;
    }
    printf( "Scanned %d/%d transponder%s\n", count, total, count == 1 ? "" : "s" );
    return 0;
  }

  int id = transponder_id;
  Transponder *t = s->GetTransponder( id );
  if( !t )
  {
    printf( "Error: transponder %d not found\n", transponder_id );
    return -1;
  }
  if( t->Disabled( ))
  {
    printf( "Transponder %d/%d disabled\n", id + 1, s->GetTransponderCount( ));
    return 0;
  }
  if( service_id == -1 || t->GetState( ) != Transponder::State_Scanned )
  {
    printf( "Scanning Transponder %d (state %d) ...\n", transponder_id, t->GetState( ));
    total++;
    if( !s->ScanTransponder( id ))
    {
      printf( "Scanning failed\n" );
      return 0;
    }
  }

  if( service_id != -1 )
  {
    Service *v = t->GetService( service_id );
    if( !v )
    {
      printf( "no channel to tune\n" );
      return 0;
    }

    if( !v->Tune( ))
    {
      printf( "tuning service failed\n" );
      return 0;
    }

    f->Untune();
  }
  return 0;
}

int main( int argc, char *argv[] )
{
  struct sigaction action;
  action.sa_handler = termination_handler;
  sigemptyset( &action.sa_mask );
  action.sa_flags = 0;
  sigaction( SIGINT, &action, NULL );
  sigaction( SIGTERM, &action, NULL );

  Log( "TVDaemon starting ...\n" );

  TVDaemon *tvd = TVDaemon::Instance( );
  if( !tvd->Create( "~/.tvdaemon" ))
  {
    LogError( "Unable to create tvdaemon\n" );
    return -1;
  }
  if( !tvd->Start( ))
  {
    LogError( "Unable to start tvdaemon\n" );
    return -1;
  }

  std::vector<std::string> list;// = tvd.GetScanfileList( TVDaemon::Source_DVB_C, "ch" );
  //printf( "\nKnown transponder lists for DVB-C in .ch:\n" );
  //for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
  //printf( "  %s\n", it->c_str( ));
  //printf( "\n" );
  //list = tvd.GetScanfileList( TVDaemon::Source_DVB_T, "ch" );
  //printf( "\nKnown transponder lists for DVB-T in .ch:\n" );
  //for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
  //printf( "  %s\n", it->c_str( ));
  //printf( "\n" );
  //list = tvd.GetScanfileList( TVDaemon::Source_DVB_S, "ch" );
  //printf( "\nKnown transponder lists for DVB-S in .ch:\n" );
  //for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
  //printf( "  %s\n", it->c_str( ));
  //printf( "\n" );

  //list = tvd->GetAdapterList( );
  //printf( "\nFound Adapters:\n" );
  //for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    //printf( "  %s\n", it->c_str( ));
  //printf( "\n" );

  //list = tvd->GetSourceList( );
  //printf( "\nFound Sources:\n" );
  //for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    //printf( "  %s\n", it->c_str( ));
  //printf( "\n" );

  //list = tvd->GetChannelList( );
  //printf( "\nFound Channels:\n" );
  //for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    //printf( "  %s\n", it->c_str( ));
  //printf( "\n" );

  create_stuff( *tvd );

  while( up )
  {
    sleep( 1 );
  }
  delete tvd;
  return 0;
}

