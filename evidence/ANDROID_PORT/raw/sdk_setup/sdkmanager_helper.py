import subprocess
import sys

SDK = r"C:\S4EP1Recomp\android\perf-sdk"
SDKMANAGER = SDK + r"\cmdline-tools\latest\bin\sdkmanager.bat"

args = sys.argv[1:]
if args and args[0] == "licenses":
    proc = subprocess.run(
        [SDKMANAGER, f"--sdk_root={SDK}", "--licenses"],
        input=("y\n" * 100),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
else:
    proc = subprocess.run(
        [SDKMANAGER, f"--sdk_root={SDK}", *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
sys.stdout.write(proc.stdout)
sys.exit(proc.returncode)
