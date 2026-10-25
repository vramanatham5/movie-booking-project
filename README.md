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
