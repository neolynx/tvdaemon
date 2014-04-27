/*
 *  tvdaemon
 *
 *  HDHomerun_Client class
 *
 *  Copyright (C) 2014 Lars Schmohl
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

#ifndef _HDHomerun_Client_
#define _HDHomerun_Client_

#include "config.h"

#ifdef HAVE_LIBHDHOMERUN

#include <libhdhomerun/hdhomerun.h>
#include <list>

#include "Thread.h"

/*
 * this class is intended to be used to discover and monitor
 * the hdhomerun devices, it should only exist once in the
 * application
 */

#define HDHOMERUN_DISOCVER_MAX_SOCK_COUNT 16

class HDHomerun_Client: public Thread
{
public:
  HDHomerun_Client( );
  virtual ~HDHomerun_Client( );

  bool Start( );

private:

  virtual void Run( );

  bool up;

  hdhomerun_discover_t* discover_obj;
  hdhomerun_discover_device_t discover_dev[HDHOMERUN_DISOCVER_MAX_SOCK_COUNT];

  std::list<uint32_t> current_devices;
};

#endif /* HAVE_LIBHDHOMERUN */

#endif /* _HDHomerun_Client_ */
