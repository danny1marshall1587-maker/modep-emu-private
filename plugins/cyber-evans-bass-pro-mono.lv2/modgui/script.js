function (event, funcs) {
    var pedal = event.icon;
    if (!pedal || !pedal.length) return;

    var ROT_MIN = -140;
    var ROT_MAX =  140;
    var ROT_RANGE = ROT_MAX - ROT_MIN;

    var dialMap = {};

    var rotaryNames = ['OFF', 'SLOW', 'FAST'];
    var rotaryClasses = ['', 'slow', 'fast'];

    // All slider ports
    var nsSymbols = [
        'ns_sub_oct', 'ns_sub_fifth', 'ns_sub_third', 'ns_root',
        'ns_third', 'ns_fifth', 'ns_oct', 'ns_oct_third', 'ns_oct_fifth', 'ns_dbl_oct'
    ];
    var dbSymbols = [
        'db_16', 'db_5_3', 'db_8', 'db_4', 'db_2_3', 'db_2', 'db_1_5', 'db_1_3', 'db_1'
    ];

    var noteNames = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'];
    var currentRoot = pedal.data('ebp-root');
    if (typeof currentRoot === 'undefined') currentRoot = -1;
    var currentMinor = pedal.data('ebp-minor') || false;

    function updateChordDisplay() {
        var el = pedal.find('#ebp-chord-readout');
        console.log('[EBP-MONO] updateChordDisplay: el.length=' + el.length + ' root=' + currentRoot + ' minor=' + currentMinor);
        if (!el.length) return;
        if (currentRoot >= 0 && currentRoot < 12) {
            var str = noteNames[currentRoot] + (currentMinor ? 'm' : '');
            el.text(str);
            console.log('[EBP-MONO] chord display set to: ' + str);
        } else {
            el.text('--');
        }
    }

    function getPct(val, min, max) {
        var clamped = Math.max(min, Math.min(max, val));
        return ((clamped - min) / (max - min)) * 100.0;
    }

    function updateSliderDisplay(sym, val) {
        var thumb = pedal.find('#ebp-thumb-' + sym);
        if (!thumb.length) return;
        var min = parseFloat(thumb.attr('data-min')) || 0;
        var max = parseFloat(thumb.attr('data-max')) || 10;
        var fVal = parseFloat(val) || 0;
        var pct = getPct(fVal, min, max);

        var track = thumb.closest('.ebp-vtrack');
        var trackH = track.height() || 100;
        var thumbH = thumb.outerHeight() || 18;
        var bottomPx = (pct / 100.0) * (trackH - thumbH);

        thumb.css('bottom', bottomPx + 'px');
        pedal.find('#ebp-fill-' + sym).css('height', (bottomPx + thumbH / 2) + 'px');
        pedal.find('#ebp-val-' + sym).text(Math.round(fVal * 10) / 10);
    }

    function updateKnobDisplay(dial, val) {
        var minVal = parseFloat(dial.attr('data-min')) || 0;
        var maxVal = parseFloat(dial.attr('data-max')) || 10;
        var clamped = Math.max(minVal, Math.min(maxVal, val));
        var norm = (clamped - minVal) / (maxVal - minVal);
        var deg = ROT_MIN + norm * ROT_RANGE;
        dial.find('.ebp-dial-rotor').css('transform', 'translate(-50%, -100%) rotate(' + deg + 'deg)');
        dial.data('current-val', clamped);

        var sym = dial.attr('data-sym');
        if (sym) {
            pedal.find('#ebp-val-' + sym).text(Math.round(clamped * 10) / 10);
        }
        return clamped;
    }

    function updateRotaryDisplay(val) {
        var idx = Math.max(0, Math.min(2, Math.round(parseFloat(val) || 0)));
        pedal.find('#ebp-rotary-txt').text(rotaryNames[idx]);
        var icon = pedal.find('#ebp-rotary-icon');
        icon.removeClass('slow fast');
        if (idx > 0) icon.addClass(rotaryClasses[idx]);
    }

    function updateThirdPills(val) {
        var v = Math.round(parseFloat(val) || 0);
        pedal.find('#ebp-q-auto, #ebp-q-min, #ebp-q-maj').removeClass('active');
        if (v === 0) pedal.find('#ebp-q-auto').addClass('active');
        else if (v === 1) pedal.find('#ebp-q-min').addClass('active');
        else pedal.find('#ebp-q-maj').addClass('active');
    }

    function sendPortValue(symbol, value) {
        if (funcs && typeof funcs.set_port_value === 'function') {
            funcs.set_port_value(symbol, value);
        } else if (event && typeof event.set_port_value === 'function') {
            event.set_port_value(symbol, value);
        }
        var w = pedal.find('.mod-knob-image[mod-port-symbol="' + symbol + '"]');
        if (w.length) w.val(value).trigger('change');
    }

    function applyPortValue(symbol, value) {
        if (!pedal || !pedal.length) return;
        var fVal = parseFloat(value);
        if (isNaN(fVal)) return;

        if (nsSymbols.indexOf(symbol) !== -1 || dbSymbols.indexOf(symbol) !== -1) {
            updateSliderDisplay(symbol, fVal);
            return;
        }

        
        if (symbol === 'preset') {
            var sel = pedal.find('#ebp-select-inst');
            if (sel.length) {
                sel.val(Math.round(fVal));
                pedal.find('#ebp-inst-display').text(sel.find('option:selected').text());
            }
            return;
        }
if (symbol === 'rotary') {
            updateRotaryDisplay(fVal);
            return;
        }

        if (symbol === 'third_mode') {
            updateThirdPills(fVal);
            return;
        }

        if (symbol === 'key_root') {
            pedal.find('#ebp-select-key').val(Math.round(fVal));
            return;
        }

        if (symbol === 'scale_mode') {
            pedal.find('#ebp-select-scale').val(Math.round(fVal));
            return;
        }

        if (symbol === 'bass_pattern') {
            pedal.find('#ebp-select-bass-pattern').val(Math.round(fVal));
            return;
        }

        if (symbol === 'ballad_relax') {
            var bActive = fVal > 0.5;
            pedal.find('#ebp-btn-ballad-relax').toggleClass('ebp-btn-active', bActive);
            return;
        }

        if (symbol === 'fretless_express') {
            var fActive = fVal > 0.5;
            pedal.find('#ebp-btn-fretless-express').toggleClass('ebp-btn-active', fActive);
            return;
        }

        if (symbol === 'working_memory') {
            var mActive = fVal > 0.5;
            pedal.find('#ebp-btn-working-memory').toggleClass('ebp-btn-active', mActive);
            return;
        }

        if (symbol === 'solo_mode') {
            var sActive = fVal > 0.5;
            pedal.find('#ebp-btn-solo-mode').toggleClass('ebp-btn-active', sActive);
            return;
        }

        if (symbol === 'song_record') {
            var rActive = fVal > 0.5;
            pedal.find('#ebp-btn-song-record').toggleClass('ebp-btn-active', rActive);
            return;
        }

        if (symbol === 'song_slot') {
            var sSlot = Math.round(fVal);
            pedal.find('#ebp-select-song-slot').val(sSlot);
            return;
        }

        if (symbol === 'bass_mute') {
            var muteActive = fVal > 0.5;
            pedal.find('#ebp-btn-bass-mute').toggleClass('ebp-btn-active', muteActive);
            return;
        }

        if (symbol === 'adventurous_mode') {
            var advActive = fVal > 0.5;
            pedal.find('#ebp-btn-adventurous').toggleClass('ebp-btn-active', advActive);
            return;
        }

        if (symbol === 'detected_root') {
            currentRoot = Math.round(fVal);
            pedal.data('ebp-root', currentRoot);
            updateChordDisplay();
            return;
        }

        if (symbol === 'detected_quality') {
            currentMinor = (fVal > 0.5);
            pedal.data('ebp-minor', currentMinor);
            updateChordDisplay();
            return;
        }

        if (dialMap[symbol]) {
            updateKnobDisplay(dialMap[symbol], fVal);
        } else {
            var dial = pedal.find('.ebp-dial[data-sym="' + symbol + '"]');
            if (dial.length) {
                dialMap[symbol] = dial;
                updateKnobDisplay(dial, fVal);
            }
        }

    }

    // 1. Host sync
    if (event.type === 'change' && event.symbol) {
        console.log('[EBP-MONO] change event: symbol=' + event.symbol + ' value=' + event.value);
        applyPortValue(event.symbol, event.value);
        return;
    }

    // 2. Hidden port changes
    pedal.find('.mod-knob-image').off('change.ebp valuechange.ebp').on('change.ebp valuechange.ebp', function () {
        var sym = $(this).attr('mod-port-symbol');
        var val = $(this).val();
        if (sym && typeof val !== 'undefined') applyPortValue(sym, val);
    });

    // 3. Slider drag interaction for Note Selector & Drawbars
    var allSliders = nsSymbols.concat(dbSymbols);
    allSliders.forEach(function (sym) {
        var thumb = pedal.find('#ebp-thumb-' + sym);
        if (!thumb.length) return;
        var track = thumb.closest('.ebp-vtrack');
        var min = parseFloat(thumb.attr('data-min')) || 0;
        var max = parseFloat(thumb.attr('data-max')) || 10;
        var range = max - min;

        thumb.off('mousedown.ebp touchstart.ebp').on('mousedown.ebp touchstart.ebp', function (e) {
            e.preventDefault();
            e.stopPropagation();
            var startY = (e.touches && e.touches.length) ? e.touches[0].clientY : e.clientY;
            var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="' + sym + '"]').val());
            if (isNaN(hiddenVal)) hiddenVal = parseFloat(thumb.attr('data-def')) || 0;
            var startVal = hiddenVal;

            $(window).off('.ebp_drag_' + sym);
            $(window).on('mousemove.ebp_drag_' + sym + ' touchmove.ebp_drag_' + sym, function (ev) {
                var curY = (ev.touches && ev.touches.length) ? ev.touches[0].clientY : ev.clientY;
                var deltaY = startY - curY; // up increases value
                var sensitivity = 80.0;
                var deltaVal = (deltaY / sensitivity) * range;
                var newVal = Math.max(min, Math.min(max, startVal + deltaVal));
                newVal = Math.round(newVal * 10) / 10;
                updateSliderDisplay(sym, newVal);
                sendPortValue(sym, newVal);
            });
            $(window).on('mouseup.ebp_drag_' + sym + ' touchend.ebp_drag_' + sym, function () {
                $(window).off('.ebp_drag_' + sym);
            });
        });

        track.off('click.ebp').on('click.ebp', function (e) {
            if ($(e.target).hasClass('ebp-thumb')) return;
            var rect = track[0].getBoundingClientRect();
            var relY = e.clientY - rect.top;
            var trackH = rect.height;
            var pct = 1.0 - (relY / trackH);
            var newVal = Math.max(min, Math.min(max, min + pct * range));
            newVal = Math.round(newVal * 10) / 10;
            updateSliderDisplay(sym, newVal);
            sendPortValue(sym, newVal);
        });

        thumb.off('wheel.ebp').on('wheel.ebp', function (e) {
            e.preventDefault();
            var delta = e.originalEvent.deltaY < 0 ? (range / 20.0) : -(range / 20.0);
            var cur = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="' + sym + '"]').val()) || 0;
            var newVal = Math.max(min, Math.min(max, cur + delta));
            newVal = Math.round(newVal * 10) / 10;
            updateSliderDisplay(sym, newVal);
            sendPortValue(sym, newVal);
        });
    });

    // 4. Performance Knobs (click/drag/wheel)
    function initKnobs() {
        pedal.find('.ebp-dial').each(function () {
            var dial = $(this);
            var sym = dial.attr('data-sym');
            if (!sym) return;
            dialMap[sym] = dial;

            var minVal = parseFloat(dial.attr('data-min')) || 0;
            var maxVal = parseFloat(dial.attr('data-max')) || 10;
            var defVal = parseFloat(dial.attr('data-def')) || 5;
            var curVal = dial.data('current-val');
            if (typeof curVal === 'undefined') {
                curVal = defVal;
                updateKnobDisplay(dial, curVal);
            }

            dial.off('mousedown.ebp touchstart.ebp').on('mousedown.ebp touchstart.ebp', function (e) {
                e.preventDefault();
                e.stopPropagation();
                var startY = (e.touches && e.touches.length) ? e.touches[0].clientY : e.clientY;
                var startVal = dial.data('current-val');
                if (typeof startVal === 'undefined') startVal = defVal;
                var range = maxVal - minVal;

                $(window).off('.ebpk_' + sym);
                $(window).on('mousemove.ebpk_' + sym + ' touchmove.ebpk_' + sym, function (ev) {
                    var curY = (ev.touches && ev.touches.length) ? ev.touches[0].clientY : ev.clientY;
                    var deltaY = startY - curY;
                    var sensitivity = 150.0;
                    var deltaVal = (deltaY / sensitivity) * range;
                    var newVal = Math.max(minVal, Math.min(maxVal, startVal + deltaVal));
                    newVal = Math.round(newVal * 10) / 10;
                    updateKnobDisplay(dial, newVal);
                    sendPortValue(sym, newVal);
                });
                $(window).on('mouseup.ebpk_' + sym + ' touchend.ebpk_' + sym, function () {
                    $(window).off('.ebpk_' + sym);
                });
            });

            dial.off('wheel.ebp').on('wheel.ebp', function (e) {
                e.preventDefault();
                var step = (maxVal - minVal) / 50.0;
                var cur = dial.data('current-val');
                if (typeof cur === 'undefined') cur = defVal;
                var newVal = Math.max(minVal, Math.min(maxVal, cur + (e.originalEvent.deltaY < 0 ? step : -step)));
                newVal = Math.round(newVal * 10) / 10;
                updateKnobDisplay(dial, newVal);
                sendPortValue(sym, newVal);
            });

            dial.off('dblclick.ebp').on('dblclick.ebp', function (e) {
                e.preventDefault();
                updateKnobDisplay(dial, defVal);
                sendPortValue(sym, defVal);
            });
        });
    }

    initKnobs();

    // 5. Rotary button toggle
    pedal.find('#ebp-rotary-btn').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var cur = parseInt(pedal.find('.mod-knob-image[mod-port-symbol="rotary"]').val()) || 0;
        var next = (cur + 1) % 3;
        sendPortValue('rotary', next);
        updateRotaryDisplay(next);
    });

    // 6. 3rd quality pills
    pedal.find('#ebp-q-auto').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        sendPortValue('third_mode', 0);
        updateThirdPills(0);
    });
    pedal.find('#ebp-q-min').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        sendPortValue('third_mode', 1);
        updateThirdPills(1);
    });
    pedal.find('#ebp-q-maj').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        sendPortValue('third_mode', 2);
        updateThirdPills(2);
    });

    // 7. Key and Scale dropdown listeners
    pedal.find('#ebp-select-key').off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var val = parseInt($(this).val()) || 0;
        sendPortValue('key_root', val);
    });
    pedal.find('#ebp-select-scale').off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var val = parseInt($(this).val()) || 0;
        sendPortValue('scale_mode', val);
    });

    // 8. Bass Pattern & Tap Tempo listeners
    pedal.find('#ebp-select-bass-pattern').off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var val = parseInt($(this).val()) || 0;
        sendPortValue('bass_pattern', val);
    });

    pedal.find('#ebp-tap-btn').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        sendPortValue('bass_tap', 1.0);
        setTimeout(function () {
            sendPortValue('bass_tap', 0.0);
        }, 50);
    });

    // Verse / Ballad Relax toggle
    pedal.find('#ebp-btn-ballad-relax').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="ballad_relax"]').val()) || 0.0;
        var newVal = (hiddenVal > 0.5) ? 0.0 : 1.0;
        sendPortValue('ballad_relax', newVal);
        $(this).toggleClass('ebp-btn-active', newVal > 0.5);
    });

    // Fretless Expression toggle
    pedal.find('#ebp-btn-fretless-express').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="fretless_express"]').val()) || 0.0;
        var newVal = (hiddenVal > 0.5) ? 0.0 : 1.0;
        sendPortValue('fretless_express', newVal);
        $(this).toggleClass('ebp-btn-active', newVal > 0.5);
    });

    // Working Memory & Learn toggle
    pedal.find('#ebp-btn-working-memory').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="working_memory"]').val()) || 1.0;
        var newVal = (hiddenVal > 0.5) ? 0.0 : 1.0;
        sendPortValue('working_memory', newVal);
        $(this).toggleClass('ebp-btn-active', newVal > 0.5);
    });

    // Bass Solo Mode (Momentary Footswitchable)
    var soloBtn = pedal.find('#ebp-btn-solo-mode');
    function setSoloState(active) {
        sendPortValue('solo_mode', active ? 1.0 : 0.0);
        soloBtn.toggleClass('ebp-btn-active', active);
    }
    soloBtn.off('mousedown.ebp touchstart.ebp mouseup.ebp touchend.ebp mouseleave.ebp touchcancel.ebp')
        .on('mousedown.ebp touchstart.ebp', function (e) {
            e.preventDefault();
            e.stopPropagation();
            setSoloState(true);
        })
        .on('mouseup.ebp touchend.ebp mouseleave.ebp touchcancel.ebp', function (e) {
            e.preventDefault();
            e.stopPropagation();
            setSoloState(false);
        });

    // Record Song Toggle
    pedal.find('#ebp-btn-song-record').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="song_record"]').val()) || 0.0;
        var newVal = (hiddenVal > 0.5) ? 0.0 : 1.0;
        sendPortValue('song_record', newVal);
        $(this).toggleClass('ebp-btn-active', newVal > 0.5);
    });

    // Song Slot Selector
    pedal.find('#ebp-select-song-slot').off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var val = parseInt($(this).val()) || 0;
        sendPortValue('song_slot', val);
    });

    // Bass Mute Toggle
    pedal.find('#ebp-btn-bass-mute').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="bass_mute"]').val()) || 0.0;
        var newVal = (hiddenVal > 0.5) ? 0.0 : 1.0;
        sendPortValue('bass_mute', newVal);
        $(this).toggleClass('ebp-btn-active', newVal > 0.5);
    });

    // Adventurous / Virtuoso Mode Toggle
    pedal.find('#ebp-btn-adventurous').off('click.ebp').on('click.ebp', function (e) {
        e.stopPropagation();
        var hiddenVal = parseFloat(pedal.find('.mod-knob-image[mod-port-symbol="adventurous_mode"]').val()) || 0.0;
        var newVal = (hiddenVal > 0.5) ? 0.0 : 1.0;
        sendPortValue('adventurous_mode', newVal);
        $(this).toggleClass('ebp-btn-active', newVal > 0.5);
    });

    
    // Pro listeners
    pedal.find('#ebp-select-track-mode').off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var val = parseInt($(this).val()) || 0;
        sendPortValue('track_mode', val);
    });

    pedal.find('#ebp-select-performer-mode').off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var val = parseInt($(this).val()) || 0;
        sendPortValue('performer_mode', val);
    });

    // MIDI File Selection handling
    var midiSelect = pedal.find('#ebp-select-midi-file');
    var nativePicker = pedal.find('.ebp-midi-file-picker');


    function syncMidiFilesFromApi() {
        if (!midiSelect.length) return;
        $.ajax({
            url: '/files/list',
            data: { 'types': 'midiclip,midisong' },
            dataType: 'json',
            cache: false,
            success: function (data) {
                if (data && data.ok && data.files) {
                    var curVal = midiSelect.val();
                    midiSelect.empty();
                    midiSelect.append($('<option>', { value: '', text: '-- No File / Off --' }));
                    for (var fi = 0; fi < data.files.length; fi++) {
                        var fileObj = data.files[fi];
                        midiSelect.append($('<option>', {
                            value: fileObj.fullname,
                            text: fileObj.basename
                        }));
                    }
                    if (curVal) midiSelect.val(curVal);
                }
            }
        });
    }

    // Populate from native picker options if available or from API
    if (nativePicker.length && nativePicker.find('[mod-role="enumeration-option"]').length) {
        var nativeOpts = nativePicker.find('[mod-role="enumeration-option"]');
        midiSelect.empty();
        midiSelect.append($('<option>', { value: '', text: '-- No File / Off --' }));
        nativeOpts.each(function () {
            var fullVal = $(this).attr('mod-parameter-value');
            var baseName = $(this).text();
            midiSelect.append($('<option>', { value: fullVal, text: baseName }));
        });
    }
    syncMidiFilesFromApi();

    midiSelect.off('change.ebp').on('change.ebp', function (e) {
        e.stopPropagation();
        var selectedPath = $(this).val() || '';
        var paramUri = 'http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono#midi_file';
        if (event && typeof event.set_parameter_value === 'function') {
            event.set_parameter_value(paramUri, selectedPath);
        } else if (funcs && typeof funcs.set_parameter_value === 'function') {
            funcs.set_parameter_value(paramUri, selectedPath);
        }
        if (nativePicker.length) {
            nativePicker.find('[mod-role="enumeration-option"][mod-parameter-value="' + selectedPath + '"]').trigger('click');
        }
    });

    // Prevent drag handle clicks from stealing events on controls
    pedal.find('.ebp-pill, .ebp-rotary-btn, .ebp-dial, .ebp-thumb, .ebp-select').on('mousedown touchstart pointerdown', function (e) {
        e.stopPropagation();
    });


    // 7. Initialise on start
    if (event.type === 'start') {
        console.log('[EBP-MONO] START event fired. ports count: ' + (event.ports ? event.ports.length : 0));
        if (event.ports) {
            for (var i = 0; i < event.ports.length; i++) {
                var p = event.ports[i];
                applyPortValue(p.symbol, p.value);
            }
        }
        updateChordDisplay();
    }

    event.handle_event = function (symbol, value) {
        applyPortValue(symbol, value);
    };
    pedal.data('handle_event', event.handle_event);
}

