with open('/usr/share/mod/html/daw/index.html', 'r', encoding='utf-8') as f:
    text = f.read()

target = '<meta charset="UTF-8">'
replacement = '<meta charset="UTF-8">\n    <script src="/daw/coi-serviceworker.min.js"></script>'

if 'coi-serviceworker.min.js' not in text:
    text = text.replace(target, replacement, 1)
    with open('/usr/share/mod/html/daw/index.html', 'w', encoding='utf-8') as f:
        f.write(text)
    print('Inserted COI SW')
else:
    print('Already present')
