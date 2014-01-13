/*
 *  tvdaemon
 *
 *  CAMClientHandler class
 *
 *  Copyright (C) 2014 Bndré Roth
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

#ifndef _CAMClientHandler_
#define _CAMClientHandler_

#include "ConfigObject.h"

#include <vector>

class TVDaemon;
class CAMClient;

class CAMClientHandler : public ConfigObject
{
  public:
    static CAMClientHandler *Instance( );
    ~CAMClientHandler( );

    virtual bool SaveConfig( );
    virtual bool LoadConfig( );

    CAMClient *GetCAMClient( uint16_t caid );

  private:
    CAMClientHandler( );

    static CAMClientHandler *instance;

    std::vector<CAMClient *> clients;
};

#endif
