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

#include "Frontend_HDHR.h"

#include "Log.h"
#include "Source.h"
#include "Adapter.h"

Frontend_HDHR::Frontend_HDHR( Adapter &adapter, std::string name, int adapter_id, int frontend_id, int config_id ) :
  Frontend( adapter, name, adapter_id, frontend_id, config_id )
{
  type = Source::Type_HDHR;
  Log( "  Creating Frontend HDHR %d Tuner %d", adapter_id, frontend_id );
}

Frontend_HDHR::Frontend_HDHR( Adapter &adapter, std::string configfile ) : Frontend( adapter, configfile )
{
}

Frontend_HDHR::~Frontend_HDHR( )
{
}
