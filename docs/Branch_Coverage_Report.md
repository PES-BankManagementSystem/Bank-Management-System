# Branch Coverage Report — S-42 (#118)

## Required S-40 CI result

The required CI source is the successful [S-40 coverage job](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060/job/127045274980) for commit `31f1cf48659dbe1a559d6e5eeb321b1d30e16281`. Its `coverage-report` artifact (ID `13387499446`) with CI HTML and summary is on the [workflow run page](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060).

**S-40 CI branch percentages: NOT AVAILABLE: artifact download returned HTTP 401; download the artifact from the linked run page while signed in and copy the module/overall branch percentages from `summary.json`.** The report does not substitute local numbers for CI data.

| Module | S-40 CI branch coverage |
|---|---:|
| account | NOT AVAILABLE |
| api | NOT AVAILABLE |
| audit | NOT AVAILABLE |
| auth | NOT AVAILABLE |
| common | NOT AVAILABLE |
| customer | NOT AVAILABLE |
| main | NOT AVAILABLE |
| reporting | NOT AVAILABLE |
| staff | NOT AVAILABLE |
| transaction | NOT AVAILABLE |
| **Overall** | **NOT AVAILABLE** |

The measured local HTML report is 5.4 MB, above the requested size limit. It is not committed; use the linked CI artifact for HTML details. Coverage uses **gcovr 8.8**, `--root . --filter 'src/' --gcov-executable 'gcov' --exclude-throw-branches --exclude-unreachable-branches`. The only inclusion filter is `src/`; tests, fetched dependencies and system headers outside `src/` are not counted. Throw and unreachable branches are excluded. See [Code Coverage Report](Code_Coverage_Report.md) for the CI job, instrumentation, local toolchain and exact reproduction notes.

## Local macOS branch coverage before and after test additions

These are AppleClang 17 / `llvm-cov gcov` / gcovr 8.8 local results, not the S-40 CI values. The before run used the original 29-target suite; the after run includes the additional focused tests and 31 CTest targets. Numerators, denominators and percentages come directly from gcovr's `summary.json` and `summary.md`.

| Module | Before | After | 60% threshold after? |
|---|---:|---:|---|
| account | 51.9% (919/1772) | 52.0% (922/1772) | Below |
| api | 42.6% (187/439) | 44.0% (193/439) | Below |
| audit | 54.3% (189/348) | 54.3% (189/348) | Below |
| auth | 53.6% (398/743) | 53.6% (398/743) | Below |
| common | 41.2% (226/548) | 45.6% (250/548) | Below |
| customer | 54.5% (792/1454) | 54.5% (792/1454) | Below |
| main | 5.6% (6/108) | 9.3% (10/108) | Below |
| reporting | 51.9% (667/1286) | 51.9% (667/1286) | Below |
| staff | 47.4% (512/1080) | 47.4% (512/1080) | Below |
| transaction | 50.4% (527/1046) | 51.3% (537/1046) | Below |
| **Overall** | **50.1% (4423/8824)** | **50.7% (4470/8824)** | **Below** |

The added tests target deterministic error paths: invalid account-number generation and transactional no-row behavior, closed-database guards, customer permission refusal, deposit/transfer balance overflow, the API unknown-exception handler and the server help path. Existing module tests already exercise validation, permission refusal, not-found, conflict and rollback cases in the audit, auth, customer, reporting and staff modules. No files were excluded to increase the percentages.

## Remaining branch sites and explanation

The line ranges below were generated from detailed local gcovr JSON and contain every source line with at least one unexecuted branch outcome after the added tests. For readability, neighboring sites were compressed; a range can contain covered statements between uncovered branch sites. The linked CI artifact remains authoritative for CI data.

