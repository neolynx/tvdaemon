/*
 *  tvdaemon
 *
 *  ATSC Transponder class
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

#ifndef _Transponder_ATSC_
#define _Transponder_ATSC_

#include "Transponder.h"

class Transponder_ATSC : public Transponder
{
  public:
    Transponder_ATSC( Source &src, const fe_delivery_system_t delsys, int config_id );
    Transponder_ATSC( Source &src, std::string configfile );
    virtual ~Transponder_ATSC( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    virtual void AddProperty( const struct dtv_property &prop );

    virtual std::string toString( ) const;

    bool IsSame( const Transponder &transponder );

  private:

    uint32_t modulation;
};

#endif
