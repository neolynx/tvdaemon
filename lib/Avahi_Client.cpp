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

#include "Avahi_Client.h"
#include "TVDaemon.h"
#include "Log.h"

#include <avahi-common/alternative.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

static void browse_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event,
                            const char *name, const char *type, const char *domain,
                            AvahiLookupResultFlags flags, void* userdata)
{
  Avahi_Client *srv = static_cast<Avahi_Client *>( userdata );
  srv->Browse_Callback( b, interface, protocol, event, name, type, domain, flags );
}

static void client_callback( AvahiClient *c, AvahiClientState state, void *userdata )
{
  Avahi_Client *srv = static_cast<Avahi_Client *>( userdata );
  srv->Client_Callback( c, state );
}

static void entry_group_callback( AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata )
{
  Avahi_Client *srv = static_cast<Avahi_Client *>( userdata );
  srv->Entry_Group_Callback( g, state );
}

/*static void modify_callback( AvahiTimeout *e, void *userdata )
{
  Avahi_Client *srv = static_cast<Avahi_Client *>( userdata );
  srv->Modify_Callback( e );
}*/

static void resolve_callback( AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
                              const char *name, const char *type, const char *domain, const char *host_name,
                              const AvahiAddress *address, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags,
                              void* userdata)
{
  Avahi_Client *srv = static_cast<Avahi_Client *>( userdata );
  srv->Resolve_Callback( r, event, name, type, domain, host_name, address, port, txt, flags );
  avahi_service_resolver_free( r );
}

Avahi_Client::Avahi_Client( ) : Thread( ),
  client( NULL ),
  group( NULL ),
  browser( NULL ),
  simple_poll( NULL )
{
}

Avahi_Client::~Avahi_Client()
{
  if ( simple_poll )
    avahi_simple_poll_quit( simple_poll );

  JoinThread( );

  if( browser )
    avahi_service_browser_free( browser );

  if( client )
    avahi_client_free( client );

  if( simple_poll )
    avahi_simple_poll_free( simple_poll );
}

bool Avahi_Client::Start( )
{
  return StartThread( );
}

void Avahi_Client::Browse_Callback( AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event,
                                    const char *name, const char *type, const char *domain,
                                    AvahiLookupResultFlags flags )
{
  if( !b )
  {
    LogError( "Avahi: browser null pointer" );
    return;
  }

  // Called whenever a new services becomes available on the LAN or is removed from the LAN
  switch (event)
  {
    case AVAHI_BROWSER_FAILURE:
      LogError( "Avahi: Browser - %s", avahi_strerror( avahi_client_errno( avahi_service_browser_get_client( b ))));
      break;

    case AVAHI_BROWSER_NEW:
      Log( "Avahi: new service found, '%s' of type '%s' in domain '%s'", name, type, domain);
      if( !( avahi_service_resolver_new( client, interface, protocol, name, type, domain,
                                         AVAHI_PROTO_INET, AVAHI_LOOKUP_USE_MULTICAST, resolve_callback, static_cast<void *>( this ))))
      {
        LogError( "Avahi: Failed to create service resolver '%s': %s", name, avahi_strerror( avahi_client_errno( client )));
      }
      break;

    case AVAHI_BROWSER_REMOVE:
      Log( "Avahi: service removed, '%s' of type '%s' in domain '%s'", name, type, domain);
      break;

//    case AVAHI_BROWSER_ALL_FOR_NOW:
//    case AVAHI_BROWSER_CACHE_EXHAUSTED:
//      break;
  }
}

void Avahi_Client::Client_Callback( AvahiClient *c, AvahiClientState state )
{
  if( !c )
  {
    LogError( "Avahi: client null pointer" );
    return;
  }

  // Called whenever the client or server state changes
  switch( state )
  {
    // The server has startup successfully and registered its host
    // name on the network, so it's time to create our services
    case AVAHI_CLIENT_S_RUNNING:
      Create_Services( c );
      break;

    case AVAHI_CLIENT_FAILURE:
      LogError( "Avahi: Client failure: %s", avahi_strerror( avahi_client_errno( c )));
      avahi_simple_poll_quit( simple_poll );
      break;

    // Let's drop our registered services. When the server is back
    // in AVAHI_SERVER_RUNNING state we will register them
    // again with the new host name.
    case AVAHI_CLIENT_S_COLLISION:

    // The server records are now being established. This
    // might be caused by a host name change. We need to wait
    // for our own records to register until the host name is
    // properly established.
    case AVAHI_CLIENT_S_REGISTERING:
      if( group )
        avahi_entry_group_reset( group );
      break;
  }
}

