/* Cyber System & Hardware Settings Manager - Client JS */
(function (window, $) {
    'use strict';

    function CyberSystemManagerUI() {
        this.isOpen = false;
        this.currentTab = 'wifi';
        this.selectedBuffer = 128;
        this.selectedCard = 'default';
        this.selectedSSID = '';
        this.init();
    }

    CyberSystemManagerUI.prototype.init = function () {
        var self = this;
        this.buildModal();
        this.bindEvents();
    };

    CyberSystemManagerUI.prototype.buildModal = function () {
        var html = [
            '<div id="cyber-sys-backdrop" class="cyber-sys-modal-backdrop"></div>',
            '<div id="cyber-sys-window" class="cyber-sys-window">',
            '  <div class="cyber-sys-header">',
            '    <div class="cyber-sys-title">',
            '      <span>&#9881;</span> SYSTEM & HARDWARE MANAGER',
            '    </div>',
            '    <button id="cyber-sys-close" class="cyber-sys-close-btn">&times;</button>',
            '  </div>',
            '  <div class="cyber-sys-tabs">',
            '    <button class="cyber-sys-tab-btn active" data-tab="wifi">Wi-Fi & Network</button>',
            '    <button class="cyber-sys-tab-btn" data-tab="audio">Audio & Buffer</button>',
            '    <button class="cyber-sys-tab-btn" data-tab="bank">Bank & MIDI Nav</button>',
            '    <button class="cyber-sys-tab-btn" data-tab="system">System & Power</button>',
            '  </div>',
            '  <div class="cyber-sys-body">',
            '    <!-- TAB 1: WI-FI -->',
            '    <div id="cyber-tab-wifi" class="cyber-sys-tab-pane active">',
            '      <div id="cyber-wifi-status-box" class="cyber-sys-status-box">',
            '        Current Mode: <span id="cyber-wifi-mode" class="cyber-sys-status-val">Checking...</span> | ',
            '        IP: <span id="cyber-wifi-ip" class="cyber-sys-status-val">---</span>',
            '      </div>',
            '      <div class="cyber-sys-group">',
            '        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">',
            '          <span class="cyber-sys-label" style="margin:0;">Available Networks</span>',
            '          <button id="cyber-wifi-scan-btn" class="cyber-sys-action-btn" style="padding:4px 10px; font-size:11px;">Scan Wi-Fi</button>',
            '        </div>',
            '        <div id="cyber-wifi-list" class="cyber-sys-wifi-list">',
            '          <div style="padding:16px; text-align:center; color:#888;">Click "Scan Wi-Fi" to discover nearby routers...</div>',
            '        </div>',
            '      </div>',
            '      <div class="cyber-sys-group">',
            '        <label class="cyber-sys-label">Selected Network Password</label>',
            '        <input type="password" id="cyber-wifi-password" class="cyber-sys-input" placeholder="Enter Wi-Fi password...">',
            '      </div>',
            '      <div style="display:flex; justify-content:space-between; gap:10px; margin-top:16px;">',
            '        <button id="cyber-wifi-connect-btn" class="cyber-sys-action-btn" style="flex:1;">Connect to Wi-Fi</button>',
            '        <button id="cyber-wifi-hotspot-btn" class="cyber-sys-action-btn" style="background:#4a5568; border-color:#718096;">Switch to Hotspot (172.24.1.1)</button>',
            '      </div>',
            '    </div>',
            '    <!-- TAB 2: AUDIO & BUFFER -->',
            '    <div id="cyber-tab-audio" class="cyber-sys-tab-pane">',
            '      <div class="cyber-sys-group">',
            '        <label class="cyber-sys-label">Audio Interface / Soundcard</label>',
            '        <select id="cyber-audio-card-select" class="cyber-sys-select">',
            '          <option value="hw:0">Default Soundcard</option>',
            '        </select>',
            '      </div>',
            '      <div class="cyber-sys-group">',
            '        <label class="cyber-sys-label">Buffer Size (Latency vs CPU)</label>',
            '        <div class="cyber-sys-options-row">',
            '          <button class="cyber-sys-option-btn" data-buffer="64">64 (1.3ms Ultra)</button>',
            '          <button class="cyber-sys-option-btn selected" data-buffer="128">128 (2.7ms Low)</button>',
            '          <button class="cyber-sys-option-btn" data-buffer="256">256 (5.3ms Safe)</button>',
            '          <button class="cyber-sys-option-btn" data-buffer="512">512 (10.7ms Studio)</button>',
            '        </div>',
            '      </div>',
            '      <div class="cyber-sys-group">',
            '        <label class="cyber-sys-label">Sample Rate</label>',
            '        <select id="cyber-audio-rate-select" class="cyber-sys-select">',
            '          <option value="48000" selected>48000 Hz (Recommended for MODEP / LV2)</option>',
            '          <option value="44100">44100 Hz (Standard CD Quality)</option>',
            '        </select>',
            '      </div>',
            '      <div style="margin-top:20px;">',
            '        <button id="cyber-audio-apply-btn" class="cyber-sys-action-btn" style="width:100%;">Apply & Restart Audio Engine</button>',
            '      </div>',
            '    </div>',
            '    <!-- TAB 3: BANK & MIDI NAVIGATION -->',
            '    <div id="cyber-tab-bank" class="cyber-sys-tab-pane">',
            '      <div class="cyber-sys-group">',
            '        <div class="cyber-sys-status-box">',
            '          Active Bank: <span id="cyber-bank-name" class="cyber-sys-status-val" style="color:#00ffcc; font-weight:bold;">Loading...</span><br>',
            '          Pedalboards in Bank: <span id="cyber-bank-pb-list" class="cyber-sys-status-val" style="color:#eee;">---</span>',
            '        </div>',
            '      </div>',
            '      <div class="cyber-sys-group" style="background:#151d28; padding:12px; border-radius:6px; border:1px solid #233549; margin-bottom:12px;">',
            '        <div style="display:flex; justify-content:space-between; align-items:center;">',
            '          <div>',
            '            <span style="font-weight:bold; color:#00ffcc; font-size:13px;">&#9654; NEXT PEDALBOARD</span><br>',
            '            <span id="cyber-bank-next-val" style="font-size:12px; color:#aaa;">Not mapped</span>',
            '          </div>',
            '          <div style="display:flex; gap:8px;">',
            '            <button id="cyber-bank-learn-next-btn" class="cyber-sys-action-btn" style="padding:4px 12px; font-size:11px;">MIDI Learn</button>',
            '            <button id="cyber-bank-test-next-btn" class="cyber-sys-action-btn" style="background:#2d3748; padding:4px 12px; font-size:11px;">Test Next &#9654;</button>',
            '          </div>',
            '        </div>',
            '      </div>',
            '      <div class="cyber-sys-group" style="background:#151d28; padding:12px; border-radius:6px; border:1px solid #233549; margin-bottom:16px;">',
            '        <div style="display:flex; justify-content:space-between; align-items:center;">',
            '          <div>',
            '            <span style="font-weight:bold; color:#00ffcc; font-size:13px;">&#9664; PREVIOUS PEDALBOARD</span><br>',
            '            <span id="cyber-bank-prev-val" style="font-size:12px; color:#aaa;">Not mapped</span>',
            '          </div>',
            '          <div style="display:flex; gap:8px;">',
            '            <button id="cyber-bank-learn-prev-btn" class="cyber-sys-action-btn" style="padding:4px 12px; font-size:11px;">MIDI Learn</button>',
            '            <button id="cyber-bank-test-prev-btn" class="cyber-sys-action-btn" style="background:#2d3748; padding:4px 12px; font-size:11px;">&#9664; Test Prev</button>',
            '          </div>',
            '        </div>',
            '      </div>',
            '      <div style="font-size:11px; color:#888; line-height:1.4;">',
            '        &#9432; Momentary footswitches (e.g. Nektar PACER) trigger cleanly on button press (CC &ge; 64). Reaching the end wraps automatically.',
            '      </div>',
            '    </div>',
            '    <!-- TAB 4: SYSTEM & POWER -->',
            '    <div id="cyber-tab-system" class="cyber-sys-tab-pane">',
            '      <div class="cyber-sys-group">',
            '        <div class="cyber-sys-status-box">',
            '          CPU Temp: <span id="cyber-sys-temp" class="cyber-sys-status-val">---</span><br>',
            '          Free Storage: <span id="cyber-sys-disk" class="cyber-sys-status-val">---</span>',
            '        </div>',
            '      </div>',
            '      <div style="display:flex; gap:10px; margin-top:25px;">',
            '        <button id="cyber-sys-reboot-btn" class="cyber-sys-action-btn cyber-sys-warn-btn" style="flex:1;">Reboot Pi</button>',
            '        <button id="cyber-sys-poweroff-btn" class="cyber-sys-action-btn cyber-sys-warn-btn" style="flex:1; background:#4a0510;">Power Off</button>',
            '      </div>',
            '    </div>',
            '  </div>',
            '</div>'
        ].join('\n');

        $('body').append(html);
    };

    CyberSystemManagerUI.prototype.bindEvents = function () {
        var self = this;

        // Close
        $('#cyber-sys-close, #cyber-sys-backdrop').on('click', function () {
            self.close();
        });

        // Tabs
        $('.cyber-sys-tab-btn').on('click', function () {
            var tab = $(this).attr('data-tab');
            $('.cyber-sys-tab-btn').removeClass('active');
            $(this).addClass('active');
            $('.cyber-sys-tab-pane').removeClass('active');
            $('#cyber-tab-' + tab).addClass('active');
            self.currentTab = tab;
            if (tab === 'audio') self.loadAudioInfo();
            if (tab === 'bank') self.loadBankNavInfo();
            if (tab === 'system') self.loadSystemStats();
        });

        // Buffer size buttons
        $('.cyber-sys-option-btn').on('click', function () {
            $('.cyber-sys-option-btn').removeClass('selected');
            $(this).addClass('selected');
            self.selectedBuffer = parseInt($(this).attr('data-buffer')) || 128;
        });

        // Bank Nav: Learn Next
        $('#cyber-bank-learn-next-btn').on('click', function () {
            var $btn = $(this);
            $btn.prop('disabled', true).text('Press Footswitch...');
            $.post('/system/bank_nav/learn', { target: 'next' }, function () {
                // Poll or listen for mapping
                var poll = setInterval(function () {
                    $.getJSON('/system/bank_nav', function (data) {
                        if (data.next_cc >= 0 && $btn.text() === 'Press Footswitch...') {
                            clearInterval(poll);
                            $btn.prop('disabled', false).text('MIDI Learn');
                            self.loadBankNavInfo();
                        }
                    });
                }, 1000);
                setTimeout(function () {
                    clearInterval(poll);
                    $btn.prop('disabled', false).text('MIDI Learn');
                }, 15000);
            });
        });

        // Bank Nav: Learn Prev
        $('#cyber-bank-learn-prev-btn').on('click', function () {
            var $btn = $(this);
            $btn.prop('disabled', true).text('Press Footswitch...');
            $.post('/system/bank_nav/learn', { target: 'prev' }, function () {
                var poll = setInterval(function () {
                    $.getJSON('/system/bank_nav', function (data) {
                        if (data.prev_cc >= 0 && $btn.text() === 'Press Footswitch...') {
                            clearInterval(poll);
                            $btn.prop('disabled', false).text('MIDI Learn');
                            self.loadBankNavInfo();
                        }
                    });
                }, 1000);
                setTimeout(function () {
                    clearInterval(poll);
                    $btn.prop('disabled', false).text('MIDI Learn');
                }, 15000);
            });
        });

        // Bank Nav: Test Next
        $('#cyber-bank-test-next-btn').on('click', function () {
            $.post('/system/bank_nav/trigger', { direction: 1 }, function () {
                setTimeout(function () { self.loadBankNavInfo(); }, 1500);
            });
        });

        // Bank Nav: Test Prev
        $('#cyber-bank-test-prev-btn').on('click', function () {
            $.post('/system/bank_nav/trigger', { direction: -1 }, function () {
                setTimeout(function () { self.loadBankNavInfo(); }, 1500);
            });
        });

        // Wi-Fi Scan
        $('#cyber-wifi-scan-btn').on('click', function () {
            self.scanWifi();
        });

        // Wi-Fi Connect
        $('#cyber-wifi-connect-btn').on('click', function () {
            var pwd = $('#cyber-wifi-password').val();
            if (!self.selectedSSID) {
                alert('Please select a Wi-Fi network from the list first.');
                return;
            }
            self.connectWifi(self.selectedSSID, pwd);
        });

        // Switch to Hotspot
        $('#cyber-wifi-hotspot-btn').on('click', function () {
            if (confirm('Switch Pi back to Standalone Hotspot mode (172.24.1.1)?')) {
                $.post('/system/wifi_hotspot', function () {
                    alert('Hotspot mode initiated. Reconnect your device to the "Patchbox" Wi-Fi!');
                });
            }
        });

        // Apply Audio Settings
        $('#cyber-audio-apply-btn').on('click', function () {
            var card = $('#cyber-audio-card-select').val();
            var rate = $('#cyber-audio-rate-select').val();
            self.applyAudioConfig(card, self.selectedBuffer, rate);
        });

        // Reboot / Poweroff
        $('#cyber-sys-reboot-btn').on('click', function () {
            if (confirm('Are you sure you want to reboot the Raspberry Pi?')) {
                $.post('/system/power', { action: 'reboot' });
            }
        });

        $('#cyber-sys-poweroff-btn').on('click', function () {
            if (confirm('Are you sure you want to power off the Raspberry Pi safely?')) {
                $.post('/system/power', { action: 'poweroff' });
            }
        });

        // Top Button / Action Trigger
        $(document).on('click', '#mod-sys-mgr-btn, .js-sys-mgr', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.open();
        });
    };

    CyberSystemManagerUI.prototype.open = function () {
        this.isOpen = true;
        $('#cyber-sys-backdrop, #cyber-sys-window').fadeIn(200);
        this.loadNetworkStatus();
        this.loadBankNavInfo();
    };

    CyberSystemManagerUI.prototype.close = function () {
        this.isOpen = false;
        $('#cyber-sys-backdrop, #cyber-sys-window').fadeOut(150);
    };

    CyberSystemManagerUI.prototype.loadNetworkStatus = function () {
        $.getJSON('/system/wifi_status', function (data) {
            $('#cyber-wifi-mode').text(data.is_hotspot ? 'Standalone Hotspot (Patchbox)' : 'Home Wi-Fi (' + data.ssid + ')');
            $('#cyber-wifi-ip').text(data.ip || '---');
        });
    };

    CyberSystemManagerUI.prototype.scanWifi = function () {
        var $list = $('#cyber-wifi-list');
        $list.html('<div style="padding:16px; text-align:center; color:#00b4d8;">Scanning wireless channels...</div>');
        var self = this;
        $.getJSON('/system/wifi_scan', function (networks) {
            if (!networks || !networks.length) {
                $list.html('<div style="padding:16px; text-align:center; color:#e76f51;">No networks found. Try again.</div>');
                return;
            }
            $list.empty();
            networks.forEach(function (net) {
                var lockIcon = net.secure ? '&#128274; ' : '';
                var item = $('<div class="cyber-sys-wifi-item">' +
                    '<span>' + lockIcon + net.ssid + '</span>' +
                    '<span style="color:#00b4d8; font-size:11px;">' + net.signal + '%</span>' +
                    '</div>');
                item.on('click', function () {
                    $('.cyber-sys-wifi-item').removeClass('selected');
                    $(this).addClass('selected');
                    self.selectedSSID = net.ssid;
                    $('#cyber-wifi-password').focus();
                });
                $list.append(item);
            });
        });
    };

    CyberSystemManagerUI.prototype.connectWifi = function (ssid, pwd) {
        var $btn = $('#cyber-wifi-connect-btn');
        $btn.prop('disabled', true).text('Connecting...');
        $.post('/system/wifi_connect', { ssid: ssid, password: pwd }, function (res) {
            $btn.prop('disabled', false).text('Connect to Wi-Fi');
            if (res.ok) {
                alert('Connected to ' + ssid + '! Your Pi is now on your home Wi-Fi.');
            } else {
                alert('Failed to connect: ' + (res.error || 'Check password and try again'));
            }
        });
    };

    CyberSystemManagerUI.prototype.loadAudioInfo = function () {
        var self = this;
        $.getJSON('/system/audio_config', function (info) {
            var $sel = $('#cyber-audio-card-select');
            $sel.empty();
            if (info.cards && info.cards.length) {
                info.cards.forEach(function (c) {
                    $sel.append('<option value="' + c.id + '">' + c.display + '</option>');
                });
            }
            if (info.current_card) $sel.val(info.current_card);
            if (info.buffer_size) {
                $('.cyber-sys-option-btn').removeClass('selected');
                $('.cyber-sys-option-btn[data-buffer="' + info.buffer_size + '"]').addClass('selected');
                self.selectedBuffer = info.buffer_size;
            }
        });
    };

    CyberSystemManagerUI.prototype.applyAudioConfig = function (card, buffer, rate) {
        var $btn = $('#cyber-audio-apply-btn');
        $btn.prop('disabled', true).text('Restarting Audio Engine...');
        $.post('/system/audio_config', { card: card, buffer: buffer, rate: rate }, function () {
            $btn.prop('disabled', false).text('Apply & Restart Audio Engine');
            alert('Audio settings applied! JACK restarted with buffer ' + buffer + ' samples.');
        });
    };

    CyberSystemManagerUI.prototype.loadSystemStats = function () {
        $.getJSON('/system/stats', function (data) {
            $('#cyber-sys-temp').text(data.cpu_temp || '---');
            $('#cyber-sys-disk').text(data.disk_free || '---');
        });
    };

    CyberSystemManagerUI.prototype.loadBankNavInfo = function () {
        $.getJSON('/system/bank_nav', function (data) {
            $('#cyber-bank-name').text(data.bank_title || 'No active bank');
            if (data.pedalboards && data.pedalboards.length) {
                var items = data.pedalboards.map(function (p, idx) {
                    var isCurrent = (p.bundle === data.current_pb);
                    var style = isCurrent ? 'color:#00ffcc; font-weight:bold;' : 'color:#bbb;';
                    return '<span style="' + style + '">' + (idx + 1) + '. ' + p.title + (isCurrent ? ' (Active)' : '') + '</span>';
                });
                $('#cyber-bank-pb-list').html(items.join(' &bull; '));
            } else {
                $('#cyber-bank-pb-list').text('No pedalboards found in this bank');
            }

            var chStr = data.channel > 0 ? (' (Ch ' + data.channel + ')') : ' (Omni)';
            if (data.next_cc >= 0) {
                $('#cyber-bank-next-val').html('<span style="color:#00ffcc; font-weight:bold;">CC ' + data.next_cc + chStr + '</span>');
            } else {
                $('#cyber-bank-next-val').text('Not mapped (Click MIDI Learn)');
            }

            if (data.prev_cc >= 0) {
                $('#cyber-bank-prev-val').html('<span style="color:#00ffcc; font-weight:bold;">CC ' + data.prev_cc + chStr + '</span>');
            } else {
                $('#cyber-bank-prev-val').text('Not mapped (Click MIDI Learn)');
            }
        });
    };

    window.CyberSystemManager = new CyberSystemManagerUI();

})(window, jQuery);
