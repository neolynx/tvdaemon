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
#include <algorithm> // sort

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
  up = false;
  JoinThread( );
  Lock( );
  for( std::map<int, Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    delete it->second;
  recordings.clear( );
  Unlock( );
}

bool Recorder::SaveConfig( )
{
  WriteConfig( "Directory", dir );
  WriteConfigFile( );

  Lock( );
  for( std::map<int, Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
  {
    it->second->SaveConfig( ); // FIXME only save events, not channels. maybe save in ActivityRecord on create/start/stop
  }
  Unlock( );
}

bool Recorder::LoadConfig( )
{
  if( !ReadConfigFile( ))
  {
    dir = "~";
    return false;
  }
  ReadConfig( "Directory", dir );
  if( dir.empty( ))
    dir = "~";
  Log( "Recorder directoy: '%s'", dir.c_str( ));

  Lock( );
  bool ret = CreateFromConfig<Activity_Record, int, Recorder>( *this, "recording", recordings );
  Unlock( );
  return ret;
}

bool Recorder::Schedule( Event &event )
{
  SCOPELOCK( );
  for( std::map<int, Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    if( it->second->GetEventID( ) == event.GetID( ) and
        it->second->GetStart( ) == event.GetStart( ) and
        it->second->GetChannel( ) == &event.GetChannel( ))
    {
      LogError( "Recorder: Event %d already scheduled", event.GetID( ));
      return false;
    }
  int next_id = GetAvailableKey<Activity_Record, int>( recordings );
  recordings[next_id] = new Activity_Record( *this, event, next_id );
  recordings[next_id]->SaveConfig( );
  Log( "Recorder: event '%s' on '%s' scheduled", event.GetName( ).c_str( ), event.GetChannel( ).GetName( ).c_str( ));
  return true;
}

void Recorder::Record( Channel &channel )
{
  SCOPELOCK( );
  int next_id = GetAvailableKey<Activity_Record, int>( recordings );
  recordings[next_id] = new Activity_Record( *this, channel, next_id );
  Log( "Recorder: recording channel %s", channel.GetName( ).c_str( ));
}

void Recorder::Run( )
{
  double before = 5;
  double after = 5 * 60;
  double retry_interval = 5.0;
  while( up )
  {
    time_t now = time( NULL );
    Lock( );
    for( std::map<int, Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    {
      time_t start = it->second->GetStart( );
      time_t end   = it->second->GetEnd( );
      switch( it->second->GetState( ))
      {
        case Activity::State_Scheduled:
          if( difftime( start, now ) <= before &&
              difftime( now, end ) < -5.0 + after )
            it->second->Start( );
          break;

        case Activity::State_Running:
          if( difftime( now, end ) >= after )
            it->second->Stop( );
          break;

        case Activity::State_Failed:
          if( difftime( start, now ) <= before &&
              difftime( now, end ) < -5.0 + after )
          {
            if( difftime( now, it->second->GetStateChanged( )) >= retry_interval )
              it->second->Start( );
          }
          break;
      }
    }
    Unlock( );
    sleep( 1 );
  }
}

void Recorder::Stop( )
{
  SCOPELOCK( );
  up = false;
  for( std::map<int, Activity_Record *>::iterator it = recordings.begin( ); it != recordings.end( ); it++ )
    it->second->Abort( );
}

void Recorder::GetRecordings( std::vector<const JSONObject *> &data ) const
{
  SCOPELOCK( );
  for( std::map<int, Activity_Record *>::const_iterator it = recordings.begin( ); it != recordings.end( ); it++ )
  {
    data.push_back( it->second ); // FIXME: should not return pointers !!!
  }
}

bool Recorder::RPC( const HTTPRequest &request,  const std::string &cat, const std::string &action )
{
  if( action == "remove" )
  {
    int id;
    if( !request.GetParam( "id", id ))
      return false;
    if( !Remove( id ))
    {
      request.NotFound( "Recorder: unknown recording %d", id );
      return false;
    }
    request.Reply( HTTP_OK );
    return true;
  }

  request.NotFound( "RPC: unknown action: '%s'", action.c_str( ));
  return false;
}

bool Recorder::Remove( int id )
{
  SCOPELOCK( );
  std::map<int, Activity_Record *>::iterator it = recordings.find( id );
  if( it == recordings.end( ))
    return false;
  it->second->RemoveConfigFile( );
  delete it->second;
  recordings.erase( it );
  Log( "Recorder: recording %d removed", id );
  return true;
}
