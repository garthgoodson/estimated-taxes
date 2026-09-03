# Estimated Taxes

Local C++ and Nuxt application for estimating 2026 federal and California quarterly taxes. Product behavior and scope are documented in [`docs/architecture/`](docs/architecture/README.md).

## Prerequisites

- CMake 3.20 or newer
- A C++20 compiler
- Node.js 20.19 or newer
- pnpm 10

## Backend

Configure and build out of source:

```sh
cmake -S . -B build -G "Unix Makefiles"
cmake --build build
```

Run the loopback-only backend (defaults to `127.0.0.1:8080` and `estimated-taxes.sqlite`):

```sh
./build/backend/estimated_taxes_backend
```

Override the port or database location when needed:

```sh
./build/backend/estimated_taxes_backend --port 9080 --database /path/to/estimated-taxes.sqlite
```

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

The backend and frontend are independent projects. CMake does not install frontend dependencies or run the Nuxt build.
