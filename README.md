# Bank Management System

Web-based bank management system for the PES University Software Engineering mini-project (UE24CS341A). Business logic is in a C++ core, data is stored in SQLite, and the browser front end is plain HTML/CSS/JS.

## Tech stack

C++17, CMake, cpp-httplib, nlohmann/json, SQLite, libsodium (Argon2id), GoogleTest, and a plain HTML/CSS/JS front end. The choices and reasons are in [ADR 001: Technology stack](docs/decisions/001-tech-stack.md).

## Database

The database schema is defined in `db/schema.sql` and seed data in `db/seed.sql`. To create a local database for development or testing:

```bash
sqlite3 bms.db < db/schema.sql
sqlite3 bms.db < db/seed.sql
```

### Demo Credentials
All demo users use the password "Password123!" (for testing only):
- Administrator: username `admin`
- Bank Officers: username `officer1`, `officer2`
- Customers: username `customer1`, `customer2`, `customer3`

⚠️ **Warning**: These credentials are for demonstration purposes only. In production, strong unique passwords should be used.

## Building

### Prerequisites

- CMake 3.24 or newer
- A C++17 compiler: GCC 9+ or Clang 10+ on Linux, or Visual Studio 2019+ (with "Desktop development with C++") on Windows
- Git, and internet access for the first build. CMake downloads cpp-httplib, nlohmann/json, SQLite and libsodium into `build/_deps`.

### Linux

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/bin/bms_server
```

### Windows (Developer PowerShell or a normal terminal with CMake on the PATH)

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Debug\bms_server.exe
```

Useful options:

- `-DCMAKE_BUILD_TYPE=Release` (single-config generators such as Ninja or Make) or `--config Release` (Visual Studio)
- `-DBUILD_TESTING=OFF` skips the tests
- `-DBMS_WARNINGS_AS_ERRORS=ON` fails the build on any compiler warning

## Running

```sh
./build/bin/bms_server                 # http://127.0.0.1:8080
./build/bin/bms_server --port 9000 --db data/bms.db
curl http://127.0.0.1:8080/health      # {"status":"ok","version":"0.1.0"}
```

| Option | Environment variable | Default |
|---|---|---|
| `--host <address>` | `BMS_HOST` | `127.0.0.1` (this machine only) |
| `--port <number>` | `BMS_PORT` | `8080` |
| `--db <path>` | `BMS_DB_PATH` | `bms.db` |
| `--frontend <dir>` | `BMS_FRONTEND_DIR` | the repository's `frontend/` folder |

Command-line options override environment variables. The server serves the front end at `/` and the API endpoints from SADS 3.2 beside it. Stop it with Ctrl+C.

### API conventions

- Requests and responses are JSON.
- Errors always look like this, with the matching HTTP status:

  ```json
  { "code": "VALIDATION_ERROR", "message": "Amount must be positive.", "field": "amount" }
  ```

  `field` appears only when one input field caused the error. Statuses: 400 for invalid input, 401 when not logged in, 403 when the role is not allowed, 404 when something is not found, 409 for a conflict, 422 for a broken business rule (for example `INSUFFICIENT_BALANCE`), and 500 for unexpected errors, which never include internal details.
- Core code throws `bms::Error` (`src/common/error.hpp`). The API layer turns it into the response above.
- Each module registers its endpoints in `src/<module>/routes.cpp`:

  ```cpp
  void register_routes(api::Router& router) {
      router.get("/accounts/:acc/balance", [](const api::Request& req) {
          return api::Response::ok({{"account", req.param("acc")}, {"balance", "0.00"}});
      });
  }
  ```
- The access log records the method, path and status only. Request bodies, query strings and headers are never logged.

## Project structure

```
src/
  common/        shared code: config, logging, errors, database helpers
  api/           HTTP server, router, JSON error format, access log
  auth/          login, sessions, lockout, role-based access control
  customer/      customer records
  account/       accounts and account types
  transaction/   deposits, withdrawals, transfers
  reporting/     balances, history, interest, statements
  staff/         Bank Officer management, dashboards
  audit/         append-only audit trail
  main.cpp       server entry point
frontend/        HTML, CSS and JavaScript served by the server
db/              schema.sql and seed.sql
tests/           unit tests (ctest)
docs/            SRS, SADS, Test Plan, decision records, deviations log
cmake/           dependency pins and build helpers
```

Each folder under `src/` builds a static library `bms_<name>` (alias `bms::<name>`). To add a source file, list it in that folder's `CMakeLists.txt`. Include headers relative to `src/`, for example `#include "common/version.hpp"`.

## Documentation

- [Software Requirements Specification](docs/SRS_Bank_Management_System.pdf)
- [Software Architecture and Design Specification](docs/Software_Architecture_and_Design_Specification.docx)
- [Test Plan](docs/Test_Plan_BankManagementSystem.pdf)
- [Architecture decision records](docs/decisions/)
- [Deviations log](docs/DEVIATIONS.md)
- [Contributing guide](CONTRIBUTING.md)
