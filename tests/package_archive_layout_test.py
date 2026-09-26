#!/usr/bin/env python3
"""Reject SDK entries outside the declared archive root before extraction."""

from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile


if not __debug__:
    raise SystemExit("Archive layout tests require Python assertions enabled")
source = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="package-layout-", dir=source / "build") as tmp:
    root = Path(tmp)
    stem = "c.pkt.systems-1.2.3-x86_64-linux-gnu"
    archive = root / (stem + ".tar.gz")
    (root / "c.pkt.systems-1.2.3-CHECKSUMS").write_text("")
    for entry in ["other-root/", "loose.txt", stem + "/../escape.txt"]:
        with tarfile.open(archive, "w:gz") as output:
            info = tarfile.TarInfo(stem + "/")
            info.type = tarfile.DIRTYPE
            output.addfile(info)
            output.addfile(tarfile.TarInfo(entry))
        result = subprocess.run([
            "bash", str(source / "scripts/run-package-assertions.sh"),
            "-DCPKT_ARCHIVE=" + str(archive),
            "-DCPKT_TARGET_ID=x86_64-linux-gnu", "-DCPKT_BUNDLE_VERSION=1.2.3"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        assert result.returncode != 0, result.stdout
        assert "package archive contains entry outside its root" in result.stdout, result.stdout
    archive = root / "c.pkt.systems-1.2.3.tar.gz"
    for entry in ["..", "../escape.txt", "c.pkt.systems-1.2.3/../escape.txt"]:
        with tarfile.open(archive, "w:gz") as output:
            output.addfile(tarfile.TarInfo(entry))
        result = subprocess.run([
            "bash", str(source / "scripts/source-archive-verify.sh"),
            str(archive), "1.2.3"], text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)
        assert result.returncode != 0, result.stdout
        assert "source archive contains unsafe entry" in result.stdout, result.stdout
print("[test] package archive layout rejection passed")
