/*
 *  tvdaemon
 *
 *  DVB Transponder class
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

#ifndef _Transponder_DVBS_
#define _Transponder_DVBS_

#include "Transponder.h"

#include <libdvbv5/dvb-sat.h>

class Transponder_DVBS : public Transponder
{
  public:
    Transponder_DVBS( Source &source, const fe_delivery_system_t delsys, int config_id );
    Transponder_DVBS( Source &source,
		    fe_delivery_system_t delsys, uint32_t frequency,
		    dvb_sat_polarization polarization,
		    uint32_t symbol_rate,
		    fe_code_rate fec,
		    fe_modulation modulation,
		    fe_rolloff roll_off,
		    int config_id );
    Transponder_DVBS( Source &source, std::string configfile );
    virtual ~Transponder_DVBS( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    virtual void AddProperty( const struct dtv_property &prop );

    virtual bool GetParams( struct dvb_v5_fe_parms *params ) const;

    virtual std::string toString( ) const;

    bool IsSame( const Transponder &transponder );

  private:
    void Init( );

    dvb_sat_polarization polarization;

    uint32_t symbol_rate;
    fe_code_rate fec;
    fe_modulation modulation;
    fe_rolloff roll_off;

};

#endif
