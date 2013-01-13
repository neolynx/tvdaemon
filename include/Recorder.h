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

#include <vector>

#include "Thread.h"
//class Matroska;
//class RingBuffer;
//class Frame;

class Transponder;
class Service;
class Activity_Record;
class Channel;

class Recorder : public ThreadBase
{
  public:
    //Recorder( struct dvb_v5_fe_parms &fe );
    Recorder( );
    ~Recorder( );

    bool RecordNow( Channel &channel );


    //void AddTrack( );
    //void record( uint8_t *data, int size );



  private:
    //struct dvb_v5_fe_parms &fe;
    //Matroska *mkv;

    //RingBuffer *buffer;
    //Frame *frame;
    bool up;
    std::vector<Activity_Record *> recordings;
    Thread *rec_thread;

    void Rec_Thread( );
};

#endif
