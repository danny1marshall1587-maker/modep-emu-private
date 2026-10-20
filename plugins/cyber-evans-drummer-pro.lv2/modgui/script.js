function (event, funcs) {
    var pedal = event.icon;
    if (!pedal || !pedal.length) return;

    var ROT_MIN = -140;
    var ROT_MAX =  140;
    var ROT_RANGE = ROT_MAX - ROT_MIN;

    var dialMap = {};

    function getPct(val, min, max) {
        var clamped = Math.max(min, Math.min(max, val));
        return (clamped - min) / (max - min);
    }

    function updateKnobDisplay(sym, val) {
        var dial = dialMap[sym];
        if (!dial || !dial.length) return;
        var min = parseFloat(dial.attr('data-min')) || 0;
        var max = parseFloat(dial.attr('data-max')) || 10;
        var fVal = parseFloat(val);
        if (isNaN(fVal)) fVal = min;

        var pct = getPct(fVal, min, max);
        var rot = ROT_MIN + pct * ROT_RANGE;
        dial.find('.knob-rotor').css('transform', 'rotate(' + rot + 'deg)');

        var valEl = pedal.find('#edp-val-' + sym);
        if (valEl.length) {
            if (sym === 'bpm_override') {
                valEl.text(fVal < 50 ? 'AUTO' : Math.round(fVal));
            } else {
                valEl.text(fVal.toFixed(1));
            }
        }
    }

    function applyPortValue(sym, val) {
        var fVal = parseFloat(val);

        if (dialMap[sym]) {
            updateKnobDisplay(sym, fVal);
        } else if (sym === 'style') {
            pedal.find('#edp-select-style').val(Math.round(fVal));
        } else if (sym === 'detected_bpm') {
            pedal.find('#edp-bpm-readout').text(Math.round(fVal));
        } else if (sym === 'current_section') {
            var secStr = (Math.round(fVal) === 1) ? 'CHORUS' : 'VERSE';
            pedal.find('#edp-section-readout').text(secStr);
        } else if (sym === 'energy_level') {
            var pct = Math.max(5, Math.min(100, Math.round(fVal * 100)));
            pedal.find('#edp-energy-meter').css('width', pct + '%');
        } else if (sym === 'crash_on_chorus') {
            pedal.find('#edp-btn-crash').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'record_song') {
            pedal.find('#edp-btn-record').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'mute') {
            pedal.find('#edp-btn-mute').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'adventurous') {
            pedal.find('#edp-btn-adventurous').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'strum_only_mode') {
            pedal.find('#edp-btn-strumonly').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'stop_crash') {
            pedal.find('#edp-btn-stop').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'start_play') {
            pedal.find('#edp-btn-start').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'reset_listen') {
            pedal.find('#edp-btn-reset').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'fill_now') {
            pedal.find('#edp-btn-fill').toggleClass('edp-btn-active', fVal > 0.5);
        } else if (sym === 'manual_chorus') {
            pedal.find('#edp-btn-chorus').toggleClass('edp-btn-active', fVal > 0.5);
        }
    }

    // Set up rotary dials
    pedal.find('.custom-knob-dial').each(function () {
        var dial = $(this);
        var sym = dial.attr('data-symbol');
        if (!sym) return;
        dialMap[sym] = dial;

        var startY = 0;
        var startVal = 0;
        var isDragging = false;

        function onPointerMove(e) {
            if (!isDragging) return;
            var clientY = (e.touches && e.touches.length) ? e.touches[0].clientY : e.clientY;
            var dy = startY - clientY;
            var min = parseFloat(dial.attr('data-min')) || 0;
            var max = parseFloat(dial.attr('data-max')) || 10;
            var range = max - min;
            var deltaVal = (dy / 100.0) * range;
            var newVal = Math.max(min, Math.min(max, startVal + deltaVal));

            updateKnobDisplay(sym, newVal);
            event.set_port_value(sym, newVal);
        }

        function onPointerUp() {
            if (isDragging) {
                isDragging = false;
                $(document).off('.edp_knob');
            }
        }

        dial.on('mousedown touchstart', function (e) {
            e.preventDefault();
            e.stopPropagation();
            isDragging = true;
            startY = (e.touches && e.touches.length) ? e.touches[0].clientY : e.clientY;
            var portInput = pedal.find('.mod-knob-image[mod-port-symbol="' + sym + '"]');
            startVal = parseFloat(portInput.val()) || parseFloat(dial.attr('data-default')) || 0;

            $(document).on('mousemove.edp_knob touchmove.edp_knob', onPointerMove);
            $(document).on('mouseup.edp_knob touchend.edp_knob', onPointerUp);
        });
    });

    // Style dropdown
    pedal.find('#edp-select-style').on('change', function () {
        var v = parseInt($(this).val(), 10);
        event.set_port_value('style', v);
    });

    // Button Toggles
    pedal.find('.edp-btn-toggle').on('click', function (e) {
        e.preventDefault();
        var btn = $(this);
        var sym = btn.attr('data-sym');
        var wasActive = btn.hasClass('edp-btn-active');
        var newVal = wasActive ? 0.0 : 1.0;
        btn.toggleClass('edp-btn-active', !wasActive);
        event.set_port_value(sym, newVal);
    });

    // Handle incoming MOD-UI changes
    if (event.type === 'change') {
        applyPortValue(event.symbol, event.value);
    } else if (event.type === 'start') {
        pedal.find('.mod-knob-image[mod-role="input-control-port"]').each(function () {
            var sym = $(this).attr('mod-port-symbol');
            var val = $(this).val();
            if (sym && typeof val !== 'undefined') {
                applyPortValue(sym, parseFloat(val));
            }
        });
    }
}
