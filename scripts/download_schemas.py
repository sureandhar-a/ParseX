#!/usr/bin/env python3
"""Download user-supplied AUTOSAR XSD schemas onto this machine.

AUTOSAR schemas are copyrighted: the repo tracks the layout and this
script, never the .xsd files themselves (see resources/schemas/README.md).
Each user runs this script locally; it fetches the public
AUTOSAR_MMOD_XMLSchema.zip archives from autosar.org, extracts the needed
files, verifies size + SHA-256, and installs them atomically (temp file +
os.replace) as resources/schemas/<release>.xsd.

Stdlib only. Idempotent: up-to-date files are skipped unless --force.

Usage:
    python3 scripts/download_schemas.py [--dest DIR] [--releases 4.2.2 R21-11 ...]
                                        [--force] [--list]
"""

import argparse
import hashlib
import pathlib
import shutil
import ssl
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_DEST = REPO_ROOT / "resources" / "schemas"

# (release, archive URL, member inside the archive, installed name,
#  expected size in bytes, expected SHA-256 of the installed file)
SCHEMAS = [
    (
        "4.2.2",
        "https://www.autosar.org/fileadmin/standards/R4.2.2/CP/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_4-2-2.xsd",
        "4.2.2.xsd",
        4949346,
        "228bdc5e622c7cb545f29c317ed8dda97cd0a1233672d52d9085f04ba0432019",
    ),
    (
        "4.3.0",
        "https://www.autosar.org/fileadmin/standards/R4.3.0/CP/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_4-3-0.xsd",
        "4.3.0.xsd",
        5743387,
        "b8d7c1912fe4f1bd2a14879f9587c46537193e3772ec933e74605b60667c433c",
    ),
    (
        "4.3.1",
        "https://www.autosar.org/fileadmin/standards/R4.3.1/CP/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_00044.xsd",
        "4.3.1.xsd",
        6632708,
        "2e303e3d24cf0b7fdd9952070895580c1219d289517ee415520b9a60db96030f",
    ),
    (
        "4.4.0",
        "https://www.autosar.org/fileadmin/standards/R18-10_R4.4.0_R1.5.0/CP/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_00046.xsd",
        "4.4.0.xsd",
        7178542,
        "b20df1833a9d3f98e621b56afb5596bc4241fc2133136cf9940ad58a1ad1fd51",
    ),
    (
        "R19-11",
        "https://www.autosar.org/fileadmin/standards/R19-11/CP/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_00048.xsd",
        "R19-11.xsd",
        7891756,
        "242377ce9e8b8303d317dd3ddf4368bad5c47597ff371e8f9f9dd8894289d083",
    ),
    (
        "R20-11",
        "https://www.autosar.org/fileadmin/standards/R20-11/FO/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_00049.xsd",
        "R20-11.xsd",
        8577532,
        "a7a04b0a588552874ee046f097ca6515ae43ed8dd50867f2587735946d4fe689",
    ),
    (
        "R21-11",
        "https://www.autosar.org/fileadmin/standards/R21-11/FO/AUTOSAR_MMOD_XMLSchema.zip",
        "AUTOSAR_00050.xsd",
        "R21-11.xsd",
        9066074,
        "9f675c7f87feeea8c56ea2eba095d30bd59e3a8cc0011908a1c6d15d212305c5",
    ),
]

# The W3C-licensed sibling import every schema resolves relatively.
# Byte-identical across the archives that ship it; taken from R21-11's.
XML_XSD = (
    "https://www.autosar.org/fileadmin/standards/R21-11/FO/AUTOSAR_MMOD_XMLSchema.zip",
    "xml.xsd",
    "xml.xsd",
    4762,
    "70a3c67959f9ad2333016328716101bf8b476e682f29830cc9ee8da63dca7cc2",
)

CHUNK = 1 << 20

# autosar.org serves the leaf certificate only (no intermediates), which
# macOS SecureTransport tolerates but strict OpenSSL verification rejects.
# The leaf points at its issuer via AIA; fetching that (signed) intermediate
# and retrying keeps verification fully enforced — never disabled.
ISSUER_AIA_URL = "http://certificates.starfieldtech.com/repository/sfig2.crt"


