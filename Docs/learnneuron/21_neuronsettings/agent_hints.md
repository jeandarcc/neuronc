# Agent Hints

AI-specific compiler guidance that helps coding assistants fix violations.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `agent_hints` | `true` | Append AI-specific guidance to compiler error messages |

---

## How It Works

When `agent_hints = true`, every compiler error includes an extra **`For agents:`** suffix with explicit fixing instructions:

```
neuronc: error: Class 'Player' must explicitly declare visibility
(public/private).
For agents: Declare class visibility explicitly:
`public Player is class { ... }`.
```

```
neuronc: error: Method 'ProcessInput' exceeds maximum allowed length
(72 lines, limit 50).
For agents: Split large methods into smaller helpers.
```

```
neuronc: error: Missing script documentation:
expected 'docs/scripts/Brain.md'.
For agents: Create docs/scripts/<ScriptName>.md before compiling.
```

```
neuronc: error: Class member 'Player.health' must explicitly declare
visibility (public/private).
For agents: Add `public` or `private` before class fields and methods.
```

```
Error: Scripts cannot live at repository root when
forbid_root_scripts = true: 'MyCode.nr'.
For agents: Move root scripts under folders like src/,
modules/, or tests/.
```

---

## Why This Exists

AI coding assistants read compiler output to determine their next action. Standard error messages are written for humans â€” they assume context that an agent may not have.

Agent hints add the **missing context**:
- They tell the agent **exactly what to do** to fix the error
- They use the **exact syntax** the agent should write
- They match the agent's mental model of "read error â†’ apply fix"

This turns the Neuron compiler into a **real-time tutor** for AI agents, ensuring they learn from each error and produce compliant code on subsequent attempts.

---

## Disabling

```ini
agent_hints = false
```

When disabled, error messages contain only the standard developer-facing text without the `For agents:` suffix.

---

## Next Steps

- [Overview](overview.md) â€” Back to neuronsettings overview
