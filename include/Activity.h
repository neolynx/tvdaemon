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

#include <string>

class Channel;
class Transponder;
class Service;
class Frontend;
class Port;

class Activity : public Thread
{
  public:
    Activity( );
    virtual ~Activity( );

    enum State
    {
      State_New,
      State_Scheduled,
      State_Starting,
      State_Running,
      State_Done,
      State_Aborted,
      State_Failed,
      State_Missed,
      State_Last
    };
    bool HasState( State state ) const { return this->state == state; }
    bool SetState( State state );
    State GetState( ) const { return state; }

    void SetChannel( Channel *channel ) { this->channel = channel; }
    Channel *GetChannel( ) const { return channel; }

    void SetService( Service *service ) { this->service = service; }
    Service *GetService( ) const { return service; }

    void SetTransponder( Transponder *transponder ) { this->transponder = transponder; }
    Transponder *GetTransponder( ) const { return transponder; }

    void SetFrontend( Frontend *frontend ) { this->frontend = frontend; }
    Frontend *GetFrontend( ) const { return frontend; }

    void SetPort( Port *port ) { this->port = port; }
    Port *GetPort( ) const { return port; }

    time_t GetStateChanged( ) const { return state_changed; }

    bool Start( );
    void Stop( ) { up = false; }
    void Abort( );
    bool IsActive( ) { return up; }

    virtual std::string GetName( ) const = 0;
    virtual bool Perform( ) = 0;
    virtual void Failed( ) = 0;

    void Run( );

  protected:
    Channel *channel;
    Service *service;
    Transponder *transponder;
    Frontend *frontend;
    Port *port;


  private:
    State state;
    time_t state_changed;

    bool up;
};


#endif