def sha256_of(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def already_current(path: pathlib.Path, size: int, digest: str) -> bool:
    if not path.is_file():
        return False
    try:
        if path.stat().st_size != size:
            return False
        return sha256_of(path.read_bytes()) == digest
    except OSError:
        return False


def download(url: str, workdir: pathlib.Path) -> pathlib.Path:
    dest = workdir / "archive.zip"
    request = urllib.request.Request(url, headers={"User-Agent": "ParseX-schema-setup/1.0"})
    try:
        fetch_url(request, dest, ssl.create_default_context())
        return dest
    except urllib.error.URLError as exc:
        if not isinstance(exc.reason, ssl.SSLCertVerificationError):
            raise RuntimeError(f"download failed for {url}: {exc}") from exc
        # Server omits intermediates: complete the chain via the issuer's
        # AIA-published certificate and retry, verification still enforced.
        print("note: completing TLS chain via issuer AIA, retrying", flush=True)
    try:
        context = ssl.create_default_context()
        context.load_verify_locations(cadata=fetch_bytes(ISSUER_AIA_URL))
        fetch_url(request, dest, context)
    except Exception as exc:
        raise RuntimeError(f"download failed for {url}: {exc}") from exc
    return dest


def fetch_bytes(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "ParseX-schema-setup/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def fetch_url(request: urllib.request.Request, dest: pathlib.Path, context: ssl.SSLContext) -> None:
    with urllib.request.urlopen(request, timeout=120, context=context) as response:
        with open(dest, "wb") as out:
            shutil.copyfileobj(response, out, CHUNK)


def extract_member(archive: pathlib.Path, member: str) -> bytes:
    try:
        with zipfile.ZipFile(archive) as bundle:
            return bundle.read(member)
    except (zipfile.BadZipFile, KeyError) as exc:
        raise RuntimeError(f"cannot extract {member} from {archive.name}: {exc}") from exc


def install_bytes(dest: pathlib.Path, data: bytes, size: int, digest: str, label: str) -> str:
    if len(data) != size or sha256_of(data) != digest:
        raise RuntimeError(
            f"integrity check failed for {label}: got {len(data)} bytes "
            f"sha256={sha256_of(data)[:16]}..., refusing to install"
        )
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.parent / (dest.name + ".download.tmp")
    tmp.write_bytes(data)
    tmp.replace(dest)  # atomic on the same filesystem
    return "installed"


def fetch_one(url: str, member: str, dest: pathlib.Path, size: int, digest: str, force: bool) -> str:
    label = f"{dest.name} ({member})"
    if not force and already_current(dest, size, digest):
        return f"up-to-date, skipped: {label}"
    with tempfile.TemporaryDirectory(prefix="parsex_schemas_") as tmpdir:
        archive = download(url, pathlib.Path(tmpdir))
        data = extract_member(archive, member)
        install_bytes(dest, data, size, digest, label)
    return f"ok: {label} ({len(data)} bytes, sha256 verified)"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", default=str(DEFAULT_DEST), help="schema directory to populate")
    parser.add_argument("--releases", nargs="*", help="subset of releases (default: all)")
    parser.add_argument("--force", action="store_true", help="re-download even if current")
    parser.add_argument("--list", action="store_true", help="list known releases and exit")
    args = parser.parse_args(argv)

    if args.list:
        for release, url, member, _, _, _ in SCHEMAS:
            print(f"{release}: {member} <- {url}")
        return 0

    wanted = set(args.releases) if args.releases else None
    dest = pathlib.Path(args.dest)
    failures = 0
    fetched_any = False
    for release, url, member, name, size, digest in SCHEMAS:
        if wanted is not None and release not in wanted:
            continue
        fetched_any = True
        try:
            print(fetch_one(url, member, dest / name, size, digest, args.force), flush=True)
        except RuntimeError as exc:
            print(f"FAILED: {exc}", file=sys.stderr, flush=True)
            failures += 1
    # xml.xsd is the sibling import every schema resolves relatively — always
    # stage it alongside whenever any schema is fetched.
    if fetched_any:
        url, member, name, size, digest = XML_XSD
        try:
            print(fetch_one(url, member, dest / name, size, digest, args.force), flush=True)
        except RuntimeError as exc:
            print(f"FAILED: {exc}", file=sys.stderr, flush=True)
            failures += 1
    if failures:
        print(f"{failures} file(s) failed; see errors above", file=sys.stderr)
        return 1
    print("All requested schemas are in place.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
