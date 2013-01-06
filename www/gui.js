$(document).ready( ready );

window.setInterval( function( ) { update( ); }, 2000 );

var sources = [];
var adapters = [];
var no_update = false;

function getJSON( url, callback )
{
  $.ajax( {
    url: url,
    dataType: 'json',
    success: callback,
    timeout: 3000,
    error: function(jqXHR, status, errorThrown ) { callback( null, jqXHR.responseText ); }
  });
}

function ready( )
{
  theme = "";
  $('#port_popup').jqxWindow({ width: "auto", resizable: false, theme: theme, isModal: true, autoOpen: false, cancelButton: $("#port_cancel"), modalOpacity: 0.30 });
  $('#port_popup').bind( 'closed', function ( event ) { no_update = false; } );
  $('#port_ok').bind( 'click', function ( event ) { savePort( ); } );
  $('#port_source').bind( 'change', function ( event ) { if( $(this).val( ) == -2 ) editSource( ); } );

  $('#source_popup').jqxWindow({ width: "auto", resizable: false, theme: theme, isModal: true, autoOpen: false, cancelButton: $("#source_cancel"), modalOpacity: 0.30 });
  $('#source_popup').bind( 'closed', function ( event ) { no_update = false; } );
  $('#source_ok').bind( 'click', function ( event ) { saveSource( ); } );

  update( );
}

function update( )
{
  if( no_update )
    return;
  getJSON('tvd?c=tvdaemon&a=get_sources', readSources );
}

function readSources( data, errmsg )
{
  if( data )
  {
    sources = [];
    for( s in data["data"] )
    {
      sources.push( data["data"][s] );
    }
  }
  getJSON('tvd?c=tvdaemon&a=get_devices', readDevices );
}

function readDevices( data, errmsg )
{
  $( '#config' ).empty( );
  if( !data ) return;
  adapters = data;
  for( a in adapters )
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
      frontend["adapter_id"] = adapter["id"];

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
        port["frontend_id"] = frontend["id"];
        port["adapter_id"] = adapter["id"];

        cell3 = $('<td>');
        cell3.html( getPort( port ));
        cell3.attr( "class", "port" );
        row.append( cell3 );

        cell4 = $('<td>');
        cell4.html( getSource( port ));
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
  row.append( cell2 );

  cell3 = $('<td>');
  row.append( cell3 );

  cell4 = $('<td>');
  row.append( cell4 );

  row.appendTo( '#config' );
}

function getAdapter( adapter )
{
  imgstyle = "";
  if( adapter["present"] != 1 )
    imgstyle = "alt=\"not present\" class=\"grey\";";
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    icon = "<img src=\"images/usb-icon.png\" style=\"" + imgstyle + "\"/>";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    icon = "<img src=\"images/pci-icon.png\" style=\"" + imgstyle + "\"/>";
  return "<b>" + adapter["name"] + "<br/>" + icon;
}

function getFrontend( frontend )
{
  adapter = adapters[frontend["adapter_id"]];
  imgstyle = "";
  if( adapter["present"] != 1 )
    imgstyle = "alt=\"not present\" class=\"grey\";";
  if( frontend["type"] == 0 )
    icon = "<img src=\"images/antenna-icon.png\" style=\"" + imgstyle + "\"/>";
  else if( frontend["type"] == 1 )
    icon = "<img src=\"images/antenna-cable.smal.png\" style=\"" + imgstyle + "\"/>";
  else if( frontend["type"] == 2 )
    icon = "<img src=\"images/antenna-oldstyle.png\" style=\"" + imgstyle + "\"/>";
  return icon;
}

function getPort( port )
{
  frontend = adapters[port["adapter_id"]]["frontends"][port["frontend_id"]];
  //if( frontend["type"] == 0 ) // Sat
  return "<a href=\"javascript: editPort( " + port["adapter_id"] + ", " + port["frontend_id"] + ",  " + port["id"] + " );\"><b>" + port["name"] + "</b></a>";
  //return "";
}

function getSource( port )
{
  source_id = port["source_id"];
  if( source_id >= 0 )
    return "<b>" + sources[source_id]["name"]
      + "</b><br/>Type: " + sources[source_id]["type"]
      + "<br/>Transponders: " + sources[source_id]["transponders"]
      + "<br/>Services: " + sources[source_id]["services"];
  else
    return "Undefined";
}

function editPort( adapter_id, frontend_id, port_id )
{
  no_update = true;
  $("#port_popup").jqxWindow('show');
  frontend = adapters[adapter_id]["frontends"][frontend_id];
  port = frontend["ports"][port_id];
  $("#port_name").focus( );
  $("#port_name").val( port["name"] );
  $("#port_num").val( port["port_num"] );
  $("#port_adapter_id").val( adapter_id );
  $("#port_frontend_id").val( frontend_id );
  $("#port_port_id").val( port_id );
  $("#port_type").val( frontend["type"] );

  getJSON('tvd?c=tvdaemon&a=get_sources', editPortGetSources );
}

function editPortGetSources( data, errmsg )
{
  type = $("#port_type").val( );
  $("#port_source").empty( );
  $("#port_source").append( new Option( "Undefined", "-1" ));
  sources = [];
  for( s in data["data"] )
  {
    source = data["data"][s];
    if( source["type"] == type )
      $("#port_source").append( new Option( source["name"], source["id"] ));
    sources.push( source );
  }
  $("#port_source").val( port["source_id"] ).attr( 'selected', true );
  $("#port_source").append( new Option( "create new ...", "-2" ));
}

function savePort( )
{
  $.ajax( {
    type: 'POST',
    cache: false,
    url: 'tvd?c=port&a=set',
    data: $('#port_form').serialize( ),
    success: function(msg) { $("#port_popup").jqxWindow( 'hide' ); },
    error: function( jqXHR, status, errorThrown ) { alert( jqXHR.responseText ); }
  } );
}

function editSource( adapter_id, frontend_id, port_id )
{
  no_update = true;
  adapter_id = $("#port_adapter_id").val( );
  frontend_id = $("#port_frontend_id").val( );
  frontend = adapters[adapter_id]["frontends"][frontend_id];
  $("#source_type").val( frontend["type"] );

  getJSON( 'tvd?c=tvdaemon&a=get_scanfiles&type=' + frontend["type"], loadScanfiles );
}

function loadScanfiles( scanfiles, errmsg )
{
  if( !scanfiles )
  {
    alert( errmsg );
    return;
  }

  $("#source_scanfile").empty( );
  $("#source_scanfile").append( new Option( "select ...", "-1" ));
  for( i in scanfiles )
    $("#source_scanfile").append( new Option( scanfiles[i], scanfiles[i] ));

  $("#source_popup").jqxWindow('show');
}

function saveSource( )
{
  $.ajax( {
    type: 'POST',
    cache: false,
    url: 'tvd?c=tvdaemon&a=create_source',
    data: $('#source_form').serialize( ),
    success: function(msg) {
      $("#source_popup").jqxWindow( 'hide' );
      port["source_id"] = msg;
      getJSON('tvd?c=tvdaemon&a=get_sources', editPortGetSources );
      },
    error: function( jqXHR, status, errorThrown ) { alert( jqXHR.responseText ); }
  } );
}

