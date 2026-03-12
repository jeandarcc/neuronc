# Testing Rules

Rules that integrate automated testing into the build pipeline.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `max_auto_test_duration_ms` | `5000` | Maximum time (ms) for the test suite to complete |

---

## How `tests/auto` Works

When a project has a `tests/auto/` directory and a compiled test binary (`build/bin/neuron_tests`), the compiler **automatically runs** the test suite as a build gate.

### Build Gate Flow

1. Source files are compiled
2. If `build/bin/neuron_tests` exists **and** `tests/auto/` exists:
   - The test binary is **executed automatically**
   - If tests **fail** → build is **blocked**
   - If tests **exceed the time limit** → build is **blocked**
3. Only after tests pass does the build succeed

### On Failure

```
Build blocked: automated tests failed.
```

### On Timeout

```
Build blocked: automated tests exceeded 5000 ms (actual: 7234 ms).
```

---

## `max_auto_test_duration_ms = 5000`

The test suite must complete within 5 seconds (5000ms). If it takes longer, the build fails even if all tests pass.

**Why:** This creates a healthy constraint:
- Prevents test suites from growing unbounded
- Keeps the development feedback loop fast
- Forces slow tests to be optimized or moved out of the auto suite

---

## Project Structure

```
my_project/
├── tests/
│   ├── auto/         ← auto-run tests live here
│   └── unit/         ← manual tests (not auto-run)
├── build/
│   └── bin/
│       └── neuron_tests    ← compiled test binary
```

---

## Why This Matters

This creates a **continuous verification loop**:
- Every build runs tests automatically
- Broken tests = broken build = no deployment
- Slow tests = failed build = must optimize
- No way to "skip tests just this once"

For AI agents: if an agent modifies code and breaks existing tests, the build fails. The agent must fix the tests before the change can be accepted.

---

## Next Steps

- [Agent Hints](agent_hints.md)
