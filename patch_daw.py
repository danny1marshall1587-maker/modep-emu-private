with open('/usr/share/mod/html/daw/index.html', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('src="/main.', 'src="/daw/main.')
c = c.replace('href="/main.', 'href="/daw/main.')
c = c.replace('href="/favicon.', 'href="/daw/favicon.')

with open('/tmp/daw_fixed.html', 'w', encoding='utf-8') as f:
    f.write(c)
print("Successfully generated /tmp/daw_fixed.html")
