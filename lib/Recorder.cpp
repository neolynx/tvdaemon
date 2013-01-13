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

#include <unistd.h> // sleep

Recorder::Recorder( ) : up(true)
{
  rec_thread = new Thread( *this, (ThreadFunc) &Recorder::Rec_Thread );
  rec_thread->Run( );
}

Recorder::~Recorder( )
{
  delete rec_thread;
  for( std::vector<Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
  {
    delete *it;
  }
}

bool Recorder::RecordNow( Channel &channel )
{
  Lock( );
  Activity_Record *act = new Activity_Record( channel );
  recordings.push_back( act );
  act->Start( );
  Unlock( );
  return true;
}


void Recorder::Rec_Thread( )
{
  while( up )
  {
    Lock( );
    for( std::vector<Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    {
      if( (*it)->HasState( Activity::State_Start ))
      {
        if( !(*it)->Start( ))
          LogError( "Error starting recording" );
      }
    }
    Unlock( );
    sleep( 1 );
  }
}

