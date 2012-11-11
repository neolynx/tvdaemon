


var last_adapter = null;
var last_frontend = null;
var last_port = null;

function renderAdapter( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  adapter = data["adapter"]
  if( last_adapter == adapter["id"] )
    return "";
  last_adapter = adapter["id"];
  last_frontend = null;
  type = "???";
  if( adapter["path"].indexOf( "/usb" ) != -1 )
    type = "USB";
  else if ( adapter["path"].indexOf( "/pci" ) != -1 )
    type = "PCI";
  if( adapter["present"] == 1 )
    present = "* ";
  else
    present = "! ";
  return present + type + ": " + adapter["name"];
}

function renderFrontend( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  frontend = data["frontend"]
  if( last_frontend == frontend["id"] )
    return "";
  last_frontend = frontend["id"];
  last_port = null;
  return frontend["name"];
}

function renderPort( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  port = data["port"]
  if( last_port == port["id"] )
    return "";
  last_port = port["id"];
  return port["name"];
}

function renderSource( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  port = data["port"];
  if( port["source_id"] >= 0 )
    return sources[port["source_id"]];
  return "undefined";
}

var tvdata = [];

function ready( )
{
  $.getJSON('tvd?c=tvdaemon&a=list_sources', readSources );
}

var sources = [];

function readSources( data )
{
  for( s in data["data"] )
  {
    sources.push( data["data"][s]["name"] );
  }
  $.getJSON('tvd?c=tvdaemon&a=list_devices', readDevices );
}

function readDevices( data )
{
  for( a in data )
  {
    adapter = data[a];
    for( f in adapter["frontends"] )
    {
      frontend = adapter["frontends"][f];
      for( p in frontend["ports"] )
      {
        port = frontend["ports"][p];
        tvdata.push( { "adapter": adapter,
                       "frontend": frontend,
                       "port": port,
                       "source": "" } );
      }
    }
  }

  var data =
  {
    datatype: "array",
    localdata: tvdata
  };

  var theme = getTheme();

  $("#name").addClass('jqx-input');
  $("#type").addClass('jqx-input');
  if( theme.length > 0 )
  {
    $("#name").addClass('jqx-input-' + theme);
    $("#type").addClass('jqx-input-' + theme);
  }

  var dataAdapter = new $.jqx.dataAdapter(data);
  $("#gui").jqxGrid(
    {
      source: dataAdapter,
      theme: theme,
      autoheight: true,
      enablehover: false,
      columns: [
        { text: 'Adapter', datafield: 'adapter', cellsrenderer: renderAdapter },
        { text: 'Frontend', datafield: 'frontend', cellsrenderer: renderFrontend },
        { text: 'Port', datafield: 'port', cellsrenderer: renderPort },
        { text: 'Source', datafield: 'source', cellsrenderer: renderSource },
      ]
    });
}

$(document).ready( ready );
