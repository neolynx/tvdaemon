/*
 *  tvdaemon
 *
 *  Thread class
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

#ifndef _Thread_
#define _Thread_

#include <pthread.h>

class ThreadBase;
typedef void (ThreadBase::*ThreadFunc)( );

class ThreadBase
{
  public:
    ThreadBase( );
    void Lock( ) const;
    void Unlock( ) const;

  protected:
    class Thread
    {
      public:
        Thread( ThreadBase &base, ThreadFunc func );
        bool Run( );
        virtual ~Thread( );

      private:
        ThreadBase &base;
        ThreadFunc func;
        pthread_t thread;
        static void *run( void *ptr );
    };
  private:
    volatile pthread_mutex_t mutex;
};


#endif
