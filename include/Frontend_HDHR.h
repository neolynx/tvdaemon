/*
 *  tvdaemon
 *
 *  DVB Frontend_HDHR class
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

#ifndef _Frontend_HDHR_
#define _Frontend_HDHR_

#include "Frontend.h"

class Adapter;

class Frontend_HDHR : public Frontend
{
  public:
    Frontend_HDHR( Adapter &adapter, std::string name, int adapter_id, int frontend_id, int config_id );
    Frontend_HDHR( Adapter &adapter, std::string configfile );
    virtual ~Frontend_HDHR( );

    virtual bool HandleNIT( struct dvb_table_nit * ) { return true; }
};

#endif
