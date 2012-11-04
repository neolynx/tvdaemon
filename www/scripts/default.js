
$(document).ready( function ()
{
  var theme = getTheme();

  $("#name").addClass('jqx-input');
  $("#type").addClass('jqx-input');
  if (theme.length > 0) {
    $("#name").addClass('jqx-input-' + theme);
    $("#type").addClass('jqx-input-' + theme);
  }


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
      width: 440,
      source: dataAdapter,
      theme: theme,
      autoheight: true,
      columnsresize: true,
      columns: [
        { text: 'Source', datafield: 'name', width: 200 },
        { text: 'Type', datafield: 'type', cellsformat: 'n', width: 170 },

         { text: '', datafield: 'Edit', width: 30, columntype: 'button', cellsrenderer: function () {
             return '...';
         }, buttonclick: function (row) {
             // open the popup window when the user clicks a button.
             editrow = row;
             var offset = $("#jqxgrid").offset();
             $("#popupWindow").jqxWindow({ position: { x: parseInt(offset.left) + 60, y: parseInt(offset.top) + 60} });

             // get the clicked row's data and initialize the input fields.
             var dataRecord = $("#jqxgrid").jqxGrid('getrowdata', editrow);
             $("#name").val(dataRecord.name);
             $("#type").val(dataRecord.type);
             $("#id").val(dataRecord.id);

             // show the popup window.
             $("#popupWindow").jqxWindow('show');
         }
         },
         { text: '', datafield: 'Delete', width: 30, columntype: 'button', cellsrenderer: function( ) {
            return "X";
         }, buttonclick: function( row ) {
            // delete row
            deleterow = row;
            var maxrow = $("#jqxgrid").jqxGrid('getdatainformation').rowscount;
            if( deleterow >= 0 && deleterow < maxrow ) {
              var commit = $("#jqxgrid").jqxGrid("deleterow", deleterow );
            }
         }
         },

      ]
    });

    // initialize the popup window and buttons.
    $("#popupWindow").jqxWindow({ width: 250, resizable: false, theme: theme, isModal: true, autoOpen: false, cancelButton: $("#Cancel"), modalOpacity: 0.01 });
    $("#Cancel").jqxButton({ theme: theme });
    $("#Save").jqxButton({ theme: theme });

    // update the edited row when the user clicks the 'Save' button.
    $("#Save").click(function () {
        if (editrow >= 0) {
            var row = { name: $("#name").val(), type: parseInt($("#type").jqxNumberInput('decimal')),
            };
            $('#jqxgrid').jqxGrid('updaterow', editrow, row);
            $("#popupWindow").jqxWindow('hide');
        }
    });
});

