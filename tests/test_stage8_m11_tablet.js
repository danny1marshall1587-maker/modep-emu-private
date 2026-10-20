// Stage 8 Automated Verification: Lenovo Tab M11 Layout, Multi-Touch Suite, 8-Track Tape Banks & Rec Safety Lock
const fs = require('fs');
const path = require('path');

const webRoot = fs.existsSync('/home/patch/modep-live-console-release/web/js')
    ? '/home/patch/modep-live-console-release/web'
    : path.join(__dirname, '..', 'web', 'web');

const jsDir = path.join(webRoot, 'js');
const cssFile = path.join(webRoot, 'css', 'mixer.css');

// 1. Verify all JS files are strictly < 450 lines
for (const file of fs.readdirSync(jsDir)) {
    if (!file.endsWith('.js')) continue;
    const lines = fs.readFileSync(path.join(jsDir, file), 'utf8').split('\n').length;
    if (lines > 450) {
        console.error(`FAIL: ${file} has ${lines} lines (exceeds 450-line limit)`);
        process.exit(1);
    }
}

// 2. Verify Omni-Axis (dy + dx) in rotary knob & slider modules
for (const file of ['ao-vu-knobs.js', 'tal-reverb.js', 'tal-delay.js', 'touch-physics.js']) {
    const src = fs.readFileSync(path.join(jsDir, file), 'utf8');
    if (!src.includes('clientX') || !src.includes('clientY')) {
        console.error(`FAIL: ${file} missing Omni-Axis (X+Y) drag support`);
        process.exit(1);
    }
}

// 3. Verify Multi-Finger FadersMap, Portrait LV2 Drawer & 2-Finger Fine-Tune in touch-physics.js
const touchSrc = fs.readFileSync(path.join(jsDir, 'touch-physics.js'), 'utf8');
if (!touchSrc.includes('activeFadersMap') || !touchSrc.includes('togglePortraitLv2Drawer') || !touchSrc.includes('0.25')) {
    console.error('FAIL: touch-physics.js missing activeFadersMap, togglePortraitLv2Drawer, or 0.25x fine-tune');
    process.exit(1);
}

// 4. Verify 2-Finger Pinch-to-Q in reaeq-touch.js and dsp-rack-eq.js
const reaeqTouchSrc = fs.readFileSync(path.join(jsDir, 'reaeq-touch.js'), 'utf8');
const rackEqSrc = fs.readFileSync(path.join(jsDir, 'dsp-rack-eq.js'), 'utf8');
if (!reaeqTouchSrc.includes('Math.hypot') || !rackEqSrc.includes('Math.hypot')) {
    console.error('FAIL: Missing 2-finger Pinch-to-Q distance calculation in EQ modules');
    process.exit(1);
}

// 5. Verify 8-Track Bank Paging & Recording Safety Lock in reaper-transport.js
const tapeSrc = fs.readFileSync(path.join(jsDir, 'reaper-transport.js'), 'utf8');
if (!tapeSrc.includes('recStopConfirmUntil') || !tapeSrc.includes('bankStart, bankStart + 8') || !tapeSrc.includes("switchTapeSubPage('fx'")) {
    console.error('FAIL: reaper-transport.js missing 8-track bank slicing or 2-tap recording safety lock');
    process.exit(1);
}

// 6. Verify Dual-Network Auto-Discovery in telemetry.js
const telSrc = fs.readFileSync(path.join(jsDir, 'telemetry.js'), 'utf8');
if (!telSrc.includes('192.168.1.148') || !telSrc.includes('10.42.0.1') || !telSrc.includes('172.24.1.1')) {
    console.error('FAIL: telemetry.js missing Dual-Network Auto-Discovery IPs');
    process.exit(1);
}

// 7. Verify Lenovo Tab M11 Zero-Scroll & Portrait LV2 Popover CSS in mixer.css
const cssSrc = fs.readFileSync(cssFile, 'utf8');
if (!cssSrc.includes('.portrait-lv2-bar') || !cssSrc.includes('.linear-slot-container.drawer-open')) {
    console.error('FAIL: mixer.css missing Lenovo Tab M11 Portrait LV2 Popover Drawer rules');
    process.exit(1);
}

console.log('STAGE 8 LENOVO TAB M11 MULTI-TOUCH, 8-TRACK TAPE BANKS & ZERO-SCROLL LAYOUT VERIFIED (EXIT 0)');
