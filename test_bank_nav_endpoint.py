import time
import requests

url = "http://192.168.1.148/system/bank_nav"
for attempt in range(10):
    try:
        r = requests.get(url, timeout=3)
        print("Status Code:", r.status_code)
        print("Response:", r.json())
        break
    except Exception as e:
        print("Waiting for webserver...", e)
        time.sleep(1.5)
