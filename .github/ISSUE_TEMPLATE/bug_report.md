---
name: Bug report
about: Create a report to help us improve mod_http3
title: ''
labels: 'bug'
assignees: ''
---

<!-- 
Before submitting, please ensure:
1. You are on a clean, unmodified codebase (main branch or latest release).
2. You can consistently reproduce the issue.
3. You have searched open and closed issues to ensure this is not a duplicate.
4. You are reporting only ONE bug per issue.
-->

**Describe the bug**
A detailed explanation of **what does happen** vs. **what should happen**. Include when it first appeared, if known.

**To Reproduce**
A detailed, step-by-step guide to reproduce the bug.
* For request- or protocol-specific bugs, include target HTTP headers, TLS configs, and the exact `curl` commands used to reproduce.

**Expected behavior**
A clear description of what you expected to happen.

**Environment (please complete the following information):**
 - OS: [e.g. Ubuntu 22.04]
 - Compiler Version: [e.g. GCC 11.3.0]
 - mod_http3 Version: [e.g. 0.1.0 or abc1234]
 - httpd Version: [e.g. 2.5.0-trunk]
 - OpenSSL Version: [e.g. 3.5.0]
 - nghttp3 Version: [e.g. 1.17.0]
 - APR Version: [e.g. 1.7.0]

**Crash Logs & Additional Context**
If this is a crash, please include a detailed crash log (such as GDB logs from a debug build). Add any other context about the problem here (e.g., error logs, packet captures, TLS traces).
