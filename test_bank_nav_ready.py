import time
import urllib.request
import json

url = "http://192.168.1.148/system/bank_nav"
req = urllib.request.Request(url)
with urllib.request.urlopen(req, timeout=5) as resp:
    data = json.loads(resp.read().decode('utf-8'))
    print("Bank Nav Data:", json.dumps(data, indent=2))
