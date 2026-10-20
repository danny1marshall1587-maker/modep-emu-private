class TouchGovernor {
  constructor(maxDeltaPerMs = 0.05) {
    this.maxDelta = maxDeltaPerMs;
    this.lastTime = performance.now();
    this.currentValues = new Map();
  }

  process(id, targetVal) {
    const now = performance.now();
    const dt = Math.max(1, now - this.lastTime);
    this.lastTime = now;

    const current = this.currentValues.get(id) ?? targetVal;
    const maxAllowedStep = this.maxDelta * dt;
    const delta = targetVal - current;

    let nextVal = targetVal;
    if (Math.abs(delta) > maxAllowedStep) {
      nextVal = current + Math.sign(delta) * maxAllowedStep;
    }

    this.currentValues.set(id, nextVal);
    return nextVal;
  }
}

if (typeof window !== 'undefined') {
  window.TouchGovernor = TouchGovernor;
  window.touchGovernor = new TouchGovernor(0.05);
}
