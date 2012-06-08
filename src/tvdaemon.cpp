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
  tvd.CreateSource( "DVB-T", "dvb-t/ch-All" );
  tvd.CreateSource( "Astra 19.2E", "dvb-s/Astra-19.2E" );

  printf( "-------------------------\n" );
  // Test tuning/scanning
  int source_id = 0;
  int adapter_id = 2;
  int frontend_id = 0;
  int transponder_id = 4; // -1 for all

  // sat
  source_id = 1;
  adapter_id = 0;
  frontend_id = 0;
  transponder_id = -1;

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
  printf( "Got Adapter %s\n", a->GetName( ).c_str( ));

  Frontend *f = a->GetFrontend( frontend_id );
  if( !f )
  {
    printf( "Frontend %d not found\n", frontend_id );
    return -1;
  }
  printf( "Got Frontend %d\n", frontend_id );

  s->AddPort( f->GetPort( 0 ));

  int count = 0;
  if( transponder_id == -1 )
  {
    for( uint nCount = 0; up && nCount < s->GetTransponderCount(); ++nCount )
    {
      printf( "Scanning Transponder %d/%d ...\n", nCount, s->GetTransponderCount( ));
      if( !s->ScanTransponder( nCount ))
        printf( "Scanning failed\n" );
      count++;
    }
  }
  else
  {
    printf( "Scanning Transponder %d ...\n", transponder_id );
    count++;
    if( !s->ScanTransponder( transponder_id ))
      printf( "Scanning failed\n" );
    else
    {
      Transponder *t = s->GetTransponder( transponder_id );
      if( !t )
      {
        printf( "Error: transponder %d not found\n", transponder_id );
        return -1;
      }
      Service *v = t->GetService( 901 );
      if( !v )
        printf( "no channel to tune\n" );
      else
        if( !v->Tune( ))
          printf( "tuning service failed\n" );
        else
          f->Untune();
    }
  }
  printf( "Scanned %d transponder%s\n", count, count == 1 ? "" : "s" );
}

int main( int argc, char *argv[] )
{
  struct sigaction action;
  action.sa_handler = termination_handler;
  sigemptyset( &action.sa_mask );
  action.sa_flags = 0;
  sigaction( SIGINT, &action, NULL );
  sigaction( SIGTERM, &action, NULL );

  printf( "TVDaemon starting ...\n" );

  TVDaemon tvd( "~/.tvdaemon" );
  if( !tvd.Start( ))
  {
    printf( "Unable to start tvdaemon\n" );
    return -1;
  }

  std::vector<std::string> list = tvd.GetScanfileList( TVDaemon::Source_DVB_C, "ch" );
  printf( "\nKnown transponder lists for DVB-C in .ch:\n" );
  for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    printf( "  %s\n", it->c_str( ));
  printf( "\n" );
  list = tvd.GetScanfileList( TVDaemon::Source_DVB_T, "ch" );
  printf( "\nKnown transponder lists for DVB-T in .ch:\n" );
  for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    printf( "  %s\n", it->c_str( ));
  printf( "\n" );
  list = tvd.GetScanfileList( TVDaemon::Source_DVB_S, "ch" );
  printf( "\nKnown transponder lists for DVB-S in .ch:\n" );
  for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    printf( "  %s\n", it->c_str( ));
  printf( "\n" );

  list = tvd.GetAdapterList( );
  printf( "\nFound Adapters:\n" );
  for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    printf( "  %s\n", it->c_str( ));
  printf( "\n" );

  list = tvd.GetSourceList( );
  printf( "\nFound Sources:\n" );
  for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    printf( "  %s\n", it->c_str( ));
  printf( "\n" );

  list = tvd.GetChannelList( );
  printf( "\nFound Channels:\n" );
  for( std::vector<std::string>::iterator it = list.begin( ); it != list.end( ); it++ )
    printf( "  %s\n", it->c_str( ));
  printf( "\n" );

  create_stuff( tvd );

  while( up )
  {
    sleep( 1 );
  }
  return 0;
}

