import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "android"))
from check_bundle import ANDROID_TARGETS, require_16k_load_alignment

library = Path(sys.argv[1])
ndk = Path(sys.argv[2]) / "toolchains/llvm/prebuilt/linux-x86_64"
arch = sys.argv[3]
_, triple, machine = ANDROID_TARGETS[arch]
elf = subprocess.check_output(
    [ndk / "bin/llvm-readelf", "--file-header", "--program-headers", "--dynamic", "--dyn-syms", "--wide", library],
    text=True,
)
if not re.search(rf"Machine:\s+{re.escape(machine)}$", elf, re.MULTILINE) or not re.search(r"Type:\s+DYN\b", elf):
    raise SystemExit(f"{library.name} must be an Android {arch} shared object")
if not re.search(r"\bGLOBAL\s+DEFAULT\s+\d+\s+mediapipe_library_init$", elf, re.MULTILINE):
    raise SystemExit(f"{library.name} does not export mediapipe_library_init")
needed = set(re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", elf))
system_libs = {path.name for path in (ndk / f"sysroot/usr/lib/{triple}/26").glob("lib*.so")}
unresolved = needed - system_libs
if unresolved:
    raise SystemExit(f"{library.name} has unstaged dependencies {sorted(unresolved)}")
alignment = require_16k_load_alignment(library.name, elf)
print(f"{library.name} verified with {alignment // 1024} KiB ELF load alignment and dependencies {sorted(needed)}")
