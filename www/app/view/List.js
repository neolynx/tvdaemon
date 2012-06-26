Ext.define('TVDaemon.view.List', {
    extend: 'Ext.tree.Panel',
    xtype: 'exampleList',

    requires: [
        'TVDaemon.store.Examples',
        'TVDaemon.view.examples.Example',
        'TVDaemon.view.examples.Sources',
        'TVDaemon.view.examples.PanelExample',
        'TVDaemon.view.examples.forms.Contact',
        'TVDaemon.view.examples.forms.Login',
        'TVDaemon.view.examples.forms.Register',
        'TVDaemon.view.examples.grids.BasicGrid',
        'TVDaemon.view.examples.grids.GroupedGrid',
        'TVDaemon.view.examples.grids.GroupedHeaderGrid',
        'TVDaemon.view.examples.grids.LockedGrid',
        'TVDaemon.view.examples.panels.BasicPanel',
        'TVDaemon.view.examples.panels.FramedPanel',
        'TVDaemon.view.examples.tabs.BasicTabs',
        'TVDaemon.view.examples.tabs.FramedTabs',
        'TVDaemon.view.examples.tabs.IconTabs',
        'TVDaemon.view.examples.tabs.TitledTabPanels',
        'TVDaemon.view.examples.toolbars.BasicToolbar',
        'TVDaemon.view.examples.toolbars.DockedToolbar',
        'TVDaemon.view.examples.trees.BasicTree',
        'TVDaemon.view.examples.windows.BasicWindow'
    ],

    //title: 'Examples',
    rootVisible: false,

    cls: 'examples-list',

    lines: false,
    useArrows: true,

    store: 'Examples'
});
