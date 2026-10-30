"""Builds the sketch folder GitHub compiles: Runner.ino + ota.h + your code."""
import os
import pathlib
import re
import shutil

root = pathlib.Path(__file__).resolve().parent.parent
src = (root / "user_code.ino").read_text(encoding="utf-8", errors="replace")

# Rename setup()/loop() so the updater can wrap them (line count stays the same,
# so error line numbers still match the tutor screen).
src = re.sub(r"\bvoid\s+setup\s*\(\s*(void)?\s*\)", "void user_setup()", src)
src = re.sub(r"\bvoid\s+loop\s*\(\s*(void)?\s*\)", "void user_loop()", src)
if "user_setup" not in src:
    src += "\nvoid user_setup() {}\n"
if "user_loop" not in src:
    src += "\nvoid user_loop() {}\n"

build = root / "build" / "Runner"
shutil.rmtree(build.parent, ignore_errors=True)
build.mkdir(parents=True)
shutil.copy(root / "runner" / "Runner.ino", build / "Runner.ino")
shutil.copy(root / "runner" / "ota.h", build / "ota.h")
(build / "user_code.ino").write_text(src, encoding="utf-8")
sha = os.environ.get("GITHUB_SHA", "local-build")
(build / "build_info.h").write_text(f'#pragma once\n#define BUILD_VERSION "{sha}"\n', encoding="utf-8")
print("Sketch ready in", build)
