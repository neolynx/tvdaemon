Ext.define('TVDaemon.model.Restaurant', {
    extend: 'Ext.data.Model',
    fields: ['name', 'cuisine', 'description', {name:'rating', type:'float'}]
});
