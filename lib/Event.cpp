/*
 *  tvdaemon
 *
 *  Event class
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

#include "Event.h"
#include "Log.h"
#include "ConfigObject.h"
#include "Channel.h"
#include "RPCObject.h"

#include <sys/time.h>
#include <string.h>
#include <libdvbv5/eit.h>
#include <libdvbv5/desc_event_short.h>
#include <libdvbv5/desc_event_extended.h>

Event::Event( Channel &channel ) : channel(channel)
{
}

Event::Event( Channel &channel, const struct dvb_table_eit_event *event ) : channel(channel)
{
  if( !event )
  {
    LogError( "Event::Event event is NULL" ); //FIXME: load stuff in separate method
    return;
  }

  id = event->event_id;
  struct tm t;
  time_t now;

  time( &now );
  localtime_r( &now, &t );
//  printf( "local: %ld %d %s", t.tm_gmtoff, t.tm_isdst, asctime( &t ));
  //t.tm_isdst = 1; // dst in effect, do not adjust
  //time_t local = mktime( &t );

  double diff = t.tm_gmtoff; // + 3600; //difftime( utc, local );
  //printf( "diff: %f\n", diff );
  int gmt_offset = t.tm_gmtoff;

  t = event->start;
  mktime( &t );
//  printf( "event utc  : %s", asctime( &t ));

  //t.tm_isdst = 0;
  t.tm_hour += gmt_offset / 3600;

  start = mktime( &t );

  duration = event->duration;

  bool first = true;
  struct dvb_desc *desc = event->descriptor;
  while( desc )
  {
    switch( desc->type )
    {
      case short_event_descriptor:
        {
          struct dvb_desc_event_short *d = (struct dvb_desc_event_short *) desc;
          if( d->name ) name = d->name;
          if( d->language ) language = (char *) d->language;
          if( d->text ) description = d->text;
          if( description == name ) description = "";
        }
        break;
      case content_descriptor:
        break;
      case extended_event_descriptor:
        {
            struct dvb_desc_event_extended *d = (struct dvb_desc_event_extended *) desc;
            char id[255];
            snprintf(id, sizeof(id), "[%d/%d]", d->id, d->last_id);

            if( d->text ) description_extended += std::string(id) + d->text;
            //dvb_desc_default_print( NULL, desc );
            //Log( "Got dvb_desc_event_extended %s", description_extended.c_str());
            if (first and d->id != 0)
                LogError( "id not 0" );
        }
        break;
      default:
        //LogWarn( "event desc unhandled: %s", dvb_descriptors[desc->type].name );
        break;
    }
    desc = desc->next;
    if (first)
        first = false;
  }
//  printf( "name: %s\n\n", name.c_str( ));
  //LogWarn( "Event %s: %d.%d.%d %d:%d", name.c_str( ), t.tm_mday, t.tm_mon, t.tm_year, t.tm_hour, t.tm_min );
  //localtime_r( &start, &t );
  //LogError( "Check: %s", asctime( &t ));
}

Event::~Event( )
{
}

bool operator<( const Event &a, const Event &b )
{
  return difftime( a.start, b.start ) < 0.0;
}

time_t Event::GetStart( ) const
{
  return start;

  /* Summer:
   * utc  :  0 0 Sun Mar 30 15:01:10 2014
   * local: 7200 1 Sun Mar 30 17:01:10 2014
   * event utc  : Tue Apr  1 19:50:00 2014
   * event local: Tue Apr  1 21:50:00 2014
   * name: 10vor10
   */

  /* Winter 19h36
   * local: 3600 0 Mon Oct 27 19:36:48 2014
   * event utc  : Thu Oct 30 19:50:00 2014
   * name: 10vor10
   */
}

time_t Event::GetEnd( ) const
{
  struct tm t;
  localtime_r( &start, &t );
  t.tm_sec += duration;
  return mktime( &t );
}

bool Event::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "EventID",     id );
  config.WriteConfig( "Start",       start );
  config.WriteConfig( "Duration",    duration );
  config.WriteConfig( "Name",        name );
  config.WriteConfig( "Description", description );
  config.WriteConfig( "DescriptionExtended", description_extended );
  config.WriteConfig( "Language",    language );
}

bool Event::LoadConfig( ConfigBase &config )
{
  config.ReadConfig( "EventID",     id );
  config.ReadConfig( "Start",       start );
  config.ReadConfig( "Duration",    duration );
  config.ReadConfig( "Name",        name );
  config.ReadConfig( "Description", description );
  config.ReadConfig( "DescriptionExtended", description_extended );
  config.ReadConfig( "Language",    language );
}

void Event::json( json_object *entry ) const
{
  json_object_object_add( entry, "name",          json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "description",   json_object_new_string( description.c_str( )));
  json_object_object_add( entry, "description_extended",   json_object_new_string( description_extended.c_str( )));
  json_object_object_add( entry, "id",            json_object_new_int( id ));
  json_object_time_add  ( entry, "start",         start );
  json_object_object_add( entry, "duration",      json_object_new_int( duration ));
  json_object_object_add( entry, "channel",       json_object_new_string( channel.GetName( ).c_str( )));
  json_object_object_add( entry, "channel_id",    json_object_new_int( channel.GetKey( )));
}

bool Event::compare( const JSONObject &other, const int &p ) const
{
  const Event &b = (const Event &) other;
  double delta = difftime( start, b.start );
  if( delta < -0.9 || delta > 0.9 )
    return delta < 0.0;
  return channel.compare((const JSONObject &) b.channel, p );
}

