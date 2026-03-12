# Visibility Rules

Rules that enforce explicit access modifier declarations.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `require_class_explicit_visibility` | `true` | Every class must declare `public` or `private` |
| `require_property_explicit_visibility` | `true` | Every field and method must declare `public` or `private` |

---

## `require_class_explicit_visibility = true`

Every class declaration must include an access modifier:

```npp
// ❌ Rejected — no visibility
Player class { }

// ✅ Required
Player public class { }
```

```
neuronc: error: Class 'Player' must explicitly declare visibility
(public/private).
For agents: Declare class visibility explicitly:
`public Player is class { ... }`.
```

---

## `require_property_explicit_visibility = true`

Every field and method inside a class must have `public` or `private`:

```npp
// ❌ Rejected — implicit visibility
Player public class {
    health is 100;
    Run method() { }
}

// ✅ Required — explicit visibility
Player public class {
    health private is 100 as int;
    Run public method() { }
}
```

```
neuronc: error: Class member 'Player.health' must explicitly declare
visibility (public/private).
For agents: Add `public` or `private` before class fields and methods.
```

---

## Why These Rules Matter

Implicit visibility is a common source of bugs and security issues:
- A field accidentally exposed as public can be mutated from anywhere
- An internal method accidentally public becomes part of the API contract

For AI agents, this is critical — they tend to forget access modifiers, which can accidentally expose internal implementation details.

---

## Next Steps

- [Complexity Rules](complexity_rules.md)
