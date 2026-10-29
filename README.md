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

For an existing version 1 database, apply both migrations in order:

```powershell
sqlite3 bms.db ".read db/migrations/002_customer_credentials.sql"
sqlite3 bms.db ".read db/migrations/003_customer_status.sql"
```

For a version 2 database, apply only the customer-status migration:

```powershell
sqlite3 bms.db ".read db/migrations/003_customer_status.sql"
```

The customer deactivation UI's manual verification steps are in [S-13 manual test](docs/S-13-MANUAL-TEST.md).

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
| `--tls-cert <file>` | `BMS_TLS_CERT` | none (plain HTTP) |
| `--tls-key <file>` | `BMS_TLS_KEY` | none (plain HTTP) |

Command-line options override environment variables. The server serves the front end at `/` and the API endpoints from SADS 3.2 beside it. Stop it with Ctrl+C.

### HTTPS and secrets

Plain HTTP is for local development only. To serve HTTPS, give the server a PEM certificate and private key (the build includes TLS when OpenSSL 3 is found; configure with `-DBMS_ENABLE_TLS=OFF` to leave it out). For a local test certificate:

```sh
openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj "/CN=localhost" \
    -keyout dev.key -out dev.crt
BMS_TLS_CERT=dev.crt BMS_TLS_KEY=dev.key ./build/bin/bms_server    # https://127.0.0.1:8080
```

Sign-in, password change and one-time credential delivery are allowed on a loopback address or over HTTPS, and refused on a network address served over plain HTTP.

- Certificates, keys, `.env` files and local databases are listed in `.gitignore`, and CI runs `scripts/check-secrets.sh` to fail the build if any is committed. Configuration comes from the environment or command-line options only; nothing secret lives in the repository. The demo password above is for the seeded demo users.
- Passwords, one-time initial passwords and bearer tokens are never logged; `tests/log_secrets_test.cpp` drives sign-in, registration, staff creation and password change and checks the log.

### API conventions

- Requests and responses are JSON.
- Errors always look like this, with the matching HTTP status:

  ```json
  { "code": "VALIDATION_ERROR", "message": "Amount must be positive.", "field": "amount" }
  ```

  `field` appears for a single-field error; multi-field validation responses use a `fields` array of `{ "field", "message" }` entries. Statuses: 400 for invalid input, 401 when not logged in, 403 when the role is not allowed, 404 when something is not found, 409 for a conflict, 422 for a broken business rule (for example `INSUFFICIENT_BALANCE`), and 500 for unexpected errors, which never include internal details.
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

### Customer registration and sign-in

Administrators and Bank Officers sign in with `POST /auth/login` using `{"username":"…","password":"…"}`. The response contains a short-lived bearer token; send it as `Authorization: Bearer <token>` to `POST /customers`. Customer registration requires `name`, `dateOfBirth`, `address`, `phone`, `identificationReference` and `email`. A successful response returns the new customer ID and one-time temporary password; the customer must change that password after first sign-in using `POST /auth/change-password` with `{"newPassword":"…"}`. `POST /auth/logout` invalidates the bearer token.

The temporary password is shown only in the immediate registration response and success screen. The customer-registration and sign-in endpoints are enabled only when the server binds to a loopback address or serves HTTPS (see above); do not expose plain HTTP to a network. The one-time credential response is a documented exception to SADS SEC-2; see [the deviations log](docs/DEVIATIONS.md).

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
