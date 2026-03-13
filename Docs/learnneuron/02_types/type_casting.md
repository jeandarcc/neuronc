# Type Casting

Neuron uses the `as` keyword for type conversions, with `maybe` for safe (nullable) casts and `then` for chaining multiple conversion steps.

---

## Simple Cast

Convert a value to a different type using `as`:

```npp
value is 10 as int;
value as float;          // convert int → float
```

---

## Nullable Cast with `maybe`

The `maybe` modifier makes a cast safe — if the conversion fails, the result is `null` instead of an error:

```npp
value is "hello";
value as maybe int;      // returns null if can't parse as int
```

Without `maybe`, a failed cast produces a runtime error. With `maybe`, it fails gracefully.

---

## Cast Pipeline with `then`

Use `then` to chain multiple conversion steps in sequence:

```npp
value is 10 as int;
value as maybe dynamic then string then float;
```

This performs a **cast pipeline**:
1. Cast `value` to `dynamic` (nullable — `maybe` applies to the whole pipeline)
2. Then convert to `string`
3. Then convert to `float`

Each step feeds into the next.

---

## Parenthesized Cast Steps

You can parenthesize individual steps for explicit nullable control:

```npp
value as (maybe int);        // nullable cast to int
value as (maybe float);      // nullable cast to float
```

---

## Pipeline Syntax Summary

| Syntax | Meaning |
|--------|---------|
| `x as int` | Cast `x` to `int` (error on failure) |
| `x as maybe int` | Cast `x` to `int` (null on failure) |
| `x as int then string` | Cast to `int`, then to `string` |
| `x as maybe int then float` | Nullable pipeline: `int` → `float` |
| `x as (maybe int)` | Parenthesized nullable single cast |

---

## Real Example

From `CSharpFeatures.nr`:

```npp
value is 10 as int;
value as maybe dynamic then string then float;
```

---

## Next Steps

- [Constants](constants.md) — Compile-time constant values
