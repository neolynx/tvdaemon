
Ext.Loader.setConfig({
        enabled: true
});

Ext.application({
    name: 'TVDaemon',

    autoCreateViewport: true,

    controllers: [
        'Main'
    ]
});
