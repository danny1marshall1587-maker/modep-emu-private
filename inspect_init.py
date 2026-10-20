import inspect
from mod import webserver

print("Webserver initialization lines:")
lines = inspect.getsourcelines(webserver)[0]
for i in range(65, 95):
    print(lines[i], end='')
