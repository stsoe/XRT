# Windows container for building XRT with cl.exe (MSVC) and vcpkg.
# Build: docker build -t xrt-build:windows -m 2GB .
# Run (interactive): docker run -it -v C:\path\to\XRT:C:\XRT xrt-build:windows
# Then inside container: cd C:\XRT && .\build\build22.bat -opt
# Or use this image as the runner image for a self-hosted GitHub Actions Windows runner.
# escape=`

FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["cmd", "/S", "/C"]

RUN `
    curl -SL --output vs_buildtools.exe https://aka.ms/vs/17/release/vs_buildtools.exe `
    && (start /w vs_buildtools.exe --quiet --wait --norestart --nocache `
        --installPath "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" `
        --add Microsoft.VisualStudio.Workload.VCTools `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.10240 `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.10586 `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.14393 `
        --remove Microsoft.VisualStudio.Component.Windows81SDK `
        || IF "%ERRORLEVEL%"=="3010" EXIT 0) `
    && del /q vs_buildtools.exe

# Git and vcpkg: install when running the container, or add further RUN steps here.
# The GitHub Actions workflow uses the host's Git and clones vcpkg in the job.

# Developer command prompt + PowerShell (cl.exe in PATH)
ENTRYPOINT ["C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat", "-arch=amd64", "&&", "powershell.exe", "-NoLogo", "-ExecutionPolicy", "Bypass"]
