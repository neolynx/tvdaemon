$(document).ready( ready );

window.setInterval( function( ) { ready( ); }, 2000 );

var sources = [];

function getJSON( url, callback )
{
  $.ajax( {
    url: url,
    dataType: 'json',
    success: callback,
    timeout: 3000,
    error: function(jqXHR, status, errorThrown ) { callback( null ); }
  });
}

function ready( )
{
  getJSON('tvd?c=tvdaemon&a=list_sources', readSources );
}

function readSources( data )
{
  if( data )
  {
    for( s in data["data"] )
    {
      sources.push( data["data"][s]["name"] );
    }
  }
  getJSON('tvd?c=tvdaemon&a=list_devices', readDevices );
}

function readDevices( data )
{
  $( '#config' ).empty( );
  if( !data ) return;
  for( a in data )
  {
    adapter = data[a];
    row = $('<tr>');
    cell = $('<td>');
    cell.html( getAdapter( adapter ));
    cell.attr( "class", "adapter" );
    row.append( cell );

    adapterports = 0;

    for( f in adapter["frontends"] )
    {
      frontend = adapter["frontends"][f];

      cell2 = $('<td>');
      cell2.html( getFrontend( frontend ));
      cell2.attr( "class", "frontend" );
      row.append( cell2 );

      ports = Object.keys( frontend["ports"] ).length;
      adapterports += ports;
      cell2.attr( "rowspan", ports );

      for( p in frontend["ports"] )
      {
        port = frontend["ports"][p];

        cell3 = $('<td>');
        cell3.html( getPort( port ));
        cell3.attr( "class", "port" );
        row.append( cell3 );

        cell4 = $('<td>');
        cell4.html( getSource( port["source_id"] ));
        cell4.attr( "class", "source" );
        row.append( cell4 );


        row.appendTo( '#config' );
        row = $('<tr>');
      }

    }
    cell.attr( "rowspan", adapterports );
  }

  // end row
  row = $('<tr>');
  cell = $('<td>');
  cell.html( "<div id=\"button1\"> <a href=\"#\" class=\"button\">Add Tuner </a></div>" );
  row.append( cell );

  cell2 = $('<td>');
  cell2.html( "<div id=\"return frontend\"> <a href=\"\"class=\"button\">Select DVB-S/T/C</a></div>" );
  row.append( cell2 );

  cell3 = $('<td>');
  cell3.html( "<div id=\"return port\"> <a href=\"\"class=\"button\"> Add LNB  </a></div>" );
  row.append( cell3 );

  cell4 = $('<td>');
  cell4.html( "<div id=\"return sat\"> <a href=\"\"class=\"button\"> Add Location</a></div>" );
  row.append( cell4 );

  row.appendTo( '#config' );
}

function getAdapter( adapter )
{
  if( adapter["present"] == 1 )
    imgstyle = "";
  else
    imgstyle = content; // this should use class as well, and take style from css !
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    icon = "<img src=\"images/usb-icon.png\" style=\"" + imgstyle + "\"/>";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    icon = "<img src=\"images/pci-icon.png\" style=\"" + imgstyle + "\"/>";
  return icon + '<br/>' + adapter["name"];
}

function getFrontend( frontend )
{
  return frontend["name"] + "<br/><a href=\"\"class=\"button\">Add port</a>";
}

function getPort( port )
{
  return port["name"];
}

function getSource( source_id )
{
  if( source_id >= 0 )
    return sources[source_id];
  else
    return " - ";
}

