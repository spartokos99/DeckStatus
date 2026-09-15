@echo off
"%~dp0DeckStatus.exe" --mode prolink %*
if errorlevel 1 pause
