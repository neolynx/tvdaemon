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

#ifndef _Recorder_
#define _Recorder_

//#include <stdint.h> // uint64_t

#include <map>

#include "Thread.h"
#include "ConfigObject.h"
#include "RPCObject.h"
//class Matroska;
//class RingBuffer;
//class Frame;

class Transponder;
class Service;
class Activity_Record;
class Channel;
class Event;
class TVDaemon;
class JSONObject;

class Recorder : public Thread, public ConfigObject, public RPCHandler
{
  public:
    static Recorder *Instance( );
    ~Recorder( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    bool Schedule( Event &event );
    void Record( Channel &channel );
    void Stop( );
    bool Remove( int id );

    std::string GetDir( ) const { return dir; }

    //void AddTrack( );
    //void record( uint8_t *data, int size );

    void GetRecordings( std::vector<const JSONObject *> &result ) const;
    Activity_Record *GetRecording( int id );

    bool RPC( const HTTPRequest &request, const std::string &cat, const std::string &action );

  private:
    Recorder( );
    //struct dvb_v5_fe_parms &fe;
    //Matroska *mkv;

    //RingBuffer *buffer;
    //Frame *frame;
    bool up;
    std::string dir;
    std::map<int, Activity_Record *> recordings;

    virtual void Run( );
};

#endif
