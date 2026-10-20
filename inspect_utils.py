from modtools import utils
import ctypes

# Let's inspect how utils.py parses monitoredOutputs
import inspect
lines = inspect.getsourcelines(utils)[0]
for i, line in enumerate(lines):
    if 'monitoredOutputs' in line:
        start = max(0, i - 10)
        end = min(len(lines), i + 20)
        print("".join(lines[start:end]))
        print("="*40)
