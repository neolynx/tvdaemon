Ext.require([
    'Ext.grid.*',
    'Ext.data.*',
    //'Ext.util.*',
    //'Ext.state.*'
]);

Ext.define('Source', {
    extend: 'Ext.data.Model',
    fields: [{
        name: 'id',
        type: 'int'
    }, {
        name: 'name'
    }, {
        name: 'type',
        type: 'int'
    }]
});

Ext.onReady(function() {
    //Ext.QuickTips.init();

    // setup the state provider, all state information will be saved to a cookie
    //Ext.state.Manager.setProvider(Ext.create('Ext.state.CookieProvider'));

    /**
     * Custom function used for column renderer
     * @param {Object} val
     */
    function change(val) {
        if (val > 0) {
            return '<span style="color:green;">' + val + '</span>';
        } else if (val < 0) {
            return '<span style="color:red;">' + val + '</span>';
        }
        return val;
    }

    /**
     * Custom function used for column renderer
     * @param {Object} val
     */
    function pctChange(val) {
        if (val > 0) {
            return '<span style="color:green;">' + val + '%</span>';
        } else if (val < 0) {
            return '<span style="color:red;">' + val + '%</span>';
        }
        return val;
    }

    var store = Ext.create('Ext.data.JsonStore', {
        // store configs
        autoDestroy: true,
        model: 'Source',
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
        remoteSort: true,
        sorters: [{
            property: 'name',
            direction: 'ASC'
        }],
        pageSize: 50
    });

    // create the Grid
    var grid = Ext.create('Ext.grid.Panel', {
        store: store,
        stateful: true,
        collapsible: false,
        multiSelect: true,
        stateId: 'stateGrid',
        columns: [
            {
                text     : 'Source',
                flex     : 1,
                sortable : false,
                dataIndex: 'name',
                renderer: function (value, metaData, record, rowIndex, colIndex, store) {
                    return '<a href="site?s=source&source_id=' + record.get('id') + '">'+value+'</a>';
                }
            },
            {
                text     : 'Type',
                width    : 75,
                sortable : true,
                dataIndex: 'type'
            },
        ],
        height: 350,
        width: 600,
        title: 'TV Sources',
        renderTo: 'source',
        viewConfig: {
            stripeRows: true,
            enableTextSelection: true
        }
    });
    store.load( );
});

