

function renderAdapter( row )
{
  var data = $("#gui").jqxGrid( 'getrowdata', row );
  adapter = data["adapter"]
  alert( adapter["path"] );
  return adapter["name"];
}

function renderFrontend( row )
{
  return "";
}

function renderPort( row )
{
  return "";
}

function renderSource( row )
{
  return "";
}


var adapters = { };

$(document).ready( function ()
{
  var theme = getTheme();

  $("#name").addClass('jqx-input');
  $("#type").addClass('jqx-input');
  if (theme.length > 0) {
    $("#name").addClass('jqx-input-' + theme);
    $("#type").addClass('jqx-input-' + theme);
  }

  $.getJSON('tvd?c=tvdaemon&a=list_adapters', function(data) {
    adapters = data;
  });



  alert( "snoooooze..." );

  tvdata = []

  for( id in adapters["data"] )
  {
    tvdata.push( { "adapter": adapters["data"][id],
                   "frontend": "",
                   "port": "",
                   "source": "" } );
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

