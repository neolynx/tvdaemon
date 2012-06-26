Ext.define('TVDaemon.view.examples.Sources', {
    extend: 'TVDaemon.view.examples.Example',
    requires: [
        'Ext.grid.Panel',
        'TVDaemon.store.Sources'
    ],

    cls: 'sources',

    items: [
        {
            xtype: 'grid',

            title: 'TV Sources',
            frame: true,

            store: 'Sources',

            listeners: {
              itemclick : function(view,rec,item,index,eventObj) {
                view.fireEvent('source', rec);
              }
            },

            columns: [
              {
                  text     : 'Source',
                  flex     : 1,
                  sortable : false,
                  dataIndex: 'name',
                  //renderer: function (value, metaData, record, rowIndex, colIndex, store) {
                      //return '<a href="#source&source_id=' + record.get('id') + '">'+value+'</a>';
                  //}
              },
              {
                  text     : 'Type',
                  width    : 75,
                  sortable : true,
                  dataIndex: 'type'
              },
            ],
        }
    ]
});