| Module | Source file | Remaining branch-site lines |
|---|---|---|
| account | `src/account/account_opening.cpp` | 27, 31–32, 37–38, 42, 44, 52–53, 60–61, 74, 77, 79–80, 82–83, 85–86, 97–98, 100–102, 104–108, 116–121, 123, 125–126, 129–133, 137–138, 142–145, 149–155, 158, 160–164, 166, 171–172, 176–178, 181–188, 192–194, 196, 198–204, 207–211 |
| account | `src/account/account_queries.cpp` | 14, 23, 25–27, 29, 32, 36, 38–40, 47, 51–53, 55–57 |
| account | `src/account/account_status.cpp` | 13, 17–18, 27, 41–43, 52–53, 60–61, 71–81, 84, 86–89, 94–105, 108 |
| account | `src/account/account_types.cpp` | 22, 30, 37, 44, 46–48, 50, 52, 56, 60, 63, 69, 71–75, 80, 83–86, 94, 97–98, 101, 104, 107, 114, 118–120, 129, 131–132, 134–145, 152–153, 155–168 |
| account | `src/account/routes.cpp` | 36–38, 44, 53, 58–61, 67–78, 86–107, 114, 116–117, 121–124, 127, 131–132, 136, 146, 160, 165–171, 178–195, 199–215, 219–233, 237, 240–243, 248–259, 264–274, 280, 282–284, 289, 291, 294, 296, 298–305, 313, 322–323, 331–332, 338–349, 356–361, 366–381, 388, 392, 395–406, 408, 410, 412, 416, 419, 422–424, 428, 431–432, 435, 438, 440, 442–443, 446–449, 451, 453, 456, 463–464, 467, 470–472, 474, 477, 481, 483–484, 486, 488, 491, 493–494 |
| api | `src/api/access_log.cpp` | 8–9 |
| api | `src/api/errors.cpp` | 8, 20–22, 24, 26, 35, 40, 44, 48–50 |
| api | `src/api/router.cpp` | 26, 31, 36, 38–39, 41, 49, 52–63, 73, 77, 81, 85, 89 |
| api | `src/api/server.cpp` | 26, 28, 31, 34–39, 45–46, 60–62, 84, 88, 95–99, 102–103, 107–114, 119, 121–122, 127–128, 130, 147–148 |
| audit | `src/audit/record.cpp` | 12, 14, 17–21 |
| audit | `src/audit/routes.cpp` | 26–31, 33, 39, 41, 43, 45–46, 54–56, 64, 69, 71, 73–76, 80–84, 87–88, 96–114, 117, 123–126 |
| auth | `src/auth/password.cpp` | 11–12, 21, 24 |
| auth | `src/auth/rbac.cpp` | 21–23, 26, 30–34, 40, 64, 98–100, 107–109, 120, 122–124, 128, 131–133, 140–141, 149 |
| auth | `src/auth/routes.cpp` | 41, 53–54, 58, 64–70, 75, 79, 85–86, 97, 107, 109–117, 120–121, 123–124, 126, 128, 130, 135, 139, 141, 151, 153, 157–158, 161–163, 166, 168, 173–177, 181–183, 187–192, 195, 198–200, 202–203, 205, 207–208, 210–215, 219–221, 224–226, 229–231, 234–236, 238–239, 242–243, 245–246, 249–256, 260, 262, 265–272, 275, 279, 281, 283–291, 294–295, 297–301, 303–307 |
| common | `src/common/config.cpp` | 14, 19–20, 30–31, 38, 41–46, 51, 56, 58, 60, 62, 64, 66, 68, 72 |
| common | `src/common/config.hpp` | 10, 13 |
| common | `src/common/db.cpp` | 9–10, 16, 20, 23–24, 30–33, 43–49, 53, 56, 62, 74, 80, 84, 86–88, 99, 103–105, 108, 114, 117–118, 124, 127, 133, 136, 143–145, 156–159, 170–177, 182–183, 187–189, 194–195, 199–201, 206–207, 211, 217, 222–223, 227–229, 234–235, 239, 243–244, 248, 252–253, 257, 261, 263 |
| common | `src/common/db.hpp` | 21, 69, 78 |
| common | `src/common/error.cpp` | 18, 22, 27, 31, 35, 39, 44 |
| common | `src/common/input_validation.hpp` | 14, 22, 30 |
| common | `src/common/log.cpp` | 15, 30–31, 35, 41, 52 |
| customer | `src/customer/records.cpp` | 20, 25, 27–28, 46, 51, 56–58, 62–69, 73–77, 83, 85–87, 89, 95–98, 106, 110, 116, 122–128, 135, 137–138, 157–162, 168–175, 177, 179, 182–192, 202, 204–207, 215–216, 218, 221–223, 226–239, 250–261, 263, 265–266, 269–270, 272–279, 281–283, 285–286, 291, 293–299 |
| customer | `src/customer/registration.cpp` | 25, 29–30, 37, 44, 46, 52, 55–57, 62–65, 70–73, 77, 85–89, 91–92, 97–99, 101–102, 104–105, 107–109, 113–115, 118–119, 122–123, 125–133, 135–137, 139–140, 143–151 |
| customer | `src/customer/routes.cpp` | 40–59, 66, 71–77, 85–86, 92–110, 116, 124, 129, 137, 144–145, 149–157, 160–164, 168, 173–175, 178, 183, 186–193, 197, 201–202, 205–216, 221–224, 227, 231–233, 237, 241–253, 255 |
| customer | `src/customer/validation.cpp` | 29, 31, 45, 48, 51, 57, 60, 63, 65–66, 82, 84, 86–89, 94, 96, 103–108, 116–117, 119, 122, 129, 131, 138–140, 142, 145, 147, 153–155, 159, 162–163, 167, 172–173, 177–178, 184–186, 188, 191 |
| customer | `src/customer/validation.hpp` | 9 |
| main | `src/main.cpp` | 32, 36, 47, 56, 58, 61–62, 67, 69–71, 76–77, 80–82, 85–86, 88–93, 96, 98–100 |
| reporting | `src/reporting/interest.cpp` | 21, 25, 29–30, 36–37, 61–64, 80, 84–86, 92, 96–105, 109–111, 115, 125, 127, 129–130, 132, 137–141, 147, 150, 152, 179, 181–182, 186–189, 194–199, 203–205, 210–211, 214–222, 227, 229–230, 233, 235, 238–239, 244–245, 247–249, 252–253, 255–260, 262–268, 272–277, 280 |
| reporting | `src/reporting/routes.cpp` | 23, 30–38, 45, 48, 50, 54–55, 57–58, 61, 63–70, 74–80, 84, 87–88, 91–94, 98, 100–105, 108–114, 118, 122–123, 127, 129–133, 136–140 |
| reporting | `src/reporting/statement.cpp` | 19, 23, 27, 30, 33–36, 40–41, 45–47, 50, 56, 66, 69–70, 74–76, 80–81, 83, 87, 97–98, 103, 111, 115, 121–124, 134–137, 143–153, 158–159, 162–165, 170–181, 188–189, 192–197, 204, 214, 217, 219–223, 229–230, 233, 237–238, 240, 242, 247–250, 252, 254–259, 263–269, 274, 276, 279–282, 285, 287–288, 292–299, 302–303, 305–306, 308, 310–311, 321–322, 324–325, 327–328, 331–332, 334–335, 337–348, 353 |
| staff | `src/staff/routes.cpp` | 29, 35–43, 48, 55–56, 59, 61–67, 78–79, 82, 84, 94, 97–98, 101, 106–107, 115–119, 126, 132, 135–143, 151, 155–159, 161–166, 170–177, 186–198, 203–205, 209–220, 224–229, 234, 237, 240–242, 245–247, 249–250, 252–254, 256, 258–259, 261–270, 272–274, 276, 278, 280, 283–291, 294, 297, 299–303, 306–307, 310, 314–315, 317, 321–322, 325, 327–328, 330–334, 336, 338–339, 342–346, 349–354 |
| transaction | `src/transaction/deposit.cpp` | 17, 22, 26, 28, 35, 39, 46–47, 50–52, 57–58, 63, 79–83, 86–89, 94, 99, 101–103, 107–108, 110–114, 119–121, 124–126, 130–135, 138–139, 145 |
| transaction | `src/transaction/routes.cpp` | 15, 18–20, 24–25, 27–28, 30–31, 34, 37–42, 49–58, 62, 67, 69–70, 73, 77–80, 82–106 |
| transaction | `src/transaction/transfer.cpp` | 27, 31–37, 44, 47–54, 56–58, 62, 65–66, 71–73, 77–80, 88, 90, 92, 101, 103, 113–114, 118, 120–122, 125, 130–132, 135, 138, 140–141, 147, 150–158 |
| transaction | `src/transaction/withdraw.cpp` | 29–30, 33–42, 48–51, 56–57, 60–62, 67–68, 73–74, 76–78, 81–82, 84–88, 93–95, 97–99, 103–107, 110–111 |

**Why gaps remain:** the listed sites are short-circuit guard operands, unselected validation/error alternatives, and defensive branches for failures that ordinary seeded tests cannot force (for example entropy initialization failure, SQLite prepare/bind failures, impossible generated identifiers, or listener failures). Existing tests exercise representative validation, permission, not-found, conflict and rollback paths, but do not exercise every combination of optional request fields, roles, account states, and platform/runtime failures. `main.cpp`'s remaining sites are principally server construction/listening, signal registration, warning and shutdown paths; exercising the whole lifecycle belongs to integration/system verification, not a deterministic unit test. The branch threshold remains unmet in every module after the focused additions; these are reported rather than hidden by exclusions.

## NOT AVAILABLE values and how to fill them

- **S-40 CI branch percentage per module and overall:** download the linked artifact using authenticated GitHub access and copy `branch_percent`, `branch_covered`, and `branch_total` from `summary.json`.
- **CI compiler/gcov version:** inspect the S-40 build log; `ubuntu-latest` is not version-pinned.
