# -*- coding: utf-8 -*-
import subprocess
subprocess.run(["powershell","-NoProfile","-Command",
  "Get-Process -Name 'bochs*' -ErrorAction SilentlyContinue | Stop-Process -Force"],
  capture_output=True, text=True, timeout=30)
print("bochs stopped")
