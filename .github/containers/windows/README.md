# Windows container for building XRT

This directory contains a Dockerfile for building XRT on Windows inside a Docker container using **cl.exe** (MSVC) and **vcpkg** for dependencies.

## Prerequisites

- Windows host with Docker configured for Windows containers
- Sufficient memory and disk (see [Microsoft's guide](https://learn.microsoft.com/en-us/visualstudio/install/build-tools-container))

## Building the image

From this directory (or repo root):

```powershell
docker build -t xrt-build:windows -m 2GB .github/containers/windows
```

Use `-m 2GB` or more; the default 1 GB is often insufficient for the Build Tools install.

## Running a build inside the container

1. Clone XRT and submodules on the host.
2. Run the container with the repo mounted and use the vcpkg workflow (same as the GitHub Action):

```powershell
docker run -it -v C:\path\to\XRT:C:\XRT xrt-build:windows
```

Inside the container, the environment has `cl.exe` in PATH. Install or bring Git, CMake, and vcpkg (e.g. clone vcpkg and bootstrap it), then run the same configure/build steps as in `.github/workflows/build-windows.yml`: use the repo’s `src/vcpkg.json`, pass the vcpkg toolchain file to CMake, and set `KHRONOS` to the vcpkg installed directory (e.g. `vcpkg\installed\x64-windows`).

## Using with GitHub Actions

GitHub-hosted Windows runners do **not** support running the job inside a container. To run the Windows build in this container in CI:

- Use a **self-hosted** Windows runner that has Docker (Windows containers) installed.
- Build this image on that runner (or pull it from your registry).
- In your workflow, run the build by executing the same CMake/vcpkg steps inside this container (e.g. with `docker run` and the appropriate volume and command).

The main Windows workflow (`.github/workflows/build-windows.yml`) runs on `windows-latest` without a container so it works on GitHub-hosted runners.
