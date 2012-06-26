Ext.define('TVDaemon.store.Sources', {
    extend: 'Ext.data.Store',
    model: 'TVDaemon.model.Source',

    storeId: 'sources',

    proxy: {
        type: 'ajax',
        url: "tvd?s=source&a=list",
        reader: {
            type: 'json',
            root: 'data',
            idProperty: 'id',
            totalProperty: 'total'
        }
    },

    autoLoad: true,
    //autoDestroy: true,
    groupField: 'type',
    remoteSort: true,
    sorters: [{
      property: 'name',
      direction: 'ASC'
    }],
    pageSize: 50,
});

