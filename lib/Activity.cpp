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

Activity::Activity( ) : Thread( ), state(state), channel(NULL), service(NULL), transponder(NULL), frontend(NULL), up(true)
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
  StartThread( );
}

void Activity::Run( )
{
  const char *name = GetName( );
  std::string context;
  state = State_Started;
  if( channel )
  {
    context = channel->GetName( );
    Log( "Activity started: %s( %s )", name, context.c_str( ));
    if( !channel->Tune( *this ))
    {
      Log( "Activity failed:  %s( %s ) failed", name, context.c_str( ));
      goto fail;
    }
  }
  else if( transponder )
  {
    context = transponder->toString( );
    Log( "Activity started: %s( %s )", name, context.c_str( ));
    if( !transponder->Tune( *this ))
    {
      Log( "Activity failed:  %s( %s )", name, context.c_str( ));
      goto fail;
    }
  }
  else
  {
    LogError( "Activity %s unable to start: no channel or transponder found", name );
    goto fail;
  }

  if( !Perform( ))
    state = State_Failed;
  else
    state = State_Done;

  if( frontend )
    frontend->Release( );

  Log( "Activity %s %s( %s )", state == State_Done ? "done:    " : "failed:  ", name, context.c_str( ));
  return;

fail:
  state = State_Failed;
  return;
}

