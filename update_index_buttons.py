with open("/usr/share/mod/html/index.html", "r") as f:
    text = f.read()

text = text.replace("window.open('/mixer', '_blank')", "window.open('/mixer/index.html', '_blank')")
text = text.replace("window.open('/daw', '_blank')", "window.open('/daw/index.html', '_blank')")

with open("/usr/share/mod/html/index.html", "w") as f:
    f.write(text)

print("UPDATED_BUTTONS_OK")
