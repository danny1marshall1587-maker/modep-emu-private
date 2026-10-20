const WebSocket = require('/home/patch/openDAW/node_modules/ws');
const ws = new WebSocket('ws://127.0.0.1:8089', 'ws-meters');

ws.on('open', () => {
    console.log('CONNECTED TO METER DAEMON');
});

ws.on('message', (data) => {
    console.log('REAL METERS:', data.toString());
    process.exit(0);
});

ws.on('error', (err) => {
    console.error('ERROR:', err);
    process.exit(1);
});

setTimeout(() => {
    console.log('Timeout waiting for meter frame');
    process.exit(1);
}, 3000);
