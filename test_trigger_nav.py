import urllib.request
import urllib.parse
import json
import time

url = "http://192.168.1.148/system/bank_nav/trigger"
data = urllib.parse.urlencode({'direction': '1'}).encode('utf-8')
req = urllib.request.Request(url, data=data)
with urllib.request.urlopen(req) as resp:
    print("Trigger response:", resp.read().decode('utf-8'))

time.sleep(3)

status_url = "http://192.168.1.148/system/bank_nav"
with urllib.request.urlopen(status_url) as resp:
    print("New state:", json.dumps(json.loads(resp.read().decode('utf-8')), indent=2))
