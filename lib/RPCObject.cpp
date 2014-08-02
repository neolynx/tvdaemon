/*
 *  tvdaemon
 *
 *  RPCObject class
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

#include <RPCObject.h>

#include <stdio.h> // snprintf

void json_object_time_add( json_object *j, std::string name, time_t tt )
{
  struct tm t, tnow;
  time_t now;
  time( &now );
  localtime_r( &tt, &t );
  localtime_r( &now, &tnow );
  json_object_object_add( j, ( name + "_sec" ).c_str( ),     json_object_new_int( t.tm_sec ));
  json_object_object_add( j, ( name + "_min" ).c_str( ),     json_object_new_int( t.tm_min ));
  json_object_object_add( j, ( name + "_hour" ).c_str( ),    json_object_new_int( t.tm_hour ));
  json_object_object_add( j, ( name + "_day" ).c_str( ),     json_object_new_int( t.tm_mday ));
  json_object_object_add( j, ( name + "_month" ).c_str( ),   json_object_new_int( t.tm_mon + 1 ));
  json_object_object_add( j, ( name + "_year" ).c_str( ),    json_object_new_int( t.tm_year ));
  json_object_object_add( j, ( name + "_wday" ).c_str( ),    json_object_new_int( t.tm_wday ));
  json_object_object_add( j, ( name + "_istoday" ).c_str( ), json_object_new_int( t.tm_mday == tnow.tm_mday &&
                                                                                  t.tm_mon  == tnow.tm_mon &&
                                                                                  t.tm_year == tnow.tm_year ));
  tnow.tm_mday++;
  mktime( &tnow );
  json_object_object_add( j, ( name + "_istomorrow" ).c_str( ), json_object_new_int( t.tm_mday == tnow.tm_mday &&
                                                                                  t.tm_mon  == tnow.tm_mon &&
                                                                                  t.tm_year == tnow.tm_year ));
}

void json_object_object_add( json_object *j, int key, json_object *k )
{
  char t[255];
  snprintf( t, sizeof( t ), "%d", key );
  json_object_object_add( j, t, k );
}
