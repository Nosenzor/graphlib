---
name: mpl-parity
description: Establish matplotlib ground truth before designing or implementing any graphlib feature. Use when adding or changing any API that exists in matplotlib, when unsure about a default value or behavior ("what would matplotlib do?"), or when a test needs oracle fixtures. Produces a docs/PARITY.md entry and optionally committed fixtures.
---

# matplotlib parity workflow

matplotlib is graphlib's requirement document. The **primary oracle is the local installation**
(matplotlib 3.10.x — matplotlib.org 403s non-browser fetchers, so don't rely on WebFetch; the
installed package has every docstring and algorithm anyway).

## 1. Interrogate the oracle

Signature + docstring:
```bash
python3 -c "import inspect, matplotlib.pyplot as p; print(inspect.signature(p.errorbar)); print(p.errorbar.__doc__[:4000])"
```

Actual source — for algorithms you must port faithfully (tick locators, legend 'best',
tight_layout, path simplification):
```bash
python3 -c "import inspect, matplotlib.ticker as t; print(inspect.getsource(t.MaxNLocator._raw_ticks))"
```

Defaults:
```bash
python3 -c "import matplotlib as m; print({k: v for k, v in m.rcParams.items() if 'legend' in k})"
```

Behavioral probe — what does it actually compute/draw:
```bash
python3 - <<'EOF'
import matplotlib; matplotlib.use("svg")
import matplotlib.pyplot as plt
fig, ax = plt.subplots()
ax.plot([0, 1.7, 3.14], [1, -2, 4])
fig.canvas.draw()
print("xticks:", ax.get_xticks(), "xlim:", ax.get_xlim())
fig.savefig("/tmp/ref.svg")   # inspect the emitted SVG structure too
EOF
```

Docs URLs for the human to browse: `https://matplotlib.org/stable/api/_as_gen/matplotlib.pyplot.<fn>.html`.

## 2. Decide the C++ mapping

Apply AGENTS.md §API conventions: same names, kwargs → options-struct fields with rcParams
defaults (cite the rc key in a comment), matplotlib string tokens accepted. Scope-cut kwargs
explicitly: an mpl-valid token we don't support yet throws `graphlib::ValueError` naming the
milestone that adds it — never silently ignore.

## 3. Record

Add or update the feature row in docs/PARITY.md (target release, status, notes on unsupported
kwargs). Deviating from mpl behavior ⇒ row in the Deviations table with a one-line why. Porting a
nontrivial algorithm ⇒ comment at the C++ site naming the mpl source file + function + version.

## 4. Fixtures for tests (when numeric behavior matters)

Extend `tests/fixtures/generate.py` to dump oracle values as JSON: tick positions for given
(limits, axis length), color parses, dash patterns, transform outputs, contour polylines.
**Commit the JSON** — CI has no Python. Unit tests read fixtures and compare exactly (or with
stated tolerance).

## Canonical facts already locked — don't re-derive

docs/DESIGN.md has: defaults table (§12), `transData = transScale + (transLimits + transAxes)`
(§4), renderer contract (§7), path codes incl. CLOSEPOLY=79 (§5), zorders (§2),
AutoLocator = MaxNLocator(nbins='auto', steps=[1, 2, 2.5, 5, 10]) (§8), dash patterns
`'--'`=(3.7,1.6) `'-.'`=(6.4,1.6,1.0,1.6) `':'`=(1.0,1.65) at lw=1, ×linewidth (§6), tab10 cycle
and the color grammar (§9), named font-size factors (§10).