void Avahi_Client::Create_Services( AvahiClient *c )
{
  // If this is the first time we're called, let's create a new entry group
  if( !group )
    if( !( group = avahi_entry_group_new( c, entry_group_callback, static_cast<void *>( this ))))
    {
      LogError( "Avahi: avahi_entry_group_new() failed: %s", avahi_strerror( avahi_client_errno( c )));
      goto fail;
    }

  if( avahi_entry_group_is_empty( group ))
  {
    Log( "Avahi: Adding avahi service" );

    // Add the service for HTTP
    int ret;
    char hostname[HOST_NAME_MAX] = { 0 };
    gethostname( hostname, HOST_NAME_MAX);
    std::string temp = "TVDaemon Web Interface on ";
    temp += hostname;
    if(( ret = avahi_entry_group_add_service( group, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, AVAHI_PUBLISH_USE_MULTICAST,
                                              temp.c_str(), "_http._tcp", NULL, NULL, HTTPDPORT, NULL )) < 0 )
    {
      LogError( "Avahi: Failed to add _http._tcp service: %s", avahi_strerror( ret ));
      goto fail;
    }

    // Tell the server to register the service
    if( ( ret = avahi_entry_group_commit( group )) < 0 )
    {
      LogError( "Avahi: Failed to commit entry_group: %s", avahi_strerror( ret ));
      goto fail;
    }
  }
  return;

fail:
  avahi_simple_poll_quit( simple_poll );
}

void Avahi_Client::Entry_Group_Callback( AvahiEntryGroup *g, AvahiEntryGroupState state )
{
  if( !( g == group || group == NULL ))
  {
    LogError( "Avahi: avahi group pointer problem" );
    return;
  }
  group = g;

  /* Called whenever the entry group state changes */
  switch( state )
  {
    /* The entry group has been established successfully */
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
      Log( "Avahi: Service successfully established" );
      break;

    /* A service name collision happened. Let's pick a new name */
    case AVAHI_ENTRY_GROUP_COLLISION:
      LogError( "Avahi: Service name collision" );
      avahi_simple_poll_quit( simple_poll );
      break;

    /* Some kind of failure happened while we were registering our services */
    case AVAHI_ENTRY_GROUP_FAILURE:
      LogError( "Avahi: Entry group failure: %s", avahi_strerror( avahi_client_errno( avahi_entry_group_get_client( g ))));
      avahi_simple_poll_quit( simple_poll );
      break;
  }
}

/* void Avahi_Client::Modify_Callback( AvahiTimeout * )
{
  LogError( "Avahi: Doing some weird modification");

  // If the server is currently running, we need to remove our
  // service and create it anew
  if( avahi_client_get_state( client ) == AVAHI_CLIENT_S_RUNNING )
  {
    // Remove the old services
    if( group )
      avahi_entry_group_reset( group );

    // And create them again with the new name
    Avahi_Client::Create_Services( client );
  }
} */

void Avahi_Client::Resolve_Callback( AvahiServiceResolver *r, AvahiResolverEvent event,
                                     const char *name, const char *type, const char *domain, const char *host_name,
                                     const AvahiAddress *address, uint16_t port,
                                     AvahiStringList *txt, AvahiLookupResultFlags flags )
{
  if( !r )
  {
    LogError( "Avahi: resolver null pointer" );
    return;
  }

  switch (event)
  {
    case AVAHI_RESOLVER_FAILURE:
      LogError( "Avahi: Failed to resolve service '%s' of type '%s' in domain '%s': %s",
                name, type, domain, avahi_strerror( avahi_client_errno( avahi_service_resolver_get_client( r ))));
      break;

    case AVAHI_RESOLVER_FOUND:
    {
      char a[AVAHI_ADDRESS_STR_MAX];
      avahi_address_snprint(a, sizeof(a), address);

      Log( "Avahi: new service resolved, '%s' of type '%s' in domain '%s': %s:%u (%s)",
           name, type, domain, host_name, port, a);
    } break;
  }
}

void Avahi_Client::Run( )
{
  // Allocate main loop object
  if( !( simple_poll = avahi_simple_poll_new( )))
  {
    LogError( "Avahi: Failed to create simple poll object." );
    return;
  }

  // Allocate a new client
  int error;
  client = avahi_client_new( avahi_simple_poll_get( simple_poll ), AVAHI_CLIENT_NO_FAIL, client_callback, static_cast<void *>( this ), &error );

  // Check whether creating the server object succeeded
  if( !client )
  {
    LogError( "Avahi: Failed to create client: %s", avahi_strerror(error ));
    return;
  }

  // Create the service browser
  if( !( browser = avahi_service_browser_new( client, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET,
                                              "_airplay._tcp", NULL, AVAHI_LOOKUP_USE_MULTICAST, browse_callback, static_cast<void *>( this ))))
  {
    LogError( "Avahi: Failed to create service browser" );
  }

  // After 10s do some weird modification to the service
  /*avahi_simple_poll_get( simple_poll )->timeout_new(
      avahi_simple_poll_get( simple_poll ),
      avahi_elapse_time( &tv, 1000*10, 0 ),
      modify_callback,
      static_cast<void *>( this ));*/

  // Run the main loop
  avahi_simple_poll_loop( simple_poll );
}
