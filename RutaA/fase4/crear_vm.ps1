# Fase 4: la VM de la fase 2 arrancando nuestro AOSP 17: disco 0 = arranque.vhdx (GRUB+kernel+ramdisk), disco 1 = sistema.vhdx (super).
# Reversible: Remove-VM -Name SurfaceDualAndroid -Force (el VHDX es nuestro). Requiere el grupo "Administradores de Hyper-V".
# -Comandos: se escriben en la consola serie (shell de Android en ttyAMA0) a los -Tras segundos.
param([int]$Segundos = 60, [string]$Comandos = '', [int]$Tras = 120)
$ErrorActionPreference = 'Stop'
$n = 'SurfaceDualAndroid'; $vhd = Join-Path $PSScriptRoot 'arranque.vhdx'; $tubo = 'surfacedual_android'
if (Get-VM -Name $n -ErrorAction SilentlyContinue) { Stop-VM -Name $n -TurnOff -Force -ErrorAction SilentlyContinue; Remove-VM -Name $n -Force }
New-VM -Name $n -Generation 2 -MemoryStartupBytes 8GB -VHDPath $vhd | Out-Null
# 8 GB fijos (17/11, antes 12: uso medido 2,9-3,8 GB; decisión del coordinador por delegación del usuario). Memoria dinámica medida: con GPU-PV Hyper-V exige inicio = mínimo y nunca
# añadió memoria (asignada fija con la "demanda" del invitado en ~190 GB): RutaA/resultados.md.
Add-VMHardDiskDrive -VMName $n -Path (Join-Path $PSScriptRoot 'android.vhdx')
# Disco de /data sin caché de escritura visible para Android: con caché (Default), cada fsync era un vaciado completo de la
# caché del NVMe del host (~0,5 ms; 4 KiB con sync: ~850 IOPS). Sin ella, Hyper-V escribe cada bloque directo al disco (como
# WRITE_THROUGH en Windows) y Android no manda vaciados: 6 400-8 600 IOPS, misma garantía de fsync (medido 17/11).
# Antes: WriteHardeningMethod Default. Revertir: -OverrideCacheAttributes Default.
Set-VMHardDiskDrive -VMName $n -ControllerType SCSI -ControllerNumber 0 -ControllerLocation 1 -OverrideCacheAttributes WriteCacheDisabled
# Red: NAT de Windows ("Default Switch"); Android lo ve como eth0 (hv_netvsc).
Connect-VMNetworkAdapter -VMName $n -SwitchName 'Default Switch'
Set-VM -Name $n -StaticMemory -ProcessorCount 8 -AutomaticCheckpointsEnabled $false -CheckpointType Disabled `
  -AutomaticStartAction Nothing -AutomaticStopAction ShutDown   # no se enciende con Windows (RAM libre hasta que se pida); al apagar Windows, apagado ordenado (con GPU-P no hay Save)
# GPU-PV necesita espacio MMIO para dxgkrnl (sin esto: "dxgglobal_getiospace: Unable to allocate mmio memory").
Set-VM -Name $n -GuestControlledCacheTypes $true -LowMemoryMappedIoSpace 1GB -HighMemoryMappedIoSpace 32GB
Set-VMFirmware -VMName $n -EnableSecureBoot Off
Set-VMComPort -VMName $n -Number 1 -Path "\\.\pipe\$tubo"
# Ligar la partición a la Adreno (hay dos rutas particionables; la PCI VEN_QCOM es la GPU) y darle recursos (valores
# estándar de GPU-PV). Sin InstancePath la partición quedaba sin GPU y DXCore no veía adaptadores.
$gpu = (Get-VMHostPartitionableGpu | Where-Object Name -match 'VEN_QCOM' | Select-Object -First 1).Name
if (-not $gpu) { $gpu = (Get-VMHostPartitionableGpu | Select-Object -First 1).Name }
Add-VMGpuPartitionAdapter -VMName $n -InstancePath $gpu
Set-VMGpuPartitionAdapter -VMName $n -MinPartitionVRAM 80000000 -MaxPartitionVRAM 100000000 -OptimalPartitionVRAM 100000000 `
  -MinPartitionEncode 80000000 -MaxPartitionEncode 100000000 -OptimalPartitionEncode 100000000 `
  -MinPartitionDecode 80000000 -MaxPartitionDecode 100000000 -OptimalPartitionDecode 100000000 `
  -MinPartitionCompute 80000000 -MaxPartitionCompute 100000000 -OptimalPartitionCompute 100000000
Get-VMGpuPartitionAdapter -VMName $n | Select-Object -First 1 InstancePath | Format-List | Out-String | Write-Host
Start-VM -Name $n
$c = New-Object System.IO.Pipes.NamedPipeClientStream('.', $tubo, [System.IO.Pipes.PipeDirection]::InOut)
$c.Connect(15000); $r = New-Object System.IO.StreamReader($c)
$enviar = (Get-Date).AddSeconds($Tras)
$fin = (Get-Date).AddSeconds($Segundos)
$t = $null
while ((Get-Date) -lt $fin -and $c.IsConnected) {
  if ($Comandos -and (Get-Date) -gt $enviar) { $b = [Text.Encoding]::ASCII.GetBytes("`n$Comandos`n"); $c.Write($b, 0, $b.Length); $c.Flush(); $Comandos = '' }
  if (-not $t) { $t = $r.ReadLineAsync() }   # una lectura pendiente a la vez
  if (-not $t.Wait(2000)) { if ((Get-VM -Name $n).State -ne 'Running') { break }; continue }
  $linea = $t.Result; $t = $null
  if ($null -eq $linea) { break }; $linea
}
$c.Dispose(); Start-Sleep 3; "estado VM: $((Get-VM -Name $n).State)"
