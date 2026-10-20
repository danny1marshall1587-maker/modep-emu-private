import re

plugins = ['cyber-organ9', 'cyber-mel9', 'cyber-synth9', 'cyber-string9', 'cyber-bass9']
for p in plugins:
    ttl_path = f'plugins/{p}.lv2/{p.replace("-", "_")}.ttl'
    modgui_path = f'plugins/{p}.lv2/modgui.ttl'
    html_path = f'plugins/{p}.lv2/modgui/icon-{p}.html'

    with open(ttl_path, 'r', encoding='utf-8') as f:
        ttl_text = f.read()
    ttl_symbols = re.findall(r'lv2:symbol\s+"([^"]+)"', ttl_text)

    with open(modgui_path, 'r', encoding='utf-8') as f:
        mg_text = f.read()
    mg_symbols = re.findall(r'lv2:symbol\s+"([^"]+)"', mg_text)

    with open(html_path, 'r', encoding='utf-8') as f:
        html_text = f.read()
    html_symbols = set(re.findall(r'mod-port-symbol="([^"]+)"', html_text))

    print(f'=== {p} ===')
    print(f'TTL ports: {len(ttl_symbols)}, Modgui ports: {len(mg_symbols)}, HTML symbols: {len(html_symbols)}')
    missing_in_html = [s for s in mg_symbols if s not in html_symbols]
    print(f'In modgui.ttl but MISSING from HTML ({len(missing_in_html)}):', missing_in_html)
    missing_in_mg = [s for s in ttl_symbols[4:] if s not in mg_symbols]
    print(f'In TTL (controls) but MISSING from modgui.ttl ({len(missing_in_mg)}):', missing_in_mg)
    print()
