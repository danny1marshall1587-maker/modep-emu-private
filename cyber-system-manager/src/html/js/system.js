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
        this.buildModal();
        this.bindEvents();
    };

    CyberSystemManagerUI.prototype.buildModal = function () {
        // Ensure backdrop exists
        if (!$('#cyber-sys-backdrop').length) {
            $('body').append('<div id="cyber-sys-backdrop" class="cyber-sys-modal-backdrop" style="display:none;"></div>');
        }

        // Ensure window container exists
        if (!$('#cyber-sys-window').length) {
            $('body').append('<div id="cyber-sys-window" class="cyber-sys-window" style="display:none;"></div>');
        }

        var $win = $('#cyber-sys-window');
        if ($win.children().length > 0) {
            return; // Already populated
        }

        var html = [
            '  <div class="cyber-sys-header">',
            '    <div class="cyber-sys-title">',
            '      <span>&#9881;</span> SYSTEM & HARDWARE MANAGER',
            '    </div>',
            '    <button id="cyber-sys-close" class="cyber-sys-close-btn">&times;</button>',
            '  </div>',
            '  <div class="cyber-sys-tabs">',
            '    <button class="cyber-sys-tab-btn active" data-tab="wifi">Wi-Fi & Network</button>',
            '    <button class="cyber-sys-tab-btn" data-tab="audio">Audio & Buffer</button>',
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
            '        <label class="cyber-sys-label">Audio Interface (Sound Card)</label>',
            '        <select id="cyber-audio-card-select" class="cyber-sys-select">',
            '          <option value="hw:0">Card 0: Default / Built-in</option>',
            '        </select>',
            '      </div>',
            '      <div class="cyber-sys-group">',
            '        <label class="cyber-sys-label">Buffer Size (Latency / Performance)</label>',
            '        <div class="cyber-sys-btn-grid">',
            '          <div class="cyber-sys-option-btn" data-buffer="64">64<span class="cyber-sys-option-sub">1.3ms (Ultra)</span></div>',
            '          <div class="cyber-sys-option-btn selected" data-buffer="128">128<span class="cyber-sys-option-sub">2.7ms (Sweet Spot)</span></div>',
            '          <div class="cyber-sys-option-btn" data-buffer="256">256<span class="cyber-sys-option-sub">5.3ms (Safe Live)</span></div>',
            '          <div class="cyber-sys-option-btn" data-buffer="512">512<span class="cyber-sys-option-sub">10.6ms (Heavy Rig)</span></div>',
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
            '    <!-- TAB 3: SYSTEM & POWER -->',
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
            '  </div>'
        ].join('\n');

        $win.html(html);
    };

    CyberSystemManagerUI.prototype.bindEvents = function () {
        var self = this;

        // Top Button / Action Trigger - Namespaced Toggle
        $(document).off('click.cybersysmgr', '#mod-sys-mgr-btn, .js-sys-mgr').on('click.cybersysmgr', '#mod-sys-mgr-btn, .js-sys-mgr', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.toggle();
        });

        // Close Button & Backdrop Click
        $(document).off('click.cybersysclose', '#cyber-sys-close, #cyber-sys-backdrop').on('click.cybersysclose', '#cyber-sys-close, #cyber-sys-backdrop', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.close();
        });

        // Stop modal content clicks from bubbling to backdrop
        $(document).off('click.cybersyswin', '#cyber-sys-window').on('click.cybersyswin', '#cyber-sys-window', function (e) {
            e.stopPropagation();
        });

        // ESC Key to Close
        $(document).off('keydown.cybersysmgr').on('keydown.cybersysmgr', function (e) {
            if (e.key === 'Escape' || e.keyCode === 27) {
                if (self.isOpen) {
                    self.close();
                }
            }
        });

        // Tabs
        $(document).off('click.cybersystab', '.cyber-sys-tab-btn').on('click.cybersystab', '.cyber-sys-tab-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            var tab = $(this).attr('data-tab');
            $('.cyber-sys-tab-btn').removeClass('active');
            $(this).addClass('active');
            $('.cyber-sys-tab-pane').removeClass('active');
            $('#cyber-tab-' + tab).addClass('active');
            self.currentTab = tab;
            if (tab === 'audio') self.loadAudioInfo();
            if (tab === 'system') self.loadSystemStats();
        });

        // Buffer Buttons
        $(document).off('click.cybersysbuf', '.cyber-sys-option-btn').on('click.cybersysbuf', '.cyber-sys-option-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            $('.cyber-sys-option-btn').removeClass('selected');
            $(this).addClass('selected');
            self.selectedBuffer = parseInt($(this).attr('data-buffer'), 10) || 128;
        });

        // Wi-Fi Scan
        $(document).off('click.cybersysscan', '#cyber-wifi-scan-btn').on('click.cybersysscan', '#cyber-wifi-scan-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.scanWifi();
        });

        // Wi-Fi Connect
        $(document).off('click.cybersysconn', '#cyber-wifi-connect-btn').on('click.cybersysconn', '#cyber-wifi-connect-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            var pwd = $('#cyber-wifi-password').val();
            if (!self.selectedSSID) {
                alert('Please select a Wi-Fi network from the list first.');
                return;
            }
            self.connectWifi(self.selectedSSID, pwd);
        });

        // Switch to Hotspot
        $(document).off('click.cybersyshotspot', '#cyber-wifi-hotspot-btn').on('click.cybersyshotspot', '#cyber-wifi-hotspot-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            if (confirm('Switch Pi back to Standalone Hotspot mode (172.24.1.1)?')) {
                $.post('/system/wifi_hotspot', function () {
                    alert('Hotspot mode initiated. Reconnect your device to the "Patchbox" Wi-Fi!');
                });
            }
        });

        // Apply Audio Config
        $(document).off('click.cybersysapply', '#cyber-audio-apply-btn').on('click.cybersysapply', '#cyber-audio-apply-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            var card = $('#cyber-audio-card-select').val();
            var rate = $('#cyber-audio-rate-select').val();
            self.applyAudioConfig(card, self.selectedBuffer, rate);
        });

        // Reboot / Poweroff
        $(document).off('click.cybersysreboot', '#cyber-sys-reboot-btn').on('click.cybersysreboot', '#cyber-sys-reboot-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            if (confirm('Are you sure you want to reboot the Raspberry Pi?')) {
                $.post('/system/power', { action: 'reboot' });
            }
        });

        $(document).off('click.cybersyspower', '#cyber-sys-poweroff-btn').on('click.cybersyspower', '#cyber-sys-poweroff-btn', function (e) {
            e.preventDefault();
            e.stopPropagation();
            if (confirm('Are you sure you want to power off the Raspberry Pi safely?')) {
                $.post('/system/power', { action: 'poweroff' });
            }
        });
    };

    CyberSystemManagerUI.prototype.toggle = function () {
        if (this.isOpen) {
            this.close();
        } else {
            this.open();
        }
    };

    CyberSystemManagerUI.prototype.open = function () {
        // Mutual exclusion: Close tuner if open so backdrops never overlap
        var tuner = window.cyberTunerInstance || window.CyberTunerInstance;
        if (tuner && tuner.isOpen) {
            tuner.close();
        }

        if (!$('#cyber-sys-window').length || !$('#cyber-sys-window').children().length) {
            this.buildModal();
            this.bindEvents();
        }

        this.isOpen = true;
        $('#mod-sys-mgr-btn, .js-sys-mgr').addClass('active');
        $('#cyber-sys-backdrop').stop(true, true).fadeIn(150);
        $('#cyber-sys-window').stop(true, true).fadeIn(150);
        this.loadNetworkStatus();
        if (this.currentTab === 'audio') this.loadAudioInfo();
        if (this.currentTab === 'system') this.loadSystemStats();
    };

    CyberSystemManagerUI.prototype.close = function () {
        this.isOpen = false;
        $('#mod-sys-mgr-btn, .js-sys-mgr').removeClass('active');
        $('#cyber-sys-backdrop').stop(true, true).fadeOut(150);
        $('#cyber-sys-window').stop(true, true).fadeOut(150);
    };

    CyberSystemManagerUI.prototype.loadNetworkStatus = function () {
        $.getJSON('/system/wifi_status', function (data) {
            $('#cyber-wifi-mode').text(data.is_hotspot ? 'Standalone Hotspot (Patchbox)' : 'Home Wi-Fi (' + (data.ssid || 'Connected') + ')');
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
            if (res && res.ok) {
                alert('Connected to ' + ssid + '! Your Pi is now on your home Wi-Fi.');
            } else {
                alert('Failed to connect: ' + (res && res.error ? res.error : 'Check password and try again'));
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

    window.CyberSystemManagerUI = CyberSystemManagerUI;

    // Self-initialize on DOM ready
    $(document).ready(function () {
        if (!window.CyberSystemManager) {
            window.CyberSystemManager = new CyberSystemManagerUI();
        }
    });

})(window, jQuery);
