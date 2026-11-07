# TC-22 Chrome / Firefox comparison and defect

> Historical initial failure. SV-01 is resolved by the [fixed-revision retest](../tc22-retest-2026-11-07/TC-22_RETEST.md); measurements below are preserved unchanged.

Tested 2026-11-07 on Windows, build
`b0e8ca24fdd348bf40b0deb68f769232cc650107` (merged PR #134).
Chrome `156.0.8078.80`; Firefox `159.0.1` (isolated extracted binary).

**Result: FAIL — mobile responsiveness and identical rendering are not met.**

Each browser's core walkthrough and responsive run used its own new schema/seed
database and server process. No browser shared a mutated database with another.
The merged `scripts/browser-check/walkthrough.js` completed all core steps in both
browsers with zero uncaught JavaScript errors. Intentional wrong-password and
frozen-account rejections occurred; Chrome additionally reported a favicon 404.
The complete request-error lists are retained, not suppressed.

Core results: [Chrome](browsers/chrome/core.json),
[Firefox](browsers/firefox/core.json). Fresh-seed/build metadata:
[Chrome](browsers/chrome/core-metadata.json),
[Firefox](browsers/firefox/core-metadata.json).

## Responsive comparison

All three roles were measured at 1280, 768 and 390 CSS pixels, height 900.
Both browsers show the same role headings, navigation labels and visible card
sets. At 1280 and 768 pixels neither browser has page-level horizontal overflow.
At 390 pixels, Officer layouts fit, but Customer and Administrator layouts fail.

| Role / viewport | Chrome document width | Firefox document width | Result |
|---|---:|---:|---|
| Officer / 1280 | 1280 | 1280 | Fits |
| Officer / 768 | 768 | 768 | Fits |
| Officer / 390 | 390 | 390 | Fits |
| Customer / 1280 | 1280 | 1280 | Fits |
| Customer / 768 | 768 | 768 | Fits |
| Customer / 390 | 399 | 399 | **Overflow by 9px in both** |
| Administrator / 1280 | 1280 | 1280 | Fits |
| Administrator / 768 | 768 | 768 | Fits |
| Administrator / 390 | 465 | 486 | **Overflow by 75px / 96px; browsers differ by 21px** |

Measured results: [Chrome](browsers/chrome/responsive.json),
[Firefox](browsers/firefox/responsive.json),
[nine paired layout comparisons](comparison.json). Screenshots:
[Chrome Customer mobile](browsers/chrome/customer1-390.png),
[Firefox Customer mobile](browsers/firefox/customer1-390.png),
[Chrome Administrator mobile](browsers/chrome/admin-390.png),
[Firefox Administrator mobile](browsers/firefox/admin-390.png).
Screenshots for every role/size are alongside these files.

## Defect SV-01: Customer and Administrator mobile horizontal overflow

1. Build the SHA above and start the server on a new schema/seed database.
2. Open Chrome or Firefox with a 390 x 900 viewport.
3. Sign in as `customer1` or `admin` using the documented demo credentials.
4. Observe dashboard/account tables (Customer) and audit content (Administrator)
   extending beyond the narrow card/page. Measure
   `document.documentElement.scrollWidth` against `window.innerWidth`.

Expected: content remains within the viewport, with any wide table contained in
its own accessible scrolling region; essential data/actions remain available.
Actual: page width exceeds the viewport as recorded above. Administrator page
width also differs across engines. Impact: mobile usability/rendering; no data
loss or financial-operation failure was observed. A product fix and fresh
Chrome/Firefox retest are required before TC-22/NFR-9 can pass. This local defect
record has not been posted as a GitHub issue.

## Operating-system evidence

[CI run 42280507786](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42280507786)
at the same SHA passed both `ubuntu-latest` and `windows-latest` build/test jobs;
[saved job evidence](ci.json). These exercise the backend via automated tests.
The interactive browser/backend runs in this record were on Windows with MinGW;
no separate interactive Ubuntu deployment was performed. CI portability evidence
does not override the observed browser failures or establish pixel-identical UI.
