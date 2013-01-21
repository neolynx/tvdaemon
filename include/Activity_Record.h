/*
 *  tvdaemon
 *
 *  Activity_Record class
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

#ifndef _Activity_Record_
#define _Activity_Record_

#include "Activity.h"
#include "ConfigObject.h"

class Event;
class Recorder;

class Activity_Record : public Activity, public ConfigObject
{
  public:
    Activity_Record( Recorder &recorder, Event &event, int config_id );
    Activity_Record( Recorder &recorder, std::string configfile );
    virtual ~Activity_Record( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    virtual std::string GetName( ) const;

    virtual bool Perform( );

    time_t GetStart( )          { return start; }
    time_t GetEnd( )            { return end; }

  private:
    Recorder &recorder;
    time_t start, end;
    std::string name;
};


#endif
