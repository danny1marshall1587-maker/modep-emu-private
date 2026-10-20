const fs = require("fs");
const html = fs.readFileSync("/usr/share/mod/html/mixer/index.html", "utf8");
const scripts = [];
const re = /<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/gi;
let m;
while ((m = re.exec(html)) !== null) {
  scripts.push(m[1]);
}
console.log("Extracted scripts count:", scripts.length);
scripts.forEach((code, idx) => {
  if (!code.trim()) return;
  try {
    new Function(code);
    console.log(`Script ${idx}: syntax OK (length: ${code.length})`);
  } catch(e) {
    console.error(`Script ${idx} SYNTAX ERROR: ${e.message}`);
  }
});
