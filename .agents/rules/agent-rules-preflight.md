---
trigger: always_on
---
# AGENT_RULES.md Is Binding
- At the start of every task (and after every context summary), read `AGENT_RULES.md` in full with `view_file` before editing code or running deploy commands.
- If a user request conflicts with `AGENT_RULES.md`, say so before proceeding.
- Before reporting completion, walk the "Rule 26 Completion & Deploy Checklist" in `AGENT_RULES.md` item-by-item and report which items passed, quoting the command + exit code for each.
