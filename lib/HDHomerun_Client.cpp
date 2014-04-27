/*
 *  tvdaemon
 *
 *  HDHomerun_Client class
 *
 *  Copyright (C) 2014 Lars Schmohl
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

#include "HDHomerun_Client.h"

#include "TVDaemon.h"

#ifdef HAVE_LIBHDHOMERUN

HDHomerun_Client::HDHomerun_Client( ) :
    Thread( ), up( false ), discover_obj( NULL )
{
}

HDHomerun_Client::~HDHomerun_Client( )
{
  up = false;
  JoinThread( );

  if( discover_obj )
    hdhomerun_discover_destroy( discover_obj );
}

bool HDHomerun_Client::Start( )
{
  if( NULL == discover_obj )
    discover_obj = hdhomerun_discover_create( NULL );
  if( NULL == discover_obj )
  {
    LogError( "HDHomerun: unable to create discovery object" );
    return false;
  }
  up = true;
  return StartThread();
}

void HDHomerun_Client::Run( )
{
  int index, dev_count, tuner;
  bool foundDevice;
  char adapter[32], frontend[32];
  while( up )
  {
    std::list<uint32_t> remove_list( current_devices );

    dev_count = hdhomerun_discover_find_devices( discover_obj, 0,
                                                 HDHOMERUN_DEVICE_TYPE_TUNER,
                                                 HDHOMERUN_DEVICE_ID_WILDCARD, discover_dev,
                                                 HDHOMERUN_DISOCVER_MAX_SOCK_COUNT );

    foundDevice = false;
    for( index = 0; index < dev_count; index++ )
    {
      remove_list.remove( discover_dev[index].device_id );

      if( find( current_devices.begin( ), current_devices.end( ), discover_dev[index].device_id )
          != current_devices.end( ) )
        continue;
      current_devices.push_back( discover_dev[index].device_id );

      sprintf( adapter, "HDHR %d", discover_dev[index].device_id );
      for( tuner = 0; tuner < discover_dev[index].tuner_count; tuner++ )
      {
        sprintf( frontend, "Tuner %d", tuner );
        TVDaemon::Instance( )->AddFrontendToList( adapter, frontend, adapter, discover_dev[index].device_id, tuner );
      }
      foundDevice = true;
    }
    if( foundDevice )
      TVDaemon::Instance( )->ProcessAdapters( );

    while( !remove_list.empty( ) )
    {
      sprintf( adapter, "HDHR %d", remove_list.front( ));
      current_devices.remove( remove_list.front( ));
      remove_list.pop_front( );
      TVDaemon::Instance()->RemoveFrontend( adapter );
    }

    for( index = 0; index < 10; index++ )
    {
      sleep( 1 );
      if( !up )
        break;
    }
  }
}

#endif /* HAVE_LIBHDHOMERUN */
