
$(document).ready( function ()
{
  var theme = getTheme();
  var source =
  {
    datatype: "jsonp",
    datafields: [
      { name: 'name' },
      { name: 'type', type: 'int' },
      { name: 'id', type: 'int' },
    ],
    url: "tvd",
    data: { c: "source", a: "list" }
  };

  var dataAdapter = new $.jqx.dataAdapter(source);
  $("#jqxgrid").jqxGrid(
    {
      width: 670,
      source: dataAdapter,
      theme: theme,
      columnsresize: false,
      columns: [
        { text: 'Source', datafield: 'name', width: 200 },
        { text: 'Type', datafield: 'type', cellsformat: 'n', width: 170 },
      ]
    });
});

