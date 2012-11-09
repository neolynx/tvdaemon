
var source_types = {};

function renderType( row )
{
  var data = $("#frontends").jqxGrid( 'getrowdata', row );
  type = data["type"]
  return '<div style="overflow: hidden; text-overflow: ellipsis; padding-bottom: 2px; text-align: left; margin: 4px;">' + type + '</div>';
}

function renderLink( row )
{
  var data = $("#frontends").jqxGrid( 'getrowdata', row );
  return '<div style="overflow: hidden; text-overflow: ellipsis; padding-bottom: 2px; text-align: left; margin: 4px;">' + '<a href="tvd?c=frontend&a=show&adapter_id=' + adapter_id + '&transponder_id=' + data["id"] + '">' + data["name"] + '</a>' + '</div>';
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

  //$.getJSON('tvd?c=tvdaemon&a=list_sourcetypes', function(data) {
    //for( type in data["data"] )
    //{
      //source_types[data["data"][type]["id"]] = data["data"][type]["type"];
    //}
  //});

  var data =
  {
    datatype: "json",
    datafields: [
      { name: 'name' },
      { name: 'type', type: 'int' },
      { name: 'id', type: 'int' },
      { name: 'state', type: 'int' },
    ],
    url: "tvd",
    data: { c: "adapter", a: "list_frontends", adapter_id: adapter_id }
  };

  var dataAdapter = new $.jqx.dataAdapter(data);
  $("#frontends").jqxGrid(
    {
      source: dataAdapter,
      theme: theme,
      autoheight: true,
      columnsresize: true,
      columns: [
        { text: 'Frontend', datafield: 'id', cellsrenderer: renderLink },
        { text: 'Type', datafield: 'type', cellsrenderer: renderType },

         { text: '', datafield: 'Edit', width: 30, columntype: 'button', cellsrenderer: function () { return '...'; }, buttonclick: function (row) { } },
         { text: '', datafield: 'Delete', width: 30, columntype: 'button', cellsrenderer: function( ) { return "X"; }, buttonclick: function( row ) { } },

      ]
    });

});

