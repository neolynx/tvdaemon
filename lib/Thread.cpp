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

#include <sys/time.h>
#include <errno.h>  // ETIMEDOUT
#include <limits.h> // PTHREAD_STACK_MIN

#include "Log.h"

Mutex::Mutex( )
{
  pthread_mutex_init( &mutex, NULL );
}

Mutex::~Mutex( )
{
  pthread_mutex_destroy( &mutex );
}

void Mutex::Lock( ) const
{
  pthread_mutex_lock( &mutex );
}

void Mutex::Unlock( ) const
{
  pthread_mutex_unlock( &mutex );
}

Condition::Condition( )
{
  pthread_cond_init((pthread_cond_t *) &cond, NULL );
}

Condition::~Condition( )
{
  pthread_cond_destroy((pthread_cond_t *) &cond );
}

void Condition::Signal( ) const
{
  pthread_cond_signal((pthread_cond_t *) &cond );
}

bool Condition::Wait( int seconds ) const
{
  struct timespec ts;

  while( seconds-- )
  {
    clock_gettime( CLOCK_REALTIME, &ts );
    ts.tv_sec++;

    int r = pthread_cond_timedwait( &cond, &mutex, &ts );
    switch( r )
    {
      case ETIMEDOUT:
        continue;
      case 0:
        Unlock( );
        return true;
      default:
        return false;
    }
  }
  return false;
}

bool Condition::WaitUntil( struct timespec ts ) const
{
  while( true )
  {
    int r = pthread_cond_timedwait( &cond, &mutex, &ts );
    switch( r )
    {
      case ETIMEDOUT:
        return true;
      case 0:
        Unlock( );
        return true;
      default:
	return false;
    }
  }
  return false;
}

Thread::Thread( ) : Mutex( ), started(false)
{
}

Thread::~Thread( )
{
  JoinThread( );
}

void Thread::JoinThread( )
{
  if( started )
    pthread_join( thread, NULL );
}

bool Thread::StartThread( )
{
  int ret;
  pthread_attr_t attr;
  pthread_attr_init( &attr );
  if(( ret = pthread_attr_setstacksize( &attr, PTHREAD_STACK_MIN * 2 )) != 0 )
  {
    LogError( "Thread: error setting stack size: %d", ret );
    return false;
  }
  if(( ret = pthread_create( &thread, &attr, run, (void *) this )) != 0 )
  {
    LogError( "error creating thread: %d", ret );
    return false;
  }
  pthread_attr_destroy( &attr );
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
