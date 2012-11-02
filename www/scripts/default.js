$(document).ready(function () {
    var theme = getTheme();

    // prepare the data
    var data = generatedata(200);


    var source =
    {
        localdata: data,
        datatype: "array",
        updaterow: function (rowid, rowdata, commit) {
            // synchronize with the server - send update command
            // call commit with parameter true if the synchronization with the server is successful
            // and with parameter false if the synchronization failder.
            commit(true);
        }
    };

    // initialize the input fields.
    $("#firstName").addClass('jqx-input');
    $("#lastName").addClass('jqx-input');
    $("#product").addClass('jqx-input');
    $("#firstName").width(150);
    $("#firstName").height(23);
    $("#lastName").width(150);
    $("#lastName").height(23);
    $("#product").width(150);
    $("#product").height(23);

    if (theme.length > 0) {
        $("#firstName").addClass('jqx-input-' + theme);
        $("#lastName").addClass('jqx-input-' + theme);
        $("#product").addClass('jqx-input-' + theme);
    }

    $("#quantity").jqxNumberInput({ width: 150, height: 23, theme: theme, decimalDigits: 0, spinButtons: true });
    $("#price").jqxNumberInput({ width: 150, height: 23, theme: theme, spinButtons: true });

    var dataAdapter = new $.jqx.dataAdapter(source);
    var editrow = -1;

    // initialize jqxGrid
    $("#jqxgrid").jqxGrid(
    {
        width: 670,
        source: dataAdapter,
        theme: theme,
        pageable: true,
        autoheight: true,
        columns: [
          { text: 'First Name', datafield: 'firstname', width: 100 },
          { text: 'Last Name', datafield: 'lastname', width: 100 },
          { text: 'Product', datafield: 'productname', width: 190 },
          { text: 'Quantity', datafield: 'quantity', width: 90, cellsalign: 'right' },
          { text: 'Price', datafield: 'price', width: 90, cellsalign: 'right', cellsformat: 'c2' },
         { text: 'Edit', datafield: 'Edit', columntype: 'button', cellsrenderer: function () {
             return "Edit";
         }, buttonclick: function (row) {
             // open the popup window when the user clicks a button.
             editrow = row;
             var offset = $("#jqxgrid").offset();
             $("#popupWindow").jqxWindow({ position: { x: parseInt(offset.left) + 60, y: parseInt(offset.top) + 60} });

             // get the clicked row's data and initialize the input fields.
             var dataRecord = $("#jqxgrid").jqxGrid('getrowdata', editrow);
             $("#firstName").val(dataRecord.firstname);
             $("#lastName").val(dataRecord.lastname);
             $("#product").val(dataRecord.productname);
             $("#quantity").jqxNumberInput({ decimal: dataRecord.quantity });
             $("#price").jqxNumberInput({ decimal: dataRecord.price });

             // show the popup window.
             $("#popupWindow").jqxWindow('show');
         }
         },
         { text: 'Delete', datafield: 'Delete', columntype: 'button', cellsrenderer: function( ) {
            return "Delete";
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
            var row = { firstname: $("#firstName").val(), lastname: $("#lastName").val(), productname: $("#product").val(),
                quantity: parseInt($("#quantity").jqxNumberInput('decimal')), price: parseFloat($("#price").jqxNumberInput('decimal'))
            };
            $('#jqxgrid').jqxGrid('updaterow', editrow, row);
            $("#popupWindow").jqxWindow('hide');
        }
    });
});
