#!/usr/bin/env python3
"""Pull the images out of a .unitypackage into a flat folder.

A .unitypackage is a gzipped tar where every asset lives in a GUID-named
directory: `asset` holds the bytes and `pathname` holds the path it would
have had inside a Unity project. This rebuilds those paths and keeps only
the images, so packs can be dropped straight into assets/raw/.

  unitypackage.py pack.unitypackage out/dir [--flat]
"""
import os, sys, tarfile

KEEP = (".png", ".jpg", ".jpeg", ".tga", ".psd")


def extract(pkg, outdir, flat=False):
    os.makedirs(outdir, exist_ok=True)
    written = skipped = 0

    with tarfile.open(pkg, "r:gz") as tar:
        members = tar.getmembers()
        # guid -> (pathname, asset member)
        entries = {}
        for m in members:
            parts = m.name.split("/")
            if len(parts) < 2:
                continue
            guid, kind = parts[0], parts[-1]
            slot = entries.setdefault(guid, {})
            if kind in ("pathname", "asset"):
                slot[kind] = m

        for guid, slot in entries.items():
            if "pathname" not in slot or "asset" not in slot:
                continue
            f = tar.extractfile(slot["pathname"])
            if f is None:
                continue
            path = f.read().decode("utf-8", "replace").splitlines()[0].strip()
            if not path.lower().endswith(KEEP):
                skipped += 1
                continue

            rel = os.path.basename(path) if flat else path
            # never let an archive path escape the output directory
            dest = os.path.normpath(os.path.join(outdir, rel))
            if not dest.startswith(os.path.abspath(outdir)) and not dest.startswith(outdir):
                print(f"  ! skipping suspicious path: {path}")
                continue
            os.makedirs(os.path.dirname(dest), exist_ok=True)

            data = tar.extractfile(slot["asset"])
            if data is None:
                continue
            with open(dest, "wb") as out:
                out.write(data.read())
            written += 1
            print(f"  + {rel}")

    print(f"{written} image(s) extracted, {skipped} non-image entr(ies) skipped")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    extract(sys.argv[1], sys.argv[2], "--flat" in sys.argv)
