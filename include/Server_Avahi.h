/*
 *  tvdaemon
 *
 *  Server_Avahi class
 *
 *  Copyright (C) 2014 André Roth
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

#ifndef _Server_Avahi_
#define _Server_Avahi_

#include "Thread.h"

class Server_Avahi : public Thread
{
public:
  Server_Avahi( );
  virtual ~Server_Avahi( );

  bool Start( );

private:
  virtual void Run( );
};

#endif
