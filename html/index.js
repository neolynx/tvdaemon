$(document).ready( ready );

window.setInterval( function( ) { update( ); }, 2000 );

var sources = [];
var adapters = [];
var no_update = false;

function ready( )
{
  theme = "";
  $('#port_popup').jqxWindow({ width: "200px", resizable: false, theme: theme, isModal: true, autoOpen: false, cancelButton: $("#port_cancel"), modalOpacity: 0.30 });
  $('#port_popup').bind( 'closed', function ( event ) { no_update = false; } );
  $('#port_ok').bind( 'click', function ( event ) { savePort( ); } );
  $('#port_source').bind( 'change', function ( event ) { if( $(this).val( ) == -2 ) editSource( ); } );

  $('#source_popup').jqxWindow({ width: "200px", resizable: false, theme: theme, isModal: true, autoOpen: false, cancelButton: $("#source_cancel"), modalOpacity: 0.30 });
  $('#source_popup').bind( 'closed', function ( event ) { no_update = false; } );
  $('#source_ok').bind( 'click', function ( event ) { saveSource( ); } );

  $('#setup').attr( 'class', 'setup' );

  Menu( "Setup" );
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
  $( '#setup' ).empty( );
  if( !data ) return;
  table = $('<table>');
  table.attr( 'class', 'setup' );
  adapters = data;
  for( a in adapters )
  {
    adapter = data[a];
    row = $('<tr>');
    cell = $('<td>');
    cell.html( getAdapter( adapter ));
    if( a != adapters.length - 1 )
      cell.attr( "class", "adapter" );
    else
      cell.attr( "class", "adapter_last" );
    row.append( cell );

    adapterports = 0;

    frontends = adapter["frontends"]
    for( f in frontends )
    {
      frontend = frontends[f];
      frontend["adapter_id"] = adapter["id"];

      cell2 = $('<td>');
      cell2.html( getFrontend( frontend ));
      if( f != frontends.length - 1 || a != adapters.length - 1 )
        cell2.attr( "class", "frontend" );
      else
        cell2.attr( "class", "frontend_last" );
      //cell2.bind( "click", function( ) { alert ( 'bla' ); } );
      row.append( cell2 );

      ports = Object.keys( frontend["ports"] ).length;
      adapterports += ports;
      cell2.attr( "rowspan", ports );

      ports = frontend["ports"];
      for( p in ports )
      {
        port = ports[p];
        port["frontend_id"] = frontend["id"];
        port["adapter_id"] = adapter["id"];

        cell3 = $('<td>');
        cell3.html( getPort( port ));
        if( p != ports.length - 1 || a != adapters.length - 1 || f != frontends.length -1 )
          cell3.attr( "class", "port" );
        else
          cell3.attr( "class", "port_last" );
        row.append( cell3 );

        cell4 = $('<td>');
        cell4.html( getSource( port ));
        if( p != ports.length - 1 || a != adapters.length - 1 || f != frontends.length -1 )
          cell4.attr( "class", "source" );
        else
          cell4.attr( "class", "source_last" );
        row.append( cell4 );


        row.appendTo( table );
        row = $('<tr>');
      }

    }
    cell.attr( "rowspan", adapterports );
  }

  // end row
  row = $('<tr>');
  cell = $('<td>');
  cell.html( "<div id=\"button1\"> <a href=\"javascript: scan( );\" class=\"button\">Scan</a></div>" );
  row.append( cell );

  cell2 = $('<td>');
  row.append( cell2 );

  cell3 = $('<td>');
  row.append( cell3 );

  cell4 = $('<td>');
  row.append( cell4 );

  row.appendTo( table );
  table.appendTo( '#setup' );
}

function getAdapter( adapter )
{
  imgstyle = "";
  if( adapter["present"] != 1 )
    imgstyle = "alt=\"not present\" class=\"icon_gray\";";
  else
    imgstyle = "class=\"icon\";";
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    icon = "<img src=\"images/usb-icon.png\" " + imgstyle + "/>";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    icon = "<img src=\"images/pci-icon.png\" " + imgstyle + "/>";
  //else FIXME
  return icon;
}

function getFrontend( frontend )
{
  adapter = adapters[frontend["adapter_id"]];
  return "<a href=\"javascript: click_frontend( " + frontend["adapter_id"] + ", " + frontend["id"] + " );\">" + adapter["name"] + "</a>";
}

function getPort( port )
{
  adapter = adapters[port["adapter_id"]];
  frontend = adapter["frontends"][port["frontend_id"]];
  imgstyle = "";
  if( adapter["present"] != 1 )
    imgstyle = "alt=\"not present\" class=\"icon_gray\";";
  else
    imgstyle = "class=\"icon\";";
  if( frontend["type"] == 0 )
    icon = "<img src=\"images/antenna-icon.png\" " + imgstyle + "/>";
  else if( frontend["type"] == 1 )
    icon = "<img src=\"images/antenna-cable.smal.png\" " + imgstyle + "/>";
  else
    icon = "<img src=\"images/antenna-oldstyle.png\" " + imgstyle + "/>";
  return "<a href=\"javascript: editPort( " + port["adapter_id"] + ", " + port["frontend_id"] + ",  " + port["id"] + " );\">" + icon + "</a>";
}

function getSource( port )
{
  source_id = port["source_id"];
  if( source_id >= 0 )
    return "<a href=\"tvd?c=source&a=show&source_id=" + source_id + "\"><b>" + sources[source_id]["name"] + "</b></a>"
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
  if( $("#port_port_id").val( ))
    u = 'tvd?c=port&a=set';
  else
    u = 'tvd?c=frontend&a=create_port';
  $.ajax( {
    type: 'POST',
    cache: false,
    url: u,
    data: $('#port_form').serialize( ),
    success: function(msg) { $("#port_popup").jqxWindow( 'hide' ); },
    error: function( jqXHR, status, errorThrown ) { alert( 'Error: ' + jqXHR.responseText ); }
  } );
}

function editSource( adapter_id, frontend_id, port_id )
{
  no_update = true;
  adapter_id = $("#port_adapter_id").val( );
  frontend_id = $("#port_frontend_id").val( );
  frontend = adapters[adapter_id]["frontends"][frontend_id];
  $("#source_type").val( frontend["type"] );
  $("#source_name").val( "" );

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

function scan( )
{
  $.ajax( {
    type: 'GET',
    cache: false,
    url: 'tvd?c=tvdaemon&a=scan',
  } );
}

function click_frontend( adapter_id, frontend_id )
{
  no_update = true;
  $("#port_popup").jqxWindow('show');
  frontend = adapters[adapter_id]["frontends"][frontend_id];
  $("#port_name").focus( );
  $("#port_name").val( "" );
  $("#port_num").val( frontend["ports"].length );
  $("#port_adapter_id").val( adapter_id );
  $("#port_frontend_id").val( frontend_id );
  $("#port_port_id").val( null );
  $("#port_type").val( frontend["type"] );

  getJSON('tvd?c=tvdaemon&a=get_sources', editPortGetSources );
}

