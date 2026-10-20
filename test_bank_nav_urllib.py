import time
import urllib.request
import json

url = "http://192.168.1.148/system/bank_nav"
for attempt in range(10):
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=3) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            print("HTTP Status:", resp.status)
            print("Bank Nav Data:", json.dumps(data, indent=2))
            break
    except Exception as e:
        print("Waiting for webserver...", e)
        time.sleep(1.5)
