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

#include "Thread.h"

#include "Log.h"

ThreadBase::ThreadBase( )
{
  pthread_mutex_init( (pthread_mutex_t *) &mutex, NULL );
}

void ThreadBase::Lock( ) const
{
  pthread_mutex_lock( (pthread_mutex_t *) &mutex );
}

void ThreadBase::Unlock( ) const
{
  pthread_mutex_unlock( (pthread_mutex_t *) &mutex );
}

ThreadBase::Thread::Thread( ThreadBase &base, ThreadFunc func ) : base(base), func(func)
{
}

ThreadBase::Thread::~Thread( )
{
  pthread_join( thread, NULL );
}

bool ThreadBase::Thread::Run( )
{
  if( pthread_create( &thread, NULL, run, (void *) this ) != 0 )
  {
    LogError( "cannot create thread" );
    return false;
  }
  return true;
}

void *ThreadBase::Thread::run( void *ptr )
{
  Thread *t = (Thread *) ptr;
  (t->base.*t->func)( );
  pthread_exit( NULL );
  return NULL;
}

