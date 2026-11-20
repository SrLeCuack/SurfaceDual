# Surface Dual

**English** | [Español](README.es.md)

**Detach the keyboard and your Surface becomes an Android tablet. Attach it and you're back in Windows.**

Surface Dual runs Android 17 with Google Play in a Hyper-V virtual machine on Windows 11 devices with a Snapdragon X chip (ARM64 only).
Android really uses the device's GPU and video hardware, and while paused its processors are stopped (0 % CPU, measured).
It is an alternative to Windows Subsystem for Android (WSA), which Microsoft discontinued, for Snapdragon X (Windows on ARM) devices.

> **Experimental project.** Tested only on a Surface Pro 11 (Snapdragon X Elite), and the installer has not yet been tested
> end to end on another device (its self-tests pass). Use at your own risk. The installer creates a Windows restore point
> first and includes an uninstaller that undoes what it did.

## What it does

- **Keyboard off = Android, keyboard on = Windows.** When you go back to Windows, Android is paused.
  Measured: resume Android 0.1 s, pause it 4.9 s, cold boot 33-36 s.
- **GPU-accelerated graphics** (the Snapdragon's Adreno, shared with Windows) and **hardware video decoding** inside Android.
- **Google Play**, with your Google account and regular apps.
- **Reading mode** for manga, manhwa and books: a quick-settings button that drops the display to 48 Hz while the page is still,
  turns off animations and puts unneeded work to sleep. It never uses grayscale.
- **Sound and microphone, cameras, screen rotation and sensors, Windows folders inside Android, and Chromecast.**
- **Tablet interface:** floating cards for notifications and quick settings, an app side panel, floating windows, split screen
  and Circle to Search.
- **Trackpad gestures with the keyboard attached:** three fingers down shows the Windows desktop and Android audio keeps
  playing; three fingers sideways switches between Android and Windows.
- **Watched boot:** if Android hangs while starting, it restarts by itself.
- **Production build:** Android is not debuggable; only your own PC can connect to Android, with its key.

## Battery (measured on a Surface Pro 11, whole device)

| Task (30 % brightness) | Android (Surface Dual) | Windows |
|---|---|---|
| Reading manga, one page every 10 s (unplugged, 10 min) | **2.37 W** (Mihon with Reading mode) | 3.83 W (Edge) |
| Watching YouTube full screen (on charger) | 11.28 W | 11.31 W (Edge) |

In this reading test, Android used **38 % less** power than Edge on Windows. YouTube uses the same on both.

## Requirements

- An **ARM64 device with a Qualcomm Snapdragon X** chip (Adreno GPU) running **Windows 11**. Only tested on a Surface Pro 11.
  **It does not work on Intel or AMD (x86/x64) PCs:** the installer checks this first and stops without changing anything.
- **16 GB of RAM** (Android takes 8 GB while it is on) and **64 GB of free disk space**.
- A Windows **administrator** account and an Internet connection: about 2.2 GB of Android is downloaded, plus Ubuntu and tools.
- Hyper-V. On Windows 11 **Home** the installer enables it **experimentally**, and only if you type `SI` to confirm.

## Installation

1. Download the `.zip` of the latest version from [Releases](../../releases) and extract it.
2. Double-click `instalador\Instalar.cmd` and choose the language: `2` = English (`1` = Spanish). It checks your device first
   and asks before changing anything.
3. If every check says "ok", type `YES` to install and accept the Windows prompt (UAC).
4. Wait. Building Android is the longest step: close to an hour (an estimate; it depends on your Internet). At the end, restart Windows once.
5. Detach the keyboard (or open "Android (Surface Dual)" from the Start menu), open Play Store and sign in with your Google account.

**Already installed? Updating is just as easy:** download the new version, extract it and double-click `instalador\Instalar.cmd`
(or `instalador\Actualizar.cmd`). It notices Surface Dual is already installed and only replaces the programs and the
Android home screen. Your Android apps and data stay as they are, and nothing is downloaded or rebuilt.

The installer, Android and Surface Dual's own programs (tray icon, viewer, home screen, quick settings) all use the
language you choose. It installs to `C:\SurfaceDual`. If it stops, open `Instalar.cmd` again and it resumes where
it left off.

**Uninstall:** double-click `instalador\Desinstalar.cmd`. It undoes what the installer did, using the Windows state it recorded before installing.

## If something fails

Double-click `instalador\Diagnostico.cmd`. It only reads (changes nothing) and creates a `.zip` on your Desktop with what's
needed to find the problem: Windows version, RAM, graphics driver, blue-screen records and Surface Dual's logs. It removes
your user name, PC name, serial number, Wi-Fi names, emails and IP addresses. If there was a blue screen, it asks before
including the crash file; send that one privately. Open a GitHub Issue (or send a direct message) with the `.zip`.

## Known limitations

- Settings > About shows the model as `sdk_gphone64_arm64`: it is Google's official emulator image.
- YouTube goes up to 1080p60. Netflix does not work ([why](#why-netflix-does-not-work)).
- Android alarms do not ring while you are in Windows, because Android is paused.
- With the trackpad gestures, Android is not paused until you go back to it.

### Why Netflix does not work

Netflix opens and closes by itself a few seconds later. We traced it: when it starts, the Netflix app creates a security key
and checks who certified it. On a certified phone or tablet that key comes from a security chip approved by Google; in
Surface Dual it is created by software inside the virtual machine, so Netflix decides on the device itself to close.
Google's own check (Play Integrity) also fails, not even at its basic level: since Android 13 that level also requires
keys certified by Google.

The only ways around it would be faking another device's identity or using leaked keys. This project does not do that and
never will. **Use Netflix on Windows** (attach the keyboard), where it works normally. Other apps that require a
Google-certified device (some banking apps, for example) may not work either.

## How it works (short version)

Android runs inside a Windows virtual machine (Hyper-V), separate from your files. Under the hood: Hyper-V runs Google's official Android image with a custom Linux kernel (based on Microsoft's WSL kernel).
The GPU is shared through GPU-P and Mesa (D3D12). The Windows Viewer shows the screen and sends touch, pen and keyboard input;
small helpers connect sound, camera, sensors and folders over vsock. The Android image is downloaded from Google on your own
device, with your consent: this repository contains nothing from Google.

## Support the project

If you find it useful, you can support it with this repository's **Sponsor** button. Thank you!

## License and notices

Surface Dual's own code is under the [MIT license](LICENSE). Third-party components keep their own licenses; see
[instalador/LICENCIAS.md](instalador/LICENCIAS.md) (the Linux kernel is GPL-2.0 and its source is in `RutaA/fase4/kernel_*`).

Independent project, not affiliated with Microsoft, Google or Qualcomm. Surface and Windows are trademarks of Microsoft;
Android and Google Play are trademarks of Google; Qualcomm, Snapdragon and Adreno are trademarks of Qualcomm Technologies, Inc.

The Android image with Google Play is licensed by Google under the Android SDK License Agreement
(https://developer.android.com/studio/terms), which you accept when installing; complying with it is your responsibility.
