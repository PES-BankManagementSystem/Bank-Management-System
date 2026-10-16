# ADR 001: Technology stack

- **Status:** Accepted
- **Date:** 2026-10-16
- **Story:** S-01 (#31)
- **Resolves:** SRS Appendix C TBD-1 (technology stack), TBD-4 (password hashing), TBD-6 (hosting)

## Context

The SRS leaves the technology stack open (SRS 2.5) but fixes these constraints:

- All banking logic lives in a C/C++ core. The front end has no business logic (SRS 2.5, SADS 4).
- The core builds and runs on Linux and Windows. The UI works in Chrome and Firefox (NFR-9).
- The database is relational, with foreign keys and ACID transactions (SRS 2.5, SRS 6, NFR-3).
- The API runs over HTTP/HTTPS and uses JSON (SRS 3.3, SADS 3.2).
- Passwords and PINs are stored with a salted one-way hash (SEC-2, NFR-4).
- The code is organised by functional area (NFR-8).

The project has four people and three one-week sprints. Every tool below was picked to install cleanly on Linux and Windows without system packages, so that `cmake` and a compiler are enough to build.

## Decision

| Concern | Choice | Version | How it is obtained |
|---|---|---|---|
| Language | C++17 | - | GCC 9+, Clang 10+, MSVC 2019+ |
| Build system | CMake | 3.24+ | Installed by each developer |
| HTTP server (API layer) | [cpp-httplib](https://github.com/yhirose/cpp-httplib) | v0.57.1 | CMake `FetchContent` |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) | v3.12.0 | CMake `FetchContent` |
| Database | [SQLite](https://sqlite.org) (amalgamation, compiled into the server) | 3.53.3 | CMake `FetchContent` |
| Password hashing and secure random numbers | [libsodium](https://libsodium.org): Argon2id via `crypto_pwhash_str` | 1.0.x via [libsodium-cmake](https://github.com/robinlinden/libsodium-cmake) | CMake `FetchContent` |
| Unit tests | [GoogleTest](https://github.com/google/googletest), run by `ctest` | v1.17.0 | CMake `FetchContent` (added in S-06) |
| Front end | Plain HTML, CSS and JavaScript (no framework, no build step) | - | Served as static files by the C++ server |
| CI | GitHub Actions, `ubuntu-latest` and `windows-latest` | - | `.github/workflows/ci.yml` |

FetchContent pins every dependency to a tag, commit or archive hash, so all machines and CI build the same code.

### Why these choices

- **C++17 with CMake.** CMake is the usual cross-platform build for C++. It drives GCC on Linux and MSVC on Windows from one `CMakeLists.txt`, which covers NFR-9. C++17 has `std::optional`, `std::string_view` and `std::filesystem`, and every compiler on CI supports it fully.
- **cpp-httplib.** A single header with no dependencies that runs on Linux and Windows. It has routing with path parameters, static file serving, and optional TLS through OpenSSL, which S-28 will need. Larger frameworks such as Crow, Drogon or Boost.Beast need Boost or several system packages, and that would slow setup down for everyone.
- **nlohmann/json.** The most widely used C++ JSON library. It is header-only, and its syntax is easy to read in a code review.
- **SQLite.** It is relational, enforces foreign keys (`PRAGMA foreign_keys = ON`) and has full ACID transactions, which covers NFR-3. It compiles into the server, so there is no database server to install or configure on each laptop or on CI. Each test can create a fresh temporary database (S-06). MySQL and PostgreSQL were considered and rejected: their only advantage here, concurrent writers, does not matter at this scale (NFR-1 assumes about 3,000 accounts).
- **libsodium / Argon2id (TBD-4).** Argon2id won the Password Hashing Competition and is the current OWASP recommendation, and SEC-2 already names Argon2. `crypto_pwhash_str` stores the salt and cost parameters inside the hash string, so the `users.password_hash` column holds a single value. libsodium also provides `randombytes_buf`, a cryptographically secure random number generator on both platforms, which session tokens need (S-08). The `libsodium-cmake` wrapper builds libsodium with CMake so it works with MSVC.
- **Plain HTML/CSS/JS front end.** No Node.js toolchain and no bundler. The C++ server serves `frontend/` from the same origin as the API, so the browser needs no CORS setup. Every member can open the files and edit them. Since the front end has no business logic, a framework would add little.

### Hosting (TBD-6)

- The **demo and evaluation** run on a team member's laptop, with all three tiers on one machine (SADS 2.5): the browser, the `bms_server` binary, and the SQLite database file. The demo videos are recorded on that setup.
- Plain HTTP on `localhost` is used for development and the demo. SRS 3.4 allows this as long as no real customer data is used, and the seed data is fictional.
- TLS support is still built (S-28) so the server can be run on a network if needed. It takes the certificate and key paths from configuration.
- There is no cloud deployment. It is not required, it would cost money, and it would put secrets and a public endpoint on a student project.

## Consequences

- **Concurrency:** SQLite locks the whole database for a write, not single rows. Balance-changing operations will use `BEGIN IMMEDIATE`, so writers queue up and none sees stale data. SADS 3.3 describes row-level locking, but SRS Appendix D lists record-level locking as future work. This difference is recorded in [`docs/DEVIATIONS.md`](../DEVIATIONS.md), and S-36 updates the SADS text.
- **First build:** the first build needs internet access to fetch the dependencies, and later builds reuse the downloads in `build/_deps`.
- **TLS:** HTTPS will need OpenSSL on the machine running the server (S-28). It is optional for the local demo.
- **Business rules:** TBD-2 (account types), TBD-3 (daily limit) and TBD-5 (interest basis) are business-rule decisions. They are recorded with S-03 and S-22, not here.
