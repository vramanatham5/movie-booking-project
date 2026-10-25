# CineBook

A responsive movie discovery and seat-booking website with a self-contained C++17 backend.

## Run it

Build with CMake and a C++17 compiler:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\cinebook-server.exe
```

For single-configuration generators, run `.\build\cinebook-server.exe` instead. The server listens on `http://127.0.0.1:8080`; open that URL in your browser. An optional first argument selects a different port.

The backend serves the website and provides `GET /api/movies` and `POST /api/bookings`. Bookings are kept in memory and are cleared when the server stops. This is a demonstration project: it does not process payments or persist customer information.

## GitHub Pages preview

In the repository settings, select **Settings → Pages → Build and deployment → Source → GitHub Actions**. After this one-time setup, pushes to `main` deploy the static website from `web/` to [CineBook on GitHub Pages](https://vramanatham5.github.io/movie-booking-project/). The Pages version runs in preview mode: movie listings use the built-in sample catalog, and bookings are simulated for the current browser session. It does not connect to the C++ backend, reserve real seats, or process payments. Run the C++ server for the backend-connected version.
