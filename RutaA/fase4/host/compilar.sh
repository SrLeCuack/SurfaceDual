#!/bin/bash
# Compila VisorVM.exe (ARM64, .NET Framework 4.8.1) con el Roslyn del SDK de .NET.
# Uso: bash RutaA/fase4/host/compilar.sh [salida=VisorVM.exe] [más fuentes u opciones]
cd "$(dirname "$0")"; F='C:\Windows\Microsoft.NET\FrameworkArm64\v4.0.30319'; W='C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.26100.0'
"/c/Program Files/dotnet/dotnet" "/c/Program Files/dotnet/sdk/10.0.404/Roslyn/bincore/csc.dll" \
  -nologo -platform:arm64 -optimize+ -unsafe -target:winexe -noconfig -nostdlib \
  -r:"$F\mscorlib.dll" -r:"$F\System.dll" -r:"$F\System.Core.dll" -r:"$F\System.Drawing.dll" \
  -r:"$F\System.Windows.Forms.dll" -r:"$F\System.Management.dll" \
  -r:"$F\System.Runtime.dll" -r:"$F\System.Runtime.WindowsRuntime.dll" -r:"$F\System.Runtime.InteropServices.WindowsRuntime.dll" \
  -r:"$W\Windows.winmd" -nowarn:CS0649 \
  -out:"${1:-VisorVM.exe}" VisorVM.cs Enlace.cs Avisos.cs "${@:2}"
# Audio (consola, sin VisorVM.cs que ya trae HvEndPoint): el mismo csc con -target:exe -main:HvAudio -out:HvAudio.exe HvAudio.cs HvPuente.cs
#   y solo -r: mscorlib, System y System.Core.
# Sensores: como la cámara con -main:HvSensores -out:HvSensores.exe HvSensores.cs HvPuente.cs.
# Cámara: igual que el audio con -unsafe -target:exe -main:HvCamara -out:HvCamara.exe HvCamara.cs HvPuente.cs, más
#   -r: System.Runtime, System.Runtime.WindowsRuntime, System.Runtime.InteropServices.WindowsRuntime y Windows.winmd.
