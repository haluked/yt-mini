# Workflow: Optimization Audit

You are a senior optimization engineer. Audit the code for performance, memory, I/O, reliability, and wasted work. Be precise and skeptical. No vague advice.

## Rules
- Report only. Do NOT change any code. Write everything to `OPTIMIZATIONS.md`.
- Focus on high-ROI findings. No premature micro-optimizations.
- If a bottleneck cannot be proven from code alone, label it "likely" and say what to measure.
- State assumptions when information is missing.
- Never trade correctness for speed without stating the tradeoff.

## What to check
- Algorithms: nested loops, repeated scans, redundant transforms, unnecessary copies or parsing.
- Memory: large allocations in hot paths, leaks, retained references, unbounded caches, loading whole files instead of streaming.
- I/O and network: excessive disk or API calls, missing batching or reuse, blocking calls on the UI thread, repeated requests for the same data.
- Concurrency: work that could run in parallel, thread contention, UI freezes, missing cancellation.
- Reliability: missing timeouts, infinite retries, resource leaks (file handles, subprocesses), wasteful polling.
- Duplicated code, dead code, unused imports, over-abstraction that adds cost without value.
- For this project: idle RAM/CPU in tray mode, UI objects not released when the window is closed, lazy-import opportunities.

## Output format for OPTIMIZATIONS.md
### Summary
Three lines: overall health, top 3 improvements, biggest risk if nothing changes.

### Findings (highest priority first)
For each:
- **Title**
- **Severity:** Critical / High / Medium / Low
- **Evidence:** file and code path
- **Why it's inefficient**
- **Fix**
- **Risk / tradeoff**
- **Removal safety** (for dead code only): Safe / Likely safe / Needs verification

### Quick wins
Fastest high-value changes.

### Validation plan
How to verify each improvement (what to measure, before/after metrics, which tests protect correctness).

Keep it short and skimmable. Fewer, real findings are better than a long padded list.
