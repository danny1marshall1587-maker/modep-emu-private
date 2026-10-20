function (event, funcs) {
    var pedal = event.icon;
    if (!pedal || !pedal.length) return;

    var chNames = ["'65 BLACKFACE", "'59 BASSMAN/JMP", "JUMPERED BLEND"];
    var qualityNames = ["1x ECO LIVE", "2x STAGE", "4x STUDIO", "8x ULTRA"];
    var cabNames = ["2x12 C12N", "4x10 P10R", "4x12 G12M", "BYPASS"];

    // 4-Page Tab Switching
    pedal.find('.uh-page-tab').off('click.uhtab').on('click.uhtab', function (e) {
        e.stopPropagation();
        var pageIdx = $(this).attr('data-uh-tab');
        var colors = ['#0284c7', '#d97706', '#059669', '#7c3aed'];
        pedal.find('.uh-page-tab').each(function () {
            var t = $(this);
            var isAct = (t.attr('data-uh-tab') === pageIdx);
            t.css({
                background: isAct ? colors[parseInt(pageIdx, 10)] : '#1e293b',
                color: isAct ? '#ffffff' : '#cbd5e1',
                borderColor: isAct ? '#f8fafc' : '#475569'
            });
        });
        pedal.find('.uh-page-view').each(function () {
            var v = $(this);
            v.css('display', (v.attr('data-uh-page') === pageIdx) ? 'block' : 'none');
        });
    });

    function applyPortValue(symbol, value) {
        var fVal = parseFloat(value);
        if (isNaN(fVal)) return;
        if (symbol === 'channel_mode') {
            var cidx = Math.max(0, Math.min(2, Math.round(fVal)));
            pedal.find('#uh-val-channel_mode').text(chNames[cidx]);
        } else if (symbol === 'quality_mode') {
            var qidx = Math.max(0, Math.min(3, Math.round(fVal)));
            pedal.find('#uh-val-quality_mode').text(qualityNames[qidx]);
        } else if (symbol === 'speaker_cab') {
            var sidx = Math.max(0, Math.min(3, Math.round(fVal)));
            pedal.find('#uh-val-speaker_cab').text(cabNames[sidx]);
        } else if (symbol === 'bright_sw') {
            pedal.find('#uh-val-bright_sw').text(fVal > 0.5 ? '120pF ON' : 'OFF');
        } else if (symbol === 'slope_mode') {
            pedal.find('#uh-val-slope_mode').text(fVal > 0.5 ? '33k JMP' : '56k TWEED');
        } else if (symbol === 'power_tubes') {
            pedal.find('#uh-val-power_tubes').text(fVal > 0.5 ? '2x EL34' : '2x 6L6GC');
        } else if (symbol === 'trem_mode') {
            pedal.find('#uh-val-trem_mode').text(fVal > 0.5 ? 'OPTO TREM' : 'HARMONIC TUBE');
        }
    }

    if (event.type === 'change' && event.symbol) {
        applyPortValue(event.symbol, event.value);
        return;
    }

    if (event.type === 'start' && event.ports) {
        for (var i = 0; i < event.ports.length; i++) {
            applyPortValue(event.ports[i].symbol, event.ports[i].value);
        }
    }

    event.handle_event = function (symbol, value) {
        applyPortValue(symbol, value);
    };
}
