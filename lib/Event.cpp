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
#include "descriptors/eit.h"
#include "descriptors/desc_event_short.h"

Event::Event( Channel &channel, const struct dvb_table_eit_event *event ) : channel(channel)
{
  if( !event )
    return;

  id = event->event_id;

  struct tm t;

  time_t now;
  time( &now );
  gmtime_r( &now, &t );
  //printf( "utc: %s\n", asctime( &t ));
  t.tm_isdst = 1; // dst in effect, do not adjust
  time_t utc = mktime( &t );

  time( &now );
  localtime_r( &now, &t );
  //printf( "local: %s\n", asctime( &t ));
  t.tm_isdst = 1; // dst in effect, do not adjust
  time_t local = mktime( &t );

  double diff = difftime( utc, local );
  //printf( "diff: %f\n", diff );

  t = event->start;
  mktime( &t );
  //printf( "event utc: %s\n", asctime( &t ));
  t.tm_hour  -= (int)( diff / 3600.0 );
  start = mktime( &t );
  //printf( "event local: %s\n", asctime( &t ));

  duration = event->duration;

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
        //dvb_desc_default_print( NULL, desc );
        break;
      default:
        //LogWarn( "event desc unhandled: %s", dvb_descriptors[desc->type].name );
        break;
    }
    desc = desc->next;
  }
}

Event::~Event( )
{
}

bool operator<( const Event &a, const Event &b )
{
  return difftime( a.start, b.start ) < 0.0;
}

bool Event::SaveConfig( ConfigBase &config )
{
  config.WriteConfig( "EventID",     id );
  config.WriteConfig( "Start",       start );
  config.WriteConfig( "Duration",    duration );
  config.WriteConfig( "Name",        name );
  config.WriteConfig( "Description", description );
  config.WriteConfig( "Language",    language );
}

bool Event::LoadConfig( ConfigBase &config )
{
  config.ReadConfig( "EventID",     id );
  config.ReadConfig( "Start",       start );
  config.ReadConfig( "Duration",    duration );
  config.ReadConfig( "Name",        name );
  config.ReadConfig( "Description", description );
  config.ReadConfig( "Language",    language );
}

void Event::json( json_object *entry ) const
{
  json_object_object_add( entry, "name",          json_object_new_string( name.c_str( )));
  json_object_object_add( entry, "description",   json_object_new_string( description.c_str( )));
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

