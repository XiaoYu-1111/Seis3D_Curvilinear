@echo off
rem Use identical output encoding for the dependency probe and C++ compilation.
chcp 65001 >nul
%*
exit /b %errorlevel%
