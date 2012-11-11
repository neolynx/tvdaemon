
function renderAdapter( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  adapter = data["adapter"]
  return adapter["name"];
}

function renderFrontend( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  frontend = data["frontend"]
  return frontend["name"];
}

function renderPort( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  port = data["port"]
  return port["name"];
}

function renderSource( row )
{
  return "";
}

$(document).ready( function ()
{
  var theme = getTheme();

  $("#name").addClass('jqx-input');
  $("#type").addClass('jqx-input');
  if (theme.length > 0) {
    $("#name").addClass('jqx-input-' + theme);
    $("#type").addClass('jqx-input-' + theme);
  }

  tvdata = [];

  $.getJSON('tvd?c=tvdaemon&a=list_devices', function(data) {
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

    var dataAdapter = new $.jqx.dataAdapter(data);
    $("#gui").jqxGrid(
      {
        source: dataAdapter,
        theme: theme,
        autoheight: true,
        columnsresize: true,
        columns: [
          { text: 'Adapter', datafield: 'adapter', cellsrenderer: renderAdapter },
          { text: 'Frontend', datafield: 'frontend', cellsrenderer: renderFrontend },
          { text: 'Port', datafield: 'port', cellsrenderer: renderPort },
          { text: 'Source', datafield: 'source', cellsrenderer: renderSource },
        ]
      });

  });

});

