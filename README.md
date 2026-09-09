# Estimated Taxes

Local C++ and Nuxt application for estimating 2026 federal and California quarterly taxes. Product behavior and scope are documented in [`docs/architecture/`](docs/architecture/README.md).

## Prerequisites

- CMake 3.20 or newer
- A C++20 compiler
- Node.js 20.19 or newer
- pnpm 10

## Start both services

From the repository root, build the backend and start both the backend and Nuxt development server:

```sh
./start.sh
```

Press Ctrl-C to stop both processes. The script reads the backend port from `~/.fi-estaxes/settings.json` and configures the frontend proxy to match.

## Backend

Configure and build out of source:

```sh
cmake -S . -B build -G "Unix Makefiles"
cmake --build build
```

Run the loopback-only backend (defaults to `127.0.0.1:8080` and `~/.fi-estaxes/estimated-taxes.sqlite`):

```sh
./build/backend/estimated_taxes_backend
```

On its first run, the backend creates this local data layout:

```text
~/.fi-estaxes/
├── estimated-taxes.sqlite
├── settings.json
└── backups/
```

`settings.json` contains the persisted listen port and may be edited while the backend is stopped:

```json
{ "port": 8080 }
```

The port must be an integer from 1 through 65535. Invalid settings prevent startup. `--port` overrides the saved port for that run only; `--database` uses a different database without moving the local settings or backup archive:

```sh
./build/backend/estimated_taxes_backend --port 9080 --database /path/to/estimated-taxes.sqlite
```

Each downloaded SQLite backup is also archived as a timestamped `.sqlite` file in `~/.fi-estaxes/backups/`. Backup retention is managed outside the application.

Run its tests:

```sh
ctest --test-dir build --output-on-failure
```

## Frontend

Install dependencies and start the development server:

```sh
cd frontend
pnpm install
pnpm dev
```

Type-check and lint:

```sh
pnpm typecheck
pnpm lint
```

Build and preview the production output:

```sh
pnpm build
pnpm preview
```

The built Nitro server proxies same-origin `/api` requests to the backend at `http://127.0.0.1:8080` by default. To use a backend on another loopback port, set its runtime origin when starting the preview server:

```sh
NUXT_BACKEND_ORIGIN=http://127.0.0.1:9080 pnpm preview
```

The backend and frontend are independent projects. CMake does not install frontend dependencies or run the Nuxt build.
