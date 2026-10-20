// SPDX-FileCopyrightText: 2026 Cyber Audio
// SPDX-License-Identifier: MIT
// Cyber TONE3000 API-Powered Studio & Proxy Controller for MOD Desktop & MODEP
// Integrated with TONE3000 API (C:\Users\danny\Documents\modep emu\api-main)

(function (window, $) {
    'use strict';

    var BUILTIN_TONES = [
        { id: 0, title: "Martin D-45 Studio Mic (Acoustic)", author: "CyberAudio", gear: "acoustic", desc: "Warm studio condenser mic response converting raw piezo pickup into full woody Dreadnought body", defaultIr: 0, irName: "Neumann U87 Condenser", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 1, title: "Taylor 814ce Grand Auditorium", author: "CyberAudio", gear: "acoustic", desc: "Pristine modern acoustic capture with airy top-end sparkle and balanced fingerpicking clarity", defaultIr: 1, irName: "Royer R-121 Ribbon", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 2, title: "Gibson J-45 Vintage Round Shoulder", author: "CyberAudio", gear: "acoustic", desc: "Classic vintage woody thud with punchy low-mids for strumming and folk rhythm", defaultIr: 2, irName: "Shure SM7B Dynamic", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 3, title: "Guild F-512 12-String Acoustic", author: "CyberAudio", gear: "acoustic", desc: "Shimmering, lush, choir-like acoustic resonance with wide stereo spread feel", defaultIr: 3, irName: "Stereo Room Pair", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 4, title: "Fender '65 Deluxe Reverb Blackface", author: "FenderTone", gear: "amp", desc: "Sparkling American clean with scooped mids, bell-like highs, and smooth edge-of-breakup", defaultIr: 4, irName: "Jensen C12N 1x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 5, title: "Fender '59 Tweed Bassman 4x10", author: "ToneLover", gear: "amp", desc: "Touch-sensitive vintage Tweed breakup with punchy low-end and singing harmonics", defaultIr: 5, irName: "Jensen P10R 4x10", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 6, title: "Dumble Overdrive Special (Clean)", author: "BoutiqueAmps", gear: "amp", desc: "Legendary thick, blooming boutique clean with infinite headroom and touch response", defaultIr: 6, irName: "EVM12L 2x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 7, title: "Matchless DC-30 Chime Channel", author: "BritishTone", gear: "amp", desc: "Iconic Class-A EL84 chime with 3D high-end clarity and harmonic richness", defaultIr: 7, irName: "Alnico Blue 2x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 8, title: "Vox AC30 Top Boost 1964", author: "JMI_Vintage", gear: "amp", desc: "The quintessential British Invasion top-boost bite, chime sparkle, and aggressive crunch", defaultIr: 7, irName: "Alnico Blue 2x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 9, title: "Marshall Bluesbreaker 1962 (JTM45)", author: "PlexiFan", gear: "amp", desc: "Warm organic tube sag with fat woody cleans pushing into singing creamy blues sustain", defaultIr: 8, irName: "Celestion V30 4x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 10, title: "Orange Rockerverb 50 MkIII", author: "DoomRigs", gear: "amp", desc: "Mid-forward British roar with thick velvety low-end and rich harmonic fuzz-edge", defaultIr: 9, irName: "Orange V30 4x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 11, title: "Marshall JCM800 2203 Classic 80s", author: "HardRock80", gear: "amp", desc: "Tight, biting 80s rock rhythm with aggressive cut and punchy low end", defaultIr: 8, irName: "Celestion V30 4x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 12, title: "Soldano SLO-100 Super Lead", author: "LeadKing", gear: "amp", desc: "Legendary singing boutique high-gain lead channel with infinite sustain and clarity", defaultIr: 10, irName: "Eminence Legend 4x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 13, title: "Mesa/Boogie Dual Rectifier Multi-Watt", author: "ModernMetal", gear: "amp", desc: "Massive American wall-of-sound rhythm tone with scooped mids and huge palm-mute thump", defaultIr: 11, irName: "Oversized V30 4x12", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 14, title: "Ampeg SVT Classic 8x10 Tube Stack", author: "BassBeast", gear: "bass", desc: "Monumental 300W tube bass tone with earth-shaking low-end authority and growl", defaultIr: 12, irName: "Ampeg 8x10 Sealed", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true },
        { id: 15, title: "Darkglass Microtubes B7K Ultra", author: "BassPrecision", gear: "bass", desc: "Aggressive modern bass preamp with clinical attack clank and biting low-end definition", defaultIr: 13, irName: "Neodymium 4x10", format: "nam", img: "img/t3k-mark.svg", isBuiltin: true }
    ];

    function CyberTone3000(options) {
        var self = this;
        this.options = options || {};
        this.windowModal = $('#mod-tone3000-window');
        this.isOpen = false;
        this.activeInstance = null;
        this.currentToneId = 0;
        this.activeTone = BUILTIN_TONES[0];
        this.mainTab = 'browser'; // 'browser' or 'chain'
        this.browserView = 'online'; // 'online' or 'downloaded'
        this.onlineTones = [];
        this.downloadedTones = [];
        this.audioCtx = null;
        this.isLoadingOnline = false;

        // Faceplate parameters
        this.params = {
            input_gain: 0,
            gate: -80,
            gate_power: 1,
            bass: 0,
            mid: 0,
            treble: 0,
            eq_power: 1,
            output_level: 0
        };

        this.init();
    }

    CyberTone3000.prototype.init = function () {
        var self = this;

        // Load saved collection from localStorage
        this.loadSavedCollection();

        // Delegated listener on document for top bar button
        $(document).off('click.t3k_top').on('click.t3k_top', '#mod-tone3000-top-btn, .js-tone3000', function (e) {
            e.preventDefault();
            e.stopPropagation();

            var target = self.findTargetPedal();
            if (!target.instance) {
                self.showToast('Please add TONE 3000 from the pedal library onto your pedalboard to link');
                return;
            }

            self.toggle();
        });

        // Global Esc key listener
        $(document).off('keydown.t3k_esc').on('keydown.t3k_esc', function (e) {
            if (e.keyCode === 27 && self.isOpen) {
                self.close();
            }
        });

        if (!$('#t3k-toast').length) {
            $('body').append('<div id="t3k-toast" class="t3k-app-toast"></div>');
        }

        // Fetch online tones from TONE3000 API
        this.fetchOnlineTones('amp-cab');

        // Dynamic linking watcher
        this.startDynamicLinkingWatcher();
    };

    CyberTone3000.prototype.loadSavedCollection = function () {
        try {
            var raw = localStorage.getItem('t3k_downloaded_collection');
            if (raw) {
                this.downloadedTones = JSON.parse(raw);
            }
        } catch (e) {}
    };

    CyberTone3000.prototype.saveCollection = function () {
        try {
            localStorage.setItem('t3k_downloaded_collection', JSON.stringify(this.downloadedTones));
        } catch (e) {}
    };

    CyberTone3000.prototype.fetchOnlineTones = function (gear) {
        var self = this;
        self.isLoadingOnline = true;
        var qs = (gear && gear !== 'all') ? ('?gear=' + gear) : '';

        $.ajax({
            url: 'https://www.tone3000.com/api/v1/tones/trending' + qs,
            type: 'GET',
            dataType: 'json',
            timeout: 6000,
            success: function (res) {
                self.isLoadingOnline = false;
                if (res && res.data && Array.isArray(res.data)) {
                    self.onlineTones = res.data.map(function (t, idx) {
                        return {
                            id: 'cloud_' + t.id,
                            remoteId: t.id,
                            title: t.title || ("Tone #" + t.id),
                            author: (t.user && t.user.username) ? t.user.username : "Community",
                            gear: t.gear || "amp-cab",
                            desc: t.description || "Captured for NAM on TONE3000.com",
                            defaultIr: 8,
                            irName: "Studio IR",
                            format: t.format || "nam",
                            img: (t.images && t.images[0]) ? t.images[0] : "img/t3k-mark.svg",
                            url: t.url || ("https://www.tone3000.com/tones/" + t.id),
                            downloads: t.downloads_count || 0,
                            favorites: t.favorites_count || 0
                        };
                    });

                    // Re-render if online tab is currently showing
                    if (self.isOpen && self.mainTab === 'browser' && self.browserView === 'online') {
                        var grid = self.windowModal.find('#t3k-cards-grid');
                        if (grid.length) {
                            self.renderCards('online', grid);
                        }
                    }
                }
            },
            error: function () {
                self.isLoadingOnline = false;
            }
        });
    };

    CyberTone3000.prototype.findTargetPedal = function () {
        var targetInstance = null;
        var el = null;

        $('.mod-pedal').each(function () {
            var p = $(this);
            var uri = p.data('uri') || p.attr('mod-uri') || '';
            var instance = p.data('instance') || p.attr('mod-instance');
            if (uri.indexOf('cyber-tone3000') !== -1) {
                targetInstance = instance;
                el = p;
                return false;
            } else if (!targetInstance && (uri.indexOf('nam-loader') !== -1 || uri.indexOf('neural-amp') !== -1)) {
                targetInstance = instance;
                el = p;
            }
        });

        return { instance: targetInstance, el: el };
    };

    CyberTone3000.prototype.startDynamicLinkingWatcher = function () {
        var self = this;

        function updateTopButton() {
            var target = self.findTargetPedal();
            var btn = $('#mod-tone3000-top-btn, .js-tone3000');
            if (!btn.length) return;

            if (target.instance) {
                btn.removeClass('is-unlinked').addClass('is-linked');
                btn.attr('title', 'TONE3000 Studio — Linked to ' + target.instance);
                btn.html('TONE3000 <span style="color:#4ade80;font-size:9px;">&#9679;</span>');
                self.activeInstance = target.instance;
            } else {
                btn.removeClass('is-linked').addClass('is-unlinked');
                btn.attr('title', 'TONE3000 (Drop TONE 3000 pedal onto pedalboard to link)');
                btn.html('TONE3000 <span style="color:#64748b;font-size:9px;">&#9675;</span>');
                self.activeInstance = null;
            }
        }

        setInterval(updateTopButton, 600);
    };

    CyberTone3000.prototype.toggle = function () {
        if (this.isOpen) {
            this.close();
        } else {
            this.open();
        }
    };

    CyberTone3000.prototype.open = function () {
        var self = this;
        this.isOpen = true;
        $('#mod-tone3000-top-btn, .js-tone3000').addClass('active');

        var target = this.findTargetPedal();
        this.activeInstance = target.instance;

        // Read initial parameter values from the pedal on canvas
        if (target.el && target.el.length) {
            ['input_gain', 'gate', 'bass', 'mid', 'treble', 'output_level'].forEach(function (s) {
                var k = target.el.find('.mod-knob-image[mod-port-symbol="' + s + '"]');
                if (k.length && k.val() !== undefined && k.val() !== '') {
                    self.params[s] = parseFloat(k.val());
                }
            });
        }

        var linkHtml = target.instance ?
            ('<span class="t3k-link-status-chip">&#9679; LINKED: ' + target.instance + '</span>') :
            ('<span class="t3k-link-status-chip unlinked">&#9888; UNLINKED (ADD TONE3000 TO BOARD)</span>');

        var modalHtml = [
            '<div class="t3k-main-app-shell">',
            '  <!-- Header (PluginHeader.tsx) -->',
            '  <div class="t3k-app-header">',
            '    <div class="t3k-header-brand-group">',
            '      <a href="https://www.tone3000.com" target="_blank" rel="noopener noreferrer">',
            '        <img src="img/t3k.svg" class="t3k-brand-img" alt="TONE3000">',
            '      </a>',
            '      <div id="t3k-header-link-badge">' + linkHtml + '</div>',
            '    </div>',
            '    <div class="t3k-header-nav-tabs">',
            '      <button type="button" class="t3k-nav-pill-btn' + (this.mainTab === 'browser' ? ' active' : '') + '" data-tab="browser">&#9729; Tone Browser</button>',
            '      <button type="button" class="t3k-nav-pill-btn' + (this.mainTab === 'chain' ? ' active' : '') + '" data-tab="chain">&#9776; Signal Chain</button>',
            '    </div>',
            '    <div class="t3k-header-right-group">',
            '      <button type="button" class="t3k-header-close-btn" id="t3k-close-btn" title="Close">&times;</button>',
            '    </div>',
            '  </div>',
            '',
            '  <!-- Center Workspace Body -->',
            '  <div class="t3k-workspace-body" id="t3k-workspace">',
            '    <!-- Dynamic Content Injected Here -->',
            '  </div>',
            '',
            '  <!-- Bottom Faceplate (Faceplate.tsx) -->',
            '  <div class="t3k-app-faceplate">',
            '    <!-- Input Level -->',
            '    <div class="t3k-plate-section">',
            '      <div class="t3k-knob-unit" data-sym="input_gain" data-min="-24" data-max="24" data-def="0">',
            '        <img src="img/primary-knob.svg" class="t3k-knob-dial-svg primary" alt="Gain">',
            '        <span class="t3k-knob-label-mono">INPUT</span>',
            '        <span class="t3k-knob-val-mono" id="t3k-disp-input_gain">' + this.params.input_gain.toFixed(1) + ' dB</span>',
            '      </div>',
            '    </div>',
            '    <div class="t3k-plate-section-divider"></div>',
            '    <!-- Noise Gate -->',
            '    <div class="t3k-plate-section">',
            '      <button type="button" class="t3k-power-toggle-btn' + (this.params.gate_power ? '' : ' off') + '" id="t3k-power-gate" title="Toggle Gate">&#x23FB;</button>',
            '      <div class="t3k-knob-unit" data-sym="gate" data-min="-80" data-max="-20" data-def="-80">',
            '        <img src="img/secondary-knob.svg" class="t3k-knob-dial-svg secondary" alt="Gate">',
            '        <span class="t3k-knob-label-mono">GATE</span>',
            '        <span class="t3k-knob-val-mono" id="t3k-disp-gate">' + this.params.gate.toFixed(1) + ' dB</span>',
            '      </div>',
            '    </div>',
            '    <div class="t3k-plate-section-divider"></div>',
            '    <!-- 3-Band Tone Stack -->',
            '    <div class="t3k-plate-section">',
            '      <button type="button" class="t3k-power-toggle-btn' + (this.params.eq_power ? '' : ' off') + '" id="t3k-power-eq" title="Toggle Tone Stack">&#x23FB;</button>',
            '      <div class="t3k-knob-unit" data-sym="bass" data-min="-12" data-max="12" data-def="0">',
            '        <img src="img/secondary-knob.svg" class="t3k-knob-dial-svg secondary" alt="Bass">',
            '        <span class="t3k-knob-label-mono">BASS</span>',
            '        <span class="t3k-knob-val-mono" id="t3k-disp-bass">' + this.params.bass.toFixed(1) + ' dB</span>',
            '      </div>',
            '      <div class="t3k-knob-unit" data-sym="mid" data-min="-12" data-max="12" data-def="0">',
            '        <img src="img/secondary-knob.svg" class="t3k-knob-dial-svg secondary" alt="Mid">',
            '        <span class="t3k-knob-label-mono">MID</span>',
            '        <span class="t3k-knob-val-mono" id="t3k-disp-mid">' + this.params.mid.toFixed(1) + ' dB</span>',
            '      </div>',
            '      <div class="t3k-knob-unit" data-sym="treble" data-min="-12" data-max="12" data-def="0">',
            '        <img src="img/secondary-knob.svg" class="t3k-knob-dial-svg secondary" alt="Treble">',
            '        <span class="t3k-knob-label-mono">TREBLE</span>',
            '        <span class="t3k-knob-val-mono" id="t3k-disp-treble">' + this.params.treble.toFixed(1) + ' dB</span>',
            '      </div>',
            '    </div>',
            '    <div class="t3k-plate-section-divider"></div>',
            '    <!-- Output Level -->',
            '    <div class="t3k-plate-section">',
            '      <div class="t3k-knob-unit" data-sym="output_level" data-min="-24" data-max="24" data-def="0">',
            '        <img src="img/primary-knob.svg" class="t3k-knob-dial-svg primary" alt="Output">',
            '        <span class="t3k-knob-label-mono">OUTPUT</span>',
            '        <span class="t3k-knob-val-mono" id="t3k-disp-output_level">' + this.params.output_level.toFixed(1) + ' dB</span>',
            '      </div>',
            '    </div>',
            '  </div>',
            '</div>'
        ].join('\n');

        this.windowModal.html(modalHtml);

        // Header Tab Handlers
        this.windowModal.find('.t3k-nav-pill-btn').on('click', function () {
            self.windowModal.find('.t3k-nav-pill-btn').removeClass('active');
            $(this).addClass('active');
            self.mainTab = $(this).data('tab');
            self.renderMainContent();
        });

        // Close button handler
        this.windowModal.find('#t3k-close-btn').on('click', function () {
            self.close();
        });

        this.windowModal.off('click.t3k_bg').on('click.t3k_bg', function (e) {
            if ($(e.target).is('#mod-tone3000-window')) {
                self.close();
            }
        });

        // Render main content
        this.renderMainContent();

        // Initialize Faceplate Knobs
        this.initFaceplateKnobs();

        // Show window
        this.windowModal.css('display', 'flex').hide().fadeIn(150);
    };

    CyberTone3000.prototype.renderMainContent = function () {
        var ws = this.windowModal.find('#t3k-workspace');
        ws.empty();

        if (this.mainTab === 'chain') {
            this.renderChainTab(ws);
        } else {
            this.renderBrowserTab(ws);
        }
    };

    CyberTone3000.prototype.renderChainTab = function (container) {
        var m = this.activeTone || BUILTIN_TONES[0];
        var html = [
            '<div class="t3k-chain-view-panel">',
            '  <div class="t3k-chain-heading-row">',
            '    <span class="t3k-section-title">Signal Chain Architecture &bull; 4 Stages</span>',
            '    <span class="t3k-section-title">Live Host Routing</span>',
            '  </div>',
            '  <div class="t3k-chain-lane-tiles">',
            '    <div class="t3k-block-tile">',
            '      <div class="t3k-block-top"><span class="t3k-block-badge">STAGE 1</span><div class="t3k-block-power-dot"></div></div>',
            '      <div class="t3k-block-title">Noise Gate</div>',
            '      <div class="t3k-block-sub">Threshold: ' + this.params.gate.toFixed(0) + ' dB</div>',
            '    </div>',
            '    <span class="t3k-rail-arrow">&rarr;</span>',
            '    <div class="t3k-block-tile active-block">',
            '      <div class="t3k-block-top"><span class="t3k-block-badge">STAGE 2 (NAM 2.0)</span><div class="t3k-block-power-dot"></div></div>',
            '      <div class="t3k-block-title" id="t3k-chain-title">' + m.title + '</div>',
            '      <div class="t3k-block-sub" id="t3k-chain-author">by ' + m.author + '</div>',
            '    </div>',
            '    <span class="t3k-rail-arrow">&rarr;</span>',
            '    <div class="t3k-block-tile active-block">',
            '      <div class="t3k-block-top"><span class="t3k-block-badge">STAGE 3 (CAB/IR)</span><div class="t3k-block-power-dot"></div></div>',
            '      <div class="t3k-block-title" id="t3k-chain-ir">' + (m.irName || 'Studio IR') + '</div>',
            '      <div class="t3k-block-sub">Studio Impulse Response</div>',
            '    </div>',
            '    <span class="t3k-rail-arrow">&rarr;</span>',
            '    <div class="t3k-block-tile">',
            '      <div class="t3k-block-top"><span class="t3k-block-badge">STAGE 4</span><div class="t3k-block-power-dot"></div></div>',
            '      <div class="t3k-block-title">3-Band Tone Stack</div>',
            '      <div class="t3k-block-sub">Bass / Mid / Treble</div>',
            '    </div>',
            '  </div>',
            '</div>'
        ].join('\n');
        container.html(html);
    };

    CyberTone3000.prototype.renderBrowserTab = function (container) {
        var self = this;
        var totalDownloaded = BUILTIN_TONES.length + this.downloadedTones.length;

        var html = [
            '<div class="t3k-browser-panel">',
            '  <!-- Sub-Tabs: Online Cloud vs Downloaded Tones -->',
            '  <div class="t3k-browser-sub-header">',
            '    <div class="t3k-browser-sub-tabs">',
            '      <button type="button" class="t3k-sub-tab-btn' + (this.browserView === 'online' ? ' active online-trigger' : '') + '" data-view="online">&#127760; Online TONE3000 Cloud (Search &amp; Audition)</button>',
            '      <button type="button" class="t3k-sub-tab-btn' + (this.browserView === 'downloaded' ? ' active' : '') + '" data-view="downloaded">&#128190; My Downloaded Tones (' + totalDownloaded + ')</button>',
            '    </div>',
            '    <div>',
            '      <a href="https://www.tone3000.com" target="_blank" style="color:#00d2ff;text-decoration:none;font-size:12px;font-weight:bold;">Visit tone3000.com &#8599;</a>',
            '    </div>',
            '  </div>',
            '  <!-- Filter Controls -->',
            '  <div style="padding:14px 24px 0 24px;">',
            '    <div class="t3k-browser-controls-row">',
            '      <input type="text" class="t3k-browser-search-bar" id="t3k-b-search" placeholder="Search captures (Martin D-45, Taylor, Deluxe, Tweed, AC30, Soldano, Satan, SD-1)...">',
            '      <div class="t3k-gear-filter-pills" id="t3k-gear-pills">',
            '        <button type="button" class="t3k-g-pill active" data-gear="all">All Tones</button>',
            '        <button type="button" class="t3k-g-pill" data-gear="amp-cab">Full Rigs (Amp + Cab)</button>',
            '        <button type="button" class="t3k-g-pill" data-gear="amp">Amp Heads</button>',
            '        <button type="button" class="t3k-g-pill" data-gear="pedal">Pedals &amp; ODs</button>',
            '        <button type="button" class="t3k-g-pill" data-gear="acoustic">Acoustic Guitars &amp; Mics</button>',
            '        <button type="button" class="t3k-g-pill" data-gear="bass">Bass Rigs</button>',
            '      </div>',
            '    </div>',
            '  </div>',
            '  <!-- Cards Grid Area -->',
            '  <div class="t3k-browser-content-area">',
            '    <div class="t3k-tone-cards-grid" id="t3k-cards-grid"></div>',
            '  </div>',
            '</div>'
        ].join('\n');
        container.html(html);

        // Sub-Tab Click Handlers
        container.find('.t3k-sub-tab-btn').on('click', function () {
            container.find('.t3k-sub-tab-btn').removeClass('active online-trigger');
            $(this).addClass('active');
            if ($(this).data('view') === 'online') $(this).addClass('online-trigger');
            self.browserView = $(this).data('view');
            self.renderCards(self.browserView, container.find('#t3k-cards-grid'));
        });

        // Filter Pills Click Handlers
        container.find('.t3k-g-pill').on('click', function () {
            container.find('.t3k-g-pill').removeClass('active');
            $(this).addClass('active');
            var g = $(this).data('gear');
            if (self.browserView === 'online' && g !== 'all' && g !== 'acoustic' && g !== 'bass') {
                self.fetchOnlineTones(g);
            }
            self.renderCards(self.browserView, container.find('#t3k-cards-grid'));
        });

        // Search bar handler
        container.find('#t3k-b-search').on('input', function () {
            self.renderCards(self.browserView, container.find('#t3k-cards-grid'));
        });

        this.renderCards(this.browserView, container.find('#t3k-cards-grid'));
    };

    CyberTone3000.prototype.renderCards = function (view, grid) {
        var self = this;
        grid.empty();

        var container = this.windowModal.find('#t3k-workspace');
        var gear = container.find('.t3k-g-pill.active').data('gear') || 'all';
        var q = container.find('#t3k-b-search').val() || '';

        var list = (view === 'online') ? this.onlineTones : [].concat(BUILTIN_TONES, this.downloadedTones);

        var filtered = list.filter(function (t) {
            var gMatch = (gear === 'all' || t.gear === gear || (gear === 'amp' && t.gear === 'amp-cab'));
            var qMatch = true;
            if (q && q.trim().length > 0) {
                var s = q.toLowerCase().trim();
                qMatch = (t.title.toLowerCase().indexOf(s) !== -1 ||
                          t.desc.toLowerCase().indexOf(s) !== -1 ||
                          t.author.toLowerCase().indexOf(s) !== -1);
            }
            return gMatch && qMatch;
        });

        if (view === 'online' && this.isLoadingOnline) {
            grid.html('<div style="grid-column:1/-1;text-align:center;padding:40px;color:#00d2ff;font-family:monospace;">Fetching live tones from TONE3000 Cloud API...</div>');
            return;
        }

        if (!filtered.length) {
            grid.html('<div style="grid-column:1/-1;text-align:center;padding:40px;color:#64748b;">No tones found matching your filter.</div>');
            return;
        }

        filtered.forEach(function (tone) {
            var isCur = (self.activeTone && self.activeTone.title === tone.title);
            var isDownloaded = self.isToneDownloaded(tone);

            var actionBtnHtml = '';
            if (view === 'online') {
                actionBtnHtml = [
                    '<button type="button" class="t3k-audition-btn" title="Audition preview">&#9658; Audition</button>',
                    '<button type="button" class="t3k-load-pedal-btn t3k-action-download" title="Save to My Downloaded Tones Collection">' + (isDownloaded ? 'Saved &#10003;' : '&#10515; Save') + '</button>',
                    '<button type="button" class="t3k-load-pedal-btn t3k-action-load' + (isCur ? ' loaded' : '') + '">' + (isCur ? 'Active &#10003;' : 'Play on Pedal &rarr;') + '</button>'
                ].join('');
            } else {
                actionBtnHtml = [
                    '<button type="button" class="t3k-audition-btn" title="Audition preview">&#9658; Audition</button>',
                    '<button type="button" class="t3k-load-pedal-btn t3k-action-load' + (isCur ? ' loaded' : '') + '">' + (isCur ? 'Active &#10003;' : 'Load to Pedal &rarr;') + '</button>'
                ].join('');
            }

            var card = $([
                '<div class="t3k-tone-card' + (isCur ? ' active-selected' : '') + '" data-id="' + tone.id + '">',
                '  <div class="t3k-card-media-box">',
                '    <img src="' + tone.img + '" alt="Tone Icon" onerror="this.src=\'img/t3k-mark.svg\'">',
                '  </div>',
                '  <div class="t3k-card-info-col">',
                '    <span class="t3k-card-title-text">' + tone.title + '</span>',
                '    <span class="t3k-card-tag-badge">' + tone.format.toUpperCase() + ' &bull; ' + tone.gear.toUpperCase() + '</span>',
                '    <span class="t3k-card-author-line">by @' + tone.author + '</span>',
                '    <div class="t3k-card-actions-row">' + actionBtnHtml + '</div>',
                '  </div>',
                '</div>'
            ].join(''));

            // Audition handler
            card.find('.t3k-audition-btn').on('click', function (e) {
                e.stopPropagation();
                self.playAudition(tone.id);
            });

            // Download handler
            card.find('.t3k-action-download').on('click', function (e) {
                e.stopPropagation();
                self.addToDownloaded(tone);
                $(this).text('Saved ✓');
            });

            // Load to pedal handler
            card.find('.t3k-action-load').on('click', function (e) {
                e.stopPropagation();
                self.loadTone(tone);
            });

            grid.append(card);
        });
    };

    CyberTone3000.prototype.isToneDownloaded = function (tone) {
        if (tone.isBuiltin) return true;
        return this.downloadedTones.some(function (t) {
            return t.title === tone.title || (t.remoteId && t.remoteId === tone.remoteId);
        });
    };

    CyberTone3000.prototype.addToDownloaded = function (tone) {
        if (this.isToneDownloaded(tone)) return;
        this.downloadedTones.push(tone);
        this.saveCollection();
        this.showToast('✓ Saved ' + tone.title + ' to My Downloaded Tones!');
    };

    CyberTone3000.prototype.initFaceplateKnobs = function () {
        var self = this;
        var knobs = this.windowModal.find('.t3k-knob-unit');

        knobs.each(function () {
            var unit = $(this);
            var sym = unit.data('sym');
            var min = parseFloat(unit.data('min'));
            var max = parseFloat(unit.data('max'));
            var def = parseFloat(unit.data('def'));
            var curVal = self.params[sym] !== undefined ? self.params[sym] : def;

            function updateAngle(val) {
                var norm = (val - min) / (max - min);
                var deg = -135 + norm * 270;
                unit.find('.t3k-knob-dial-svg').css('transform', 'rotate(' + deg + 'deg)');
                self.windowModal.find('#t3k-disp-' + sym).text(val.toFixed(1) + ' dB');
            }

            updateAngle(curVal);

            var isDragging = false;
            var startY = 0;
            var startVal = curVal;

            unit.on('mousedown touchstart pointerdown', function (e) {
                e.preventDefault();
                e.stopPropagation();
                isDragging = true;
                startY = e.pageY || (e.originalEvent.touches && e.originalEvent.touches[0].pageY);
                startVal = self.params[sym];

                $(document).on('mousemove.t3k_p touchmove.t3k_p pointermove.t3k_p', function (ev) {
                    if (!isDragging) return;
                    var curY = ev.pageY || (ev.originalEvent.touches && ev.originalEvent.touches[0].pageY);
                    var dy = startY - curY;
                    var range = max - min;
                    var newVal = Math.max(min, Math.min(max, startVal + (dy / 140) * range));

                    self.params[sym] = newVal;
                    updateAngle(newVal);

                    // Send parameter change to pedal on board
                    self.sendParamToBoard(sym, newVal);
                });

                $(document).on('mouseup.t3k_p touchend.t3k_p pointerup.t3k_p', function () {
                    isDragging = false;
                    $(document).off('mousemove.t3k_p touchmove.t3k_p pointermove.t3k_p');
                    $(document).off('mouseup.t3k_p touchend.t3k_p pointerup.t3k_p');
                });
            });
        });

        // Power toggles
        this.windowModal.find('#t3k-power-gate').on('click', function () {
            $(this).toggleClass('off');
            var isOn = !$(this).hasClass('off');
            self.params.gate_power = isOn ? 1 : 0;
        });

        this.windowModal.find('#t3k-power-eq').on('click', function () {
            $(this).toggleClass('off');
            var isOn = !$(this).hasClass('off');
            self.params.eq_power = isOn ? 1 : 0;
        });
    };

    CyberTone3000.prototype.sendParamToBoard = function (sym, val) {
        var target = this.findTargetPedal();
        if (!target.instance) return;

        var instance = target.instance; // e.g. "/graph/cyber_tone3000"
        var port = instance + '/' + sym;

        // 1. Update native knob on pedal canvas in DOM (triggers MOD-UI built-in change listener)
        if (target.el && target.el.length) {
            var knob = target.el.find('.mod-knob-image[mod-port-symbol="' + sym + '"]');
            if (knob.length) {
                knob.val(val).trigger('change');
            }
        }

        // 2. Call pedalboard pluginParameterChange directly (sends over WebSocket)
        try {
            var pb = $('#pedalboard');
            if (pb.length && typeof pb.data('pluginParameterChange') === 'function') {
                pb.data('pluginParameterChange')(port, val);
            }
        } catch (e) {}

        // 3. Call desktop.ParameterSet for HMI
        try {
            if (window.desktop && typeof window.desktop.ParameterSet === 'function') {
                window.desktop.ParameterSet(port + '/' + val);
            }
        } catch (e) {}
    };

    CyberTone3000.prototype.loadTone = function (tone) {
        this.activeTone = tone;
        var target = this.findTargetPedal();
        if (!target.instance) {
            this.showToast('Please add TONE 3000 onto your pedalboard to link');
            return;
        }

        // Map model ID: if built-in (0..15), use its index; otherwise assign safe model index
        var mId = (typeof tone.id === 'number' && tone.id < 16) ? tone.id : 0;
        var irId = tone.defaultIr !== undefined ? tone.defaultIr : 0;

        // Send model and IR parameters to board
        this.sendParamToBoard('model', mId);
        this.sendParamToBoard('ir', irId);

        // Update pedal canvas OLED display
        if (target.el && target.el.length) {
            target.el.find('#t3k-screen-title').text(tone.title);
            target.el.find('#t3k-screen-sub').text('Cab: ' + (tone.irName || 'Studio IR'));
            target.el.find('#t3k-screen-cat').text((tone.gear || 'NAM').toUpperCase());

            var modelKnob = target.el.find('#t3k-port-model, .mod-knob-image[mod-port-symbol="model"]');
            if (modelKnob.length) modelKnob.val(mId).trigger('change');

            var irKnob = target.el.find('#t3k-port-ir, .mod-knob-image[mod-port-symbol="ir"]');
            if (irKnob.length) irKnob.val(irId).trigger('change');
        }

        // Update cards highlight in browser
        this.windowModal.find('.t3k-tone-card').removeClass('active-selected');
        this.windowModal.find('.t3k-tone-card[data-id="' + tone.id + '"]').addClass('active-selected');
        this.windowModal.find('.t3k-action-load').removeClass('loaded').text('Play on Pedal →');
        this.windowModal.find('.t3k-tone-card[data-id="' + tone.id + '"] .t3k-action-load').addClass('loaded').text('Active ✓');

        // Automatically add to downloaded library if from cloud
        this.addToDownloaded(tone);

        this.showToast('✓ Loaded ' + tone.title + ' into TONE 3000!');
    };

    CyberTone3000.prototype.playAudition = function (id) {
        try {
            if (!this.audioCtx) this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            var ctx = this.audioCtx;
            var now = ctx.currentTime;
            var freqs = [196.00, 246.94, 293.66, 392.00, 493.88];

            freqs.forEach(function (f, i) {
                var osc = ctx.createOscillator();
                var gain = ctx.createGain();
                osc.type = (typeof id === 'number' && id < 4) ? 'triangle' : 'sawtooth';
                osc.frequency.setValueAtTime(f, now + i * 0.08);

                gain.gain.setValueAtTime(0.0001, now + i * 0.08);
                gain.gain.exponentialRampToValueAtTime(0.12, now + i * 0.08 + 0.02);
                gain.gain.exponentialRampToValueAtTime(0.0001, now + i * 0.08 + 0.9);

                osc.connect(gain);
                gain.connect(ctx.destination);

                osc.start(now + i * 0.08);
                osc.stop(now + i * 0.08 + 1.0);
            });
        } catch (err) {}
    };

    CyberTone3000.prototype.close = function () {
        this.isOpen = false;
        $('#mod-tone3000-top-btn, .js-tone3000').removeClass('active');
        this.windowModal.fadeOut(150);
    };

    CyberTone3000.prototype.showToast = function (msg) {
        var toast = $('#t3k-toast');
        toast.text(msg).stop(true, true).fadeIn(150).delay(2500).fadeOut(250);
    };

    window.CyberTone3000 = CyberTone3000;

})(window, jQuery);
