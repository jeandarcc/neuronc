# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| `main` branch | ✅ |
| Tagged releases | ✅ |
| Older branches | ❌ |

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

To report a security vulnerability, please open a
[GitHub Security Advisory](https://github.com/your-org/neuron-pp/security/advisories/new)
on this repository. This allows us to coordinate a fix before public disclosure.

### What to include

- Description of the vulnerability and its potential impact
- Steps to reproduce (minimal reproducer preferred)
- Affected component(s): compiler (`neuronc`), runtime, `ncon`, LSP server, etc.
- Any suggested mitigations

### Response timeline

| Stage | Target |
|-------|--------|
| Initial acknowledgement | Within 72 hours |
| Triage & confirmation | Within 7 days |
| Fix release | Depends on severity (critical: ASAP, high: ≤30 days) |
| Public disclosure | After fix is released |

## Scope

This policy covers:
- `neuronc` compiler binary
- `ncon` package manager
- `neuron-lsp` language server
- Runtime library (`runtime/`)
- IDE extensions (`extensions/`)

Out of scope: third-party dependencies (LLVM, libffi). Report those upstream.
