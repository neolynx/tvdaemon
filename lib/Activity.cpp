/*
 *  tvdaemon
 *
 *  Activity class
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

#include "Activity.h"
#include "Log.h"
#include "Frontend.h"
#include "Channel.h"

#include <unistd.h> // NULL

Activity::Activity( ) : Thread( ), state(State_New), channel(NULL), service(NULL), transponder(NULL), frontend(NULL), up(true)
{
}

Activity::~Activity( )
{
  up = false;
  JoinThread( );
}

bool Activity::Start( )
{
  state = State_Starting;
  state_changed = time( NULL );
  StartThread( );
}

void Activity::Run( )
{
  bool ret;
  std::string name = GetName( );
  state = State_Running;
  state_changed = time( NULL );
  Log( "Activity starting: %s", name.c_str( ));
  if( channel )
  {
    if( !channel->Tune( *this ))
    {
      LogError( "Activity %s unable to start: tuning failed", name.c_str( ));
      goto fail;
    }
  }
  else if( frontend and port and transponder )
  {
    if( !frontend->Tune( *this ))
    {
      frontend->LogError( "Activity %s unable to start: tuning failed", name.c_str( ));
      goto fail;
    }
  }
  else
  {
    LogError( "Activity %s unable to start: no channel or no frontend, port and transponder found", name.c_str( ));
    goto fail;
  }

  ret = Perform( );
  if( state == State_Running )
  {
    if( ret )
      state = State_Done;
    else
      state = State_Failed;
    state_changed = time( NULL );
  }

  if( frontend )
    frontend->Release( );

  Log( "Activity %s %s", state == State_Done ? "done:    " : "failed:  ", name.c_str( ));
  return;

fail:
  state = State_Failed;
  state_changed = time( NULL );
  Failed( );
  return;
}

void Activity::Abort( )
{
  if( state == State_Running )
  {
    state = State_Aborted;
    state_changed = time( NULL );
  }
  up = false;
}

bool Activity::SetState( State s )
{
  state = s;
  state_changed = time( NULL );
  return true;
}
