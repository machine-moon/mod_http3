# Contributing to mod_http3

Welcome to the `mod_http3` project! We are glad you want to help improve this Apache httpd HTTP/3 module.

As a project based on the principles of open source and collaboration, we welcome contributions of all kinds, whether they are bug reports, documentation improvements, feature development, or release testing.

This guide outlines our community standards, governance, and development procedures. Please read it carefully before participating.

---

## Community Values

We strive to maintain a friendly, welcoming, and productive environment.
- We leave personal issues behind us.
- We argue about content and technical facts, not about thin air.
- We follow the Internet Netiquette as defined in [RFC 1855](https://datatracker.ietf.org/doc/html/rfc1855).

Discussions should remain respectful, professional, and focused on building a quality system.

---

## Bug Reports

If you spot a bug, please check the following before opening an issue:
1. **Clean codebase**: The bug occurs on an unmodified version of the main branch.
2. **Reproducible**: You can consistently reproduce the issue.
3. **No Duplicates**: Search the open and closed issues to ensure the bug has not already been reported.

When creating a new bug report, please include:
- The **commit hash** (revision) where you encountered the bug, and when it first appeared if known.
- Revisions/versions of relevant dependencies (e.g., OpenSSL, httpd, APR, compiler version).
- A detailed, step-by-step guide to reproduce the bug.
- A detailed explanation of **what does happen** vs. **what should happen**.
- For request- or protocol-specific bugs, include target HTTP headers, TLS configs, and the `curl` commands used to reproduce.
- For crashes, include a detailed crash log (such as GDB logs from a debug build).
- Only report **one bug per issue**.

---

## Roles

The `mod_http3` project operates as an Apache project. There are three roles:

1. **Developers**: Anyone contributing time, code, documentation, or other resources.
2. **Committers**: Volunteers responsible for the technical aspects of the project. Committers have write access to the source repositories and cast binding votes on technical discussions.
3. **Project Management Committee (PMC)**: A group of active committers responsible for the management of the project. This includes defining releases, shared resources, license disputes, and nominating new PMC members or committers.

### Member Activity
Members are considered inactive if they declare themselves so, or if they do not contribute to the project in any form for over **six months**. Inactive members can return to active status by resuming contributions.

---

## Decision Making & Voting

Any developer can vote on any issue. However, only votes cast by **active Committers and PMC members** are binding. Technical changes can also receive a binding vote from the primary author of the code being changed.

### Vote Types
Votes are cast using the following symbols:
* **`+1`**: Yes, agree, or the action should be performed. (On technical changes, this implies the voter has compiled and tested the patch locally).
* **`±0`**: Abstain, no opinion.
* **`-1`**: No. **This is a veto.** All vetos must include a detailed technical explanation of why the change is inappropriate. A veto without a technical explanation is void. Vetos cannot be overruled; if you disagree with a veto, you must discuss it and lobby the person who cast it to rescind it.

### Approval Thresholds
* **Consensus**: Requires at least 3 binding `+1` votes and no vetos (`-1`).
* **Majority**: Requires at least 3 binding `+1` votes and more `+1` than `-1` votes.
* **Lazy Consensus**: Action is approved unless someone objects (`-1`). If a veto is cast, the issue must be resolved by consensus or majority vote.

---

## Development & Git Workflow

### Topic Branches
We recommend developing contributions on dedicated topic branches:
```sh
git checkout -b feature/my-new-feature
```
- **Commit often, commit early**: Smaller commits are easier to review and test.
- **Write descriptive commit messages**:
  - Explain *why* the change is being made and what it does.

### Coding Standards
All code must adhere to the project's Coding Standards (see docs/coding-standards.md). You can format your code using the provided `.clang-format` configuration:
```sh
clang-format -i mod_http3/src/*.c mod_http3/include/*.h
```

### Submitting Patches
- Minor changes (< 20 lines) can be posted directly as a diff in a GitHub issue (use markdown code blocks).
- Major changes and feature contributions **must** be submitted via Pull Requests.
- Ensure all code builds successfully and tests pass before submitting.
- Any committed third-party code must be covered by the Apache License 2.0 or compatible licenses.

---

## Committing Security Fixes

When a security vulnerability is identified:
- The fix should be prepared privately (e.g., in coordination with the security contacts).
- Once the fix is ready and the project agrees on the disclosure timeline, the code is committed.
- The commit message and `CHANGES` file entry must provide the best available description and include the CVE identifier (e.g., `SECURITY: CVE-YYYY-NNNN`).
- We do not obscure security fixes. Commits are pushed once the vulnerability is ready to be disclosed.

---

## Release and Backports

For detailed information on the versioning scheme, release candidates, testing, and voting on releases, see the Release Process Guide in docs/release-process.md.
