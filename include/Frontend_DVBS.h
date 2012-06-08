/*
 *  tvdaemon
 *
 *  DVB Frontend_DVBS class
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

#ifndef _Frontend_DVBS_
#define _Frontend_DVBS_

#include "Frontend.h"

class Adapter;

class Frontend_DVBS : public Frontend
{
  public:
    Frontend_DVBS( Adapter &adapter, std::string configfile );
    Frontend_DVBS( Adapter &adapter, int adapter_id, int frontend_id, int config_id );
    virtual ~Frontend_DVBS( );

    virtual bool Tune( const Transponder &transponder, int timeout = 5000 );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    virtual bool HandleNIT( struct section *section );

  private:
    std::string LNB;
};

#endif
