#!/bin/bash
# Compila Bandeja.exe (ARM64, .NET Framework 4.8.1) con el Roslyn del SDK de .NET, como RutaA/fase4/host/compilar.sh.
# Uso: bash F2/bandeja/compilar.sh [salida=Bandeja.nuevo.exe]   (se instala en F2/build/emulator/Bandeja.exe)
cd "$(dirname "$0")"; F='C:\Windows\Microsoft.NET\FrameworkArm64\v4.0.30319'; W='C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.26100.0'
"/c/Program Files/dotnet/dotnet" "/c/Program Files/dotnet/sdk/10.0.404/Roslyn/bincore/csc.dll" \
  -nologo -platform:arm64 -optimize+ -target:winexe -noconfig -nostdlib \
  -r:"$F\mscorlib.dll" -r:"$F\System.dll" -r:"$F\System.Core.dll" -r:"$F\System.Drawing.dll" \
  -r:"$F\System.Windows.Forms.dll" -r:"$F\System.Management.dll" \
  -r:"$F\System.Runtime.dll" -r:"$F\System.Runtime.WindowsRuntime.dll" -r:"$W\Windows.winmd" \
  -out:"${1:-Bandeja.nuevo.exe}" Bandeja.cs Panel.cs
