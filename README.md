# Bank Management System

Web-based bank management system for the PES University Software Engineering mini-project (UE24CS341A). Business logic is in a C++ core, data is stored in SQLite, and the browser front end is plain HTML/CSS/JS.

## Tech stack

C++17, CMake, cpp-httplib, nlohmann/json, SQLite, libsodium (Argon2id), GoogleTest, and a plain HTML/CSS/JS front end. The choices and reasons are in [ADR 001: Technology stack](docs/decisions/001-tech-stack.md).

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

## Project structure

```
src/
  common/        shared code: config, version, errors, database helpers
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
