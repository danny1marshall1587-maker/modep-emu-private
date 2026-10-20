from modtools import utils
import inspect

lines = inspect.getsourcelines(utils)[0]
for i, line in enumerate(lines):
    if 'def toPythonType' in line:
        start = max(0, i - 2)
        end = min(len(lines), i + 40)
        print("".join(lines[start:end]))
        print("="*40)
