@echo off
rem LH4051-DE Updater launcher. Usage: lh4051-de-updater.cmd [-Mode stable|beta|dev] [-Check] [-Yes]
rem Works on Windows 7 and newer (needs PowerShell 3.0+; Windows 7 needs WMF 5.1).
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0lh4051-de-updater.ps1" %*
if not defined LH4051_NOPAUSE pause
