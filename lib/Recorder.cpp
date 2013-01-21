/*
 *  tvdaemon
 *
 *  DVB Recorder class
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

#include "Recorder.h"

#include "Log.h"
#include "Activity_Record.h"
#include "Channel.h"
#include "TVDaemon.h"

#include <unistd.h> // sleep

Recorder::Recorder( TVDaemon &tvd ) :
  Thread( ),
  ConfigObject( ),
  tvd(tvd),
  up(true)
{
  std::string d = tvd.GetConfigDir( );
  d += "recorder/";
  SetConfigFile( d + "config" );
  StartThread( );
}

Recorder::~Recorder( )
{
  Log( "Stopping Recorder" );
  up = false;
  JoinThread( );
  Lock( );
  for( std::vector<Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
  {
    delete *it;
  }
  Unlock( );
}

bool Recorder::SaveConfig( )
{
  WriteConfig( "Directory", dir );
  WriteConfigFile( );

  Lock( );
  for( int i = 0; i < recordings.size( ); i++ )
  {
    recordings[i]->SaveConfig( );
  }
  Unlock( );
}

bool Recorder::LoadConfig( )
{
  if( !ReadConfigFile( ))
    return false;
  ReadConfig( "Directory", dir );
  if( dir.empty( ))
    dir = "~";
  if( !CreateFromConfig<Activity_Record, Recorder>( *this, "recording", recordings ))
    return false;
  return true;
}

bool Recorder::Schedule( Event &event )
{
  ScopeLock t;
  Activity_Record *act = new Activity_Record( *this, event, recordings.size( ));
  recordings.push_back( act );
  return true;
}

void Recorder::Run( )
{
  while( up )
  {
    Lock( );
    for( std::vector<Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    {
      switch( (*it)->GetState( ))
      {
        case Activity::State_Scheduled:
          if( time( NULL ) + 5 > (*it)->GetStart( ))
            (*it)->Start( );
          break;

        case Activity::State_Running:
          if( time( NULL ) > (*it)->GetEnd( ) + 60 )
            (*it)->Stop( );
          break;
      }
    }
    Unlock( );
    sleep( 1 );
  }
}

void Recorder::Stop( )
{
  ScopeLock _l;
  up = false;
  for( std::vector<Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    (*it)->Abort( );
}
