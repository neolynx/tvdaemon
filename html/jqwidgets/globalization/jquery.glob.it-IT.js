
(function($) {
    var cultures = $.global.cultures,
        en = cultures.en,
        standard = en.calendars.standard,
        culture = cultures["it-IT"] = $.extend(true, {}, en, {
        name: "it-IT",
        englishName: "Italian (Italy)",
        nativeName: "italiano (Italia)",
        language: "it",
        numberFormat: {
            ',': ".",
            '.': ",",
            percent: {
                pattern: ["-n%","n%"],
                ',': ".",
                '.': ","
            },
            currency: {
                pattern: ["-$ n","$ n"],
                ',': ".",
                '.': ",",
                symbol: "€"
            }
        },
        calendars: {
            standard: $.extend(true, {}, standard, {
                firstDay: 1,
                days: {
                    names: ["domenica","lunedì","martedì","mercoledì","giovedì","venerdì","sabato"],
                    namesAbbr: ["dom","lun","mar","mer","gio","ven","sab"],
                    namesShort: ["do","lu","ma","me","gi","ve","sa"]
                },
                months: {
                    names: ["gennaio","febbraio","marzo","aprile","maggio","giugno","luglio","agosto","settembre","ottobre","novembre","dicembre",""],
                    namesAbbr: ["gen","feb","mar","apr","mag","giu","lug","ago","set","ott","nov","dic",""]
                },
                AM: null,
                PM: null,
                eras: [{"name":"d.C.","start":null,"offset":0}],
                patterns: {
                    d: "dd/MM/yyyy",
                    D: "dddd d MMMM yyyy",
                    t: "HH:mm",
                    T: "HH:mm:ss",
                    f: "dddd d MMMM yyyy HH:mm",
                    F: "dddd d MMMM yyyy HH:mm:ss",
                    M: "dd MMMM",
                    Y: "MMMM yyyy"
                }
            })
        }
    }, cultures["it-IT"]);
    culture.calendar = culture.calendars.standard;
})(jQuery);