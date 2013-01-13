/*
 *  tvdaemon
 *
 *  Thread class
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

#include "Thread.h"

#include "Log.h"

Lockable::Lockable( )
{
  pthread_mutex_init( (pthread_mutex_t *) &mutex, NULL );
}

void Lockable::Lock( ) const
{
  pthread_mutex_lock( (pthread_mutex_t *) &mutex );
}

void Lockable::Unlock( ) const
{
  pthread_mutex_unlock( (pthread_mutex_t *) &mutex );
}

Thread::Thread( ) : Lockable( ), started(false)
{
}

Thread::~Thread( )
{
}

void Thread::JoinThread( )
{
  if( started )
    pthread_join( thread, NULL );
}

bool Thread::StartThread( )
{
  int ret;
  if(( ret = pthread_create( &thread, NULL, run, (void *) this )) != 0 )
  {
    LogError( "error creating thread: %d", ret );
    return false;
  }
  return true;
}

void *Thread::run( void *ptr )
{
  Thread *t = (Thread *) ptr;
  t->started = true;
  t->Run( );
  t->started = false;
  pthread_exit( NULL );
  return NULL;
}

