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
    Frontend_DVBS( Adapter &adapter, std::string name, int adapter_id, int frontend_id, int config_id );
    Frontend_DVBS( Adapter &adapter, std::string configfile );
    virtual ~Frontend_DVBS( );

    virtual bool SetTuneParams( Transponder &transponder );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    virtual bool HandleNIT( struct dvb_table_nit *nit );

  private:
    std::string LNB;
};

#endif
