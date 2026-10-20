import re

p1 = "/home/patch/modep-live-console-release/web/js/dsp-rack.js"
s1 = open(p1).read()

# Fix any nested ${window.getChannelRackState(${ch}). inside a template interpolation ${...}
s1 = s1.replace("${window.getChannelRackState(${ch}).", "${state.")

open(p1, "w").write(s1)
print("Fixed template interpolation in dsp-rack.js")
