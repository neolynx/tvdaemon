
var source_types = {};

function renderType( row )
{
  var data = $("#sources").jqxGrid( 'getrowdata', row );
  type = source_types[data["type"]]
  return '<div style="overflow: hidden; text-overflow: ellipsis; padding-bottom: 2px; text-align: left; margin: 4px;">' + type + '</div>';
}

function renderSource( row )
{
  var data = $("#sources").jqxGrid( 'getrowdata', row );
  return '<div style="overflow: hidden; text-overflow: ellipsis; padding-bottom: 2px; text-align: left; margin: 4px;">' + '<a href="tvd?c=source&a=show&source_id=' + data["id"] + '">' + data["name"] + '</a>' + '</div>';
}

function renderAdapter( row )
{
  var data = $("#adapters").jqxGrid( 'getrowdata', row );
  return '<div style="overflow: hidden; text-overflow: ellipsis; padding-bottom: 2px; text-align: left; margin: 4px;">' + '<a href="tvd?c=adapter&a=show&adapter_id=' + data["id"] + '">' + data["name"] + '</a>' + '</div>';
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

  $.getJSON('tvd?c=tvdaemon&a=list_sourcetypes', function(data) {
    for( type in data["data"] )
    {
      source_types[data["data"][type]["id"]] = data["data"][type]["type"];
    }
  });

  var sources =
  {
    datatype: "json",
    datafields: [
      { name: 'name' },
      { name: 'type', type: 'int' },
      { name: 'id', type: 'int' },
    ],
    url: "tvd",
    data: { c: "tvdaemon", a: "list_sources" }
  };

  var dataAdapter = new $.jqx.dataAdapter(sources);
  $("#sources").jqxGrid(
    {
      source: dataAdapter,
      theme: theme,
      autoheight: true,
      columnsresize: true,
      columns: [
        { text: 'Source', datafield: 'name', cellsrenderer: renderSource },
        { text: 'Type', datafield: 'type', cellsrenderer: renderType },

        { text: '', datafield: 'Edit', width: 30, columntype: 'button', cellsrenderer: function () { return '...'; }, buttonclick: function (row) { } },
        { text: '', datafield: 'Delete', width: 30, columntype: 'button', cellsrenderer: function( ) { return "X"; }, buttonclick: function( row ) { } },
      ]
    });

  var adapters =
  {
    datatype: "json",
    datafields: [
      { name: 'name' },
      { name: 'path', type: 'int' },
      { name: 'id', type: 'int' },
    ],
    url: "tvd",
    data: { c: "tvdaemon", a: "list_adapters" }
  };

  var dataAdapter = new $.jqx.dataAdapter(adapters);
  $("#adapters").jqxGrid(
    {
      source: dataAdapter,
      theme: theme,
      autoheight: true,
      columnsresize: true,
      columns: [
        { text: 'Adapter', datafield: 'name', cellsrenderer: renderAdapter },
        { text: 'Path',    datafield: 'path' },

        { text: '', datafield: 'Edit', width: 30, columntype: 'button', cellsrenderer: function () { return '...'; }, buttonclick: function (row) { } },
        { text: '', datafield: 'Delete', width: 30, columntype: 'button', cellsrenderer: function( ) { return "X"; }, buttonclick: function( row ) { } },
      ]
    });
});

