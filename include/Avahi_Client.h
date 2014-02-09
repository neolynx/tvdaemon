/*
 *  tvdaemon
 *
 *  Avahi_Client class
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

#ifndef _Avahi_Client_
#define _Avahi_Client_

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>

#include "Thread.h"

class Avahi_Client : public Thread
{
public:
  Avahi_Client( );
  virtual ~Avahi_Client( );

  bool Start( );

  // avahi functions
  void Browse_Callback( AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event,
                        const char *name, const char *type, const char *domain,
                        AvahiLookupResultFlags flags );
  void Client_Callback( AvahiClient *c, AvahiClientState state );
  void Create_Services( AvahiClient *c );
  void Entry_Group_Callback( AvahiEntryGroup *g, AvahiEntryGroupState state );
  // void Modify_Callback( AvahiTimeout *e );
  void Resolve_Callback( AvahiServiceResolver *r, AvahiResolverEvent event,
                         const char *name, const char *type, const char *domain, const char *host_name,
                         const AvahiAddress *address, uint16_t port,
                         AvahiStringList *txt, AvahiLookupResultFlags flags );

private:

  virtual void Run( );

  AvahiClient *client;
  AvahiEntryGroup *group;
  AvahiServiceBrowser *browser;
  AvahiSimplePoll *simple_poll;
};

#endif
