# TC-22 responsive fix and cross-browser retest

**PASS**, 2026-11-07. Frontend source:
`00ffd7c5e9fb9823e9f822b354b8d790baac652f`.
The initial [SV-01 failure](../final-2026-11-07/TC-22_COMPARISON.md) is retained
as historical evidence. This retest supersedes its outstanding-fix status.

## Change and provenance

The dynamically rendered dashboard and audit tables lacked the existing
`table-scroll` wrapper used elsewhere. Their containers now use that class and
have `role="region"`, accessible names and `tabindex="0"`. Wide tables scroll
inside their cards; columns are retained and can be reached. No page-level
overflow suppression or data truncation was added.

The backend executable was built at `b0e8ca24fdd348bf40b0deb68f769232cc650107`.
`git diff b0e8ca2 00ffd7c -- src db cmake CMakeLists.txt` is empty, so the same
backend binary was reused with the fixed frontend. This is a frontend retest,
not a claim that the executable was rebuilt at the new commit.
[Hashes and full source identities](provenance.json).

Each browser's core and responsive run had its own fresh schema/seed database and
new server process: four independent databases in total. Core runs used the
merged #134 walkthrough. Responsive checks waited for dashboard/audit content
and checked all roles at 1280, 768 and 390 CSS pixels (height 900).

## Results

| Check | Chrome 156.0.8078.80 | Firefox 159.0.1 |
|---|---|---|
| Core workflows, all roles | 21/21 pass | 21/21 pass |
| Officer at 1280 / 768 / 390 | Fits at all sizes | Fits at all sizes |
| Customer at 1280 / 768 / 390 | Fits at all sizes | Fits at all sizes |
| Administrator at 1280 / 768 / 390 | Fits at all sizes | Fits at all sizes |
| Dashboard/audit table regions | Labeled, focusable, columns reachable | Labeled, focusable, columns reachable |
| Uncaught JavaScript errors | 0 | 0 |

All nine paired role/viewport measurements have identical document widths,
visible card widths, headings and navigation labels across browsers. At 390px,
both browsers now have a 390px document for all roles. Screenshots were inspected;
the previously overflowing tables stay within their cards. Native date-input
appearance still varies between browser engines; pixel-identical native widgets
are not claimed.

- Chrome: [core](chrome/core.json), [core metadata](chrome/core-metadata.json),
  [responsive assertions/layouts](chrome/responsive.json),
  [responsive metadata](chrome/responsive-metadata.json).
- Firefox: [core](firefox/core.json), [core metadata](firefox/core-metadata.json),
  [responsive assertions/layouts](firefox/responsive.json),
  [responsive metadata](firefox/responsive-metadata.json).
- [Nine paired comparisons](comparison.json).
- Customer mobile: [Chrome](chrome/customer1-390.png),
  [Firefox](firefox/customer1-390.png).
- Administrator mobile: [Chrome](chrome/admin-390.png),
  [Firefox](firefox/admin-390.png).
- Full-page screenshots for each role and viewport are alongside these files.

The core logs retain intentional wrong-password/frozen-account HTTP failures and
any favicon 404; these are distinct from uncaught JavaScript errors.

## Portability scope and remaining gate

The unchanged backend has passing Ubuntu/Windows build-and-test evidence in
[CI run 42280507786](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42280507786),
including execution through API/integration tests. These browser walkthroughs ran
on Windows; a separate interactive Ubuntu deployment was not performed. PR CI
must also pass before review/merge. TC-22's recorded mobile defect is resolved;
TC-21 is NOT RUN under [documented deviation 33](../../../DEVIATIONS.md#tc-21-disposition-deviation-33).
The later user request directs administrative closure of S-43 with this gap
recorded; no usability pass, repo-owner approval or release authorization is implied.
