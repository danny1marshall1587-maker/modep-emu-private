import urllib.request

html_8080 = urllib.request.urlopen('http://127.0.0.1:8080/').read().decode('utf-8')
print("Total length of 8080 index.html:", len(html_8080))
print("Contains PATCHSTORAGE_ENABLED:", "PATCHSTORAGE_ENABLED" in html_8080)
print("Contains #patchstorage-library:", "patchstorage-library" in html_8080)
print("Contains #patchstorage:", "id=\"patchstorage\"" in html_8080)
print("Contains #mod-cloud-plugins:", "id=\"mod-cloud-plugins\"" in html_8080)
