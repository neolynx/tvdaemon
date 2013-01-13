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

#ifndef _Activity_
#define _Activity_

#include "Thread.h"

class Channel;
class Transponder;
class Service;
class Frontend;

class Activity : public ThreadBase
{
  public:
    Activity( Channel &channel );
    virtual ~Activity( );

    enum State
    {
      State_New,
      State_Start,
      State_Starting,
      State_Started,
      State_Done,
      State_Failed,
      State_Last
    };
    bool HasState( State state ) { return this->state == state; }
    State GetState( ) { return state; }
    void Schedule( ) { state = State_Start; }

    void SetTransponder( Transponder *transponder ) { this->transponder = transponder; }
    Transponder &GetTransponder( ) const { return *transponder; }

    void SetService( Service *service ) { this->service = service; }
    Service &GetService( ) const { return *service; }

    void SetFrontend( Frontend *frontend ) { this->frontend = frontend; }
    Frontend &GetFrontend( ) const { return *frontend; }

    bool Start( );

    bool IsActive( ) { return up; }

    virtual const char *GetName( ) const = 0;
    virtual bool Perform( ) = 0;

  protected:
    Channel &channel;
    State state;
    Transponder *transponder;
    Service *service;
    Frontend *frontend;

  private:
    void Activity_Thread( );
    Thread *activity_thread;
    bool up;
};


#endif
