#!/usr/bin/env python3
"""Manifest-based cleanup of ignored build trees. DRY-RUN by default.

Ignored != safe to delete. .gitignore says what Git must not track; this
manifest says what may be reclaimed. Every candidate is listed by name (from
the CLEAN3A census); nothing is discovered by globbing.

    python native/tools/cleanup_builds.py                       # dry-run, everything
    python native/tools/cleanup_builds.py --group host-historical   # still dry-run
    python native/tools/cleanup_builds.py --check PATH          # classify one path
    python native/tools/cleanup_builds.py --group tdeck-historical --delete --yes-really

Steady state (after CLEAN3): only KEEP trees exist; the DELETE/BACKUP_REQUIRED entries
are kept so a stale or regenerated historical tree is recognised (missing paths
are normal).

Deleting needs BOTH --delete and --yes-really, and at least one explicit --group.
The backup-required group additionally needs a backup marker per entry:
    <backup-dir>/<dirname>.sha256       sha256sum-format lines "<hash> *<relpath>"
    <backup-dir>/<dirname>/<relpath>    the backed-up copy of each listed file
The marker must list every unique file of the entry (UNIQUE below); each copy
must hash to the listed value and, while the source tree still exists, to the
source file too. Default backup dir: C:\\Dev\\openu5-release-artifacts\\backups.
"""
import argparse
import hashlib
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path, PurePosixPath

REPO = Path(__file__).resolve().parents[2]
DEFAULT_BACKUP_DIR = Path(r"C:\Dev\openu5-release-artifacts\backups")
ALLOWED_UNTRACKED = {"native/core/a4-release-ctest.log"}

KEEP, BACKUP, DELETE = "KEEP", "BACKUP_REQUIRED", "DELETE"
CORE = "native/core/"
TDECK = "native/targets/tdeck/"

# Hard protection, independent of the manifest. A candidate is refused if it is,
# is inside, or contains (is an ancestor of) any of these.
PROTECTED = [
    "original", "game/assets", "native/assets", "native/core/fixtures",
    "native/core/a4-ui3-shots",
    "native/core/build-shops", "native/core/build-real-arenas",
    "native/core/build-dialogue", "native/core/build-quests",
    "native/core/build-gameplay", "native/core/build-persistence",
    "native/core/build-dungeon", "native/core/build-tools", "native/core/build-zig",
    "native/targets/tdeck/build-core", "native/targets/tdeck/build-a3-rc1-post",
    "native/targets/tdeck/build-a4-rc5",
]

# (relpath, class, group, reason)
MANIFEST = [
    (CORE + "build", KEEP, "canonical", "canonical ordinary host build (configure per native/core/README.md)"),
    (CORE + "build-quests", KEEP, "specialized", "parity workspace: check-quests.ts output read by a4_parity2_quest_diff.py; needs EA assets"),
    (CORE + "build-gameplay", KEEP, "specialized", "parity workspace: check-gameplay.ts output; needs EA assets"),
    (CORE + "build-shops", KEEP, "specialized", "CMake tests shop_flow/shop_parity read flow.txt/helpers.txt here"),
    (CORE + "build-real-arenas", KEEP, "specialized", "CMake combat tests read combat.txt/maps.txt/advanced-combat.txt here"),
    (CORE + "build-dialogue", KEEP, "specialized", "CMake test dialogue_parity reads dialogue.bin here"),
    (CORE + "build-persistence", KEEP, "specialized", "check-persistence.ts transport output"),
    (CORE + "build-dungeon", KEEP, "specialized", "specialized dungeon workspace (absent at CLEAN3A; protected if it appears)"),
    (CORE + "build-tools", KEEP, "toolchain", "portable Zig toolchain; re-bootstrap needs a download"),
    (CORE + "build-zig", KEEP, "toolchain", "Zig example/build referenced by check-*.ts and docs"),
    (CORE + "build-a4-close1", DELETE, "closeout", "superseded by the canonical native/core/build (201/201 validated; identical test names); no unique files"),
    (TDECK + "build-core", KEEP, "canonical", "canonical current T-Deck build (also holds older launcher .bin files)"),
    (TDECK + "build-a3-rc1-post", KEEP, "release", "Alpha 3 RC1 release tree (artifacts also preserved out of tree)"),
    (TDECK + "build-a4-rc5", KEEP, "release", "Alpha 4 RC5 release tree (artifacts also preserved out of tree)"),
    (CORE + "build-a4-enh1-baseline", BACKUP, "backup-required", "unique hand-written alpha4_ui_s10.md and edit_p*.py scripts"),
    (CORE + "build-a3-hf5", BACKUP, "backup-required", "unique hand-written section31.md and patch scripts"),
    (TDECK + "build-batch54", BACKUP, "backup-required", "unique Alpha 2 RC1 Launcher .bin (SHA-256 ff3dfe19...)"),
    ("native/core/tools/__pycache__", DELETE, "trivial-cache", "Python bytecode cache"),
    ("native/targets/tdeck/__pycache__", DELETE, "trivial-cache", "Python bytecode cache"),
    ("re/tools/__pycache__", DELETE, "trivial-cache", "Python bytecode cache"),
]

# Historical trees from the CLEAN3A census (regenerable from the matching commit).
HOST_HISTORICAL = """
    build-a3-01 build-a3-01-baseline build-a3-02 build-a3-02-baseline build-a3-03
    build-a3-03-baseline build-a3-04-baseline build-a3-04a build-a3-04a-base build-a3-04b
    build-a3-04b-base build-a3-04c build-a3-04c-base build-a3-04d build-a3-04d-base build-a3-04e
    build-a3-04e-base build-a3-04f build-a3-04f-base build-a3-04g build-a3-04g-base build-a3-05
    build-a3-05-base build-a3-hf10 build-a3-hf10-final build-a3-hf2 build-a3-hf2-1
    build-a3-hf2-1-base build-a3-hf2-base build-a3-hf3 build-a3-hf3-base build-a3-hf4
    build-a3-hf4-base build-a3-hf5-base build-a3-hf5-final build-a3-hf6 build-a3-hf6-base
    build-a3-hf6-final build-a3-hf7 build-a3-hf7-final build-a3-hf8 build-a3-hf8-base
    build-a3-hf8-final build-a3-hf9 build-a3-hf9-base build-a3-hf9-final build-a3-rc1 build-a4-end1
    build-a4-enh1 build-a4-enh2 build-a4-enh2-baseline build-a4-parity1 build-a4-parity1-commit
    build-a4-parity2 build-a4-parity2-baseline build-a4-parity2-final build-a4-polish3
    build-a4-save1 build-a4-save2 build-a4-save3 build-a4-ui1 build-a4-ui2 build-a4-ui3 build-a4-ui4
    build-a4-ui4-closeout build-a4-ui4-red build-batch48 build-batch50 build-batch51
    build-batch51-final build-batch52-baseline build-batch52-final build-batch53
    build-batch53-baseline build-batch53-final build-batch53a build-batch53a-final build-batch53b
    build-batch53b-baseline build-batch54-baseline build-batch54-rc build-batch55-final build-hf1
    build-hf1-baseline
""".split()
TDECK_HISTORICAL = """
    build build-a3-01 build-a3-02 build-a3-02-final build-a3-03 build-a3-03-final build-a3-04
    build-a3-04-post build-a3-04a build-a3-04a-post build-a3-04b build-a3-04b-post build-a3-04c
    build-a3-04c-post build-a3-04d build-a3-04d-post build-a3-04e build-a3-04e-post build-a3-04e1
    build-a3-04e1-post build-a3-04f build-a3-04f-post build-a3-04g build-a3-04g-post build-a3-05
    build-a3-05-post build-a3-hf1 build-a3-hf1-post build-a3-hf10 build-a3-hf10-post build-a3-hf2
    build-a3-hf2-1 build-a3-hf2-1-post build-a3-hf2-post build-a3-hf3 build-a3-hf3-post build-a3-hf4
    build-a3-hf4-post build-a3-hf5 build-a3-hf5-post build-a3-hf6 build-a3-hf6-post build-a3-hf7
    build-a3-hf7-post build-a3-hf8 build-a3-hf8-post build-a3-hf9 build-a3-hf9-post build-a3-rc1
    build-a4-end1 build-a4-end1-final build-a4-end1-postcommit build-a4-enh1 build-a4-enh1-final
    build-a4-enh2 build-a4-enh2-final build-a4-flash1 build-a4-parity1 build-a4-parity1-precommit
    build-a4-polish3 build-a4-rc1 build-a4-rc2 build-a4-rc3 build-a4-rc3-pre build-a4-rc4
    build-a4-save1 build-a4-save2 build-a4-save3 build-a4-save3-final build-a4-ui1 build-a4-ui1-pre
    build-a4-ui2 build-a4-ui3 build-a4-ui3-final build-a4-ui4 build-a4-ui4-final build-a4-ui4-hf1
    build-batch48 build-batch51 build-batch53 build-batch53a build-batch54-pre
""".split()
MANIFEST += [(CORE + n, DELETE, "host-historical", "historical batch/Alpha/hotfix host build; regenerate with cmake from the commit") for n in HOST_HISTORICAL]
MANIFEST += [(TDECK + n, DELETE, "tdeck-historical", "historical batch/Alpha/RC ESP-IDF build; regenerate with idf.py from the commit (includes the stale ESP-IDF default build/)") for n in TDECK_HISTORICAL]

UNIQUE = {
    CORE + "build-a4-enh1-baseline": ["alpha4_ui_s10.md", "edit_p1_tests.py", "edit_p2.py", "edit_p2_tests.py",
                                      "edit_p3.py", "edit_p4.py", "edit_p5.py", "edit_p5_tests.py", "edit_p6_docs.py"],
    CORE + "build-a3-hf5": ["section31.md", "docs_final.py", "docs_ledger_checklist.py", "fix_test1.py",
                            "fix_test2.py", "fix_test3.py", "wire_runtime.py"],
    TDECK + "build-batch54": ["launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin"],
}
UNIQUE_HASH_PREFIX = {"launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin": "ff3dfe19"}


class Refused(Exception):
    pass


def _is_reparse(st):
    return bool(getattr(st, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def _is_link(p, st):
    return stat.S_ISLNK(st.st_mode) or _is_reparse(st)


def _inside(child, parent):
    return child == parent or parent in child.parents


def validate_manifest():
    seen = set()
    for rel, cls, grp, _ in MANIFEST:
        assert rel not in seen, "duplicate manifest entry: " + rel
        seen.add(rel)
        if cls != KEEP:
            for p in PROTECTED:
                assert not _protected_hit(PurePosixPath(rel), p), "manifest entry is protected: " + rel
    return True


def _protected_hit(rel, prot):
    p = PurePosixPath(prot)
    return rel == p or p in rel.parents or rel in p.parents


def check_path(candidate, repo=None):
    """Return the validated absolute Path or raise Refused. Existence not required."""
    repo = Path(repo if repo is not None else REPO).resolve()
    raw = str(candidate)
    if ".." in PurePosixPath(raw.replace("\\", "/")).parts:
        raise Refused("path contains '..'")
    p = Path(raw)
    p = p if p.is_absolute() else repo / p
    parent = p.parent.resolve()
    full = parent / p.name
    if not _inside(full, repo):
        raise Refused("outside the repository")
    if full == repo:
        raise Refused("repository root")
    rel = PurePosixPath(full.relative_to(repo).as_posix())
    if rel.parts[0] == ".git":
        raise Refused(".git")
    for prot in PROTECTED:
        if _protected_hit(rel, prot):
            raise Refused("protected path (" + prot + ")")
    try:
        st = full.lstat()
    except FileNotFoundError:
        return full
    if _is_link(full, st):
        raise Refused("symlink/junction/reparse point")
    return full


def measure(path):
    """(bytes, files, links) without following symlinks."""
    total = files = 0
    links = []
    stack = [str(path)]
    while stack:
        d = stack.pop()
        try:
            it = list(os.scandir(d))
        except OSError:
            continue
        for e in it:
            try:
                st = e.stat(follow_symlinks=False)
            except OSError:
                continue
            if _is_link(e.path, st):
                links.append(e.path)
            elif stat.S_ISDIR(st.st_mode):
                stack.append(e.path)
            else:
                total += st.st_size
                files += 1
    return total, files, links


def fmt(n):
    return "%.1f MiB (%s bytes)" % (n / 1048576, format(n, ","))


def git_status(repo=None):
    r = subprocess.run(["git", "status", "--porcelain=v1", "--untracked-files=all", "-z"],
                       cwd=str(repo or REPO), capture_output=True, env=dict(os.environ, GIT_OPTIONAL_LOCKS="0"))
    if r.returncode != 0:
        raise Refused("git status failed")
    return [x.decode("utf-8", "replace") for x in r.stdout.split(b"\0") if x]


def check_worktree(lines, candidates):
    """lines: porcelain -z entries. Returns warnings; raises Refused on unsafe state."""
    warnings = []
    for l in lines:
        code, path = l[:2], l[3:]
        if code == "??":
            if path in ALLOWED_UNTRACKED:
                continue
            if any(path == c or path.startswith(c + "/") for c in candidates):
                raise Refused("untracked file inside a deletion candidate: " + path)
            warnings.append("untracked outside candidates: " + path)
        elif code != "!!":
            raise Refused("tracked file modified/staged: " + l)
    return warnings


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def check_backup(rel, backup_dir, repo=None):
    repo = Path(repo or REPO)
    name = PurePosixPath(rel).name
    marker = Path(backup_dir) / (name + ".sha256")
    if not marker.is_file():
        raise Refused("no backup marker: " + str(marker))
    listed = {}
    for line in marker.read_text(encoding="utf-8").splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) == 2:
            listed[parts[1].lstrip("*").replace("\\", "/")] = parts[0].lower()
    for need in UNIQUE[rel]:
        if need not in listed:
            raise Refused("marker lacks unique file " + need)
        prefix = UNIQUE_HASH_PREFIX.get(need)
        if prefix and not listed[need].startswith(prefix):
            raise Refused("marker hash for %s does not start with %s" % (need, prefix))
    for relfile, h in listed.items():
        copy = Path(backup_dir) / name / relfile
        if not copy.is_file() or sha256(copy) != h:
            raise Refused("backup copy missing or hash mismatch: " + relfile)
        src = repo / rel / relfile
        if src.is_file() and sha256(src) != h:
            raise Refused("source differs from backed-up copy: " + relfile)


def build_rows(groups):
    rows = []
    for rel, cls, grp, reason in MANIFEST:
        if groups and grp not in groups:
            continue
        err = None
        size = files = 0
        exists = False
        try:
            p = check_path(rel)
            exists = p.exists() or p.is_symlink()
            if exists:
                size, files, links = measure(p)
                if links and cls != KEEP:
                    err = "contains symlinks/junctions: " + links[0]
        except Refused as e:
            err = str(e)
            p = REPO / rel
            exists = p.exists()
            if exists and cls == KEEP:
                size, files, _ = measure(p)
        rows.append(dict(rel=rel, cls=cls, grp=grp, reason=reason, exists=exists, size=size, files=files, err=err))
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--group", action="append", help="host-historical, tdeck-historical, closeout, trivial-cache, backup-required (repeatable)")
    ap.add_argument("--check", metavar="PATH", help="classify one path and exit (never deletes)")
    ap.add_argument("--delete", action="store_true")
    ap.add_argument("--yes-really", action="store_true")
    ap.add_argument("--backup-dir", default=str(DEFAULT_BACKUP_DIR))
    a = ap.parse_args(argv)
    validate_manifest()

    if a.check:
        entry = next((m for m in MANIFEST if PurePosixPath(m[0]) == PurePosixPath(a.check.replace("\\", "/").rstrip("/"))), None)
        try:
            check_path(a.check)
            print("ALLOW  %s  (%s)" % (a.check, entry[1] if entry else "not in manifest: would never be selected"))
            return 0
        except Refused as e:
            print("REFUSE %s  (%s)" % (a.check, e))
            return 1

    known = {m[2] for m in MANIFEST}
    for g in a.group or []:
        if g not in known:
            ap.error("unknown group " + g)
    rows = build_rows(set(a.group) if a.group else None)

    tot = {KEEP: 0, BACKUP: 0, DELETE: 0}
    cnt = {KEEP: 0, BACKUP: 0, DELETE: 0}
    missing = 0
    for r in rows:
        status = "missing" if not r["exists"] else fmt(r["size"])
        print("%-16s %-17s %s\n    %s  [%s]%s" % (r["cls"], r["grp"], r["rel"], status, r["reason"], "  REFUSED: " + r["err"] if r["err"] and r["cls"] != KEEP else ""))
        if not r["exists"]:
            missing += 1
        else:
            tot[r["cls"]] += r["size"]
            cnt[r["cls"]] += 1
    print("\nSummary (%s)" % ("DRY-RUN" if not (a.delete and a.yes_really) else "DELETE"))
    for c in (KEEP, BACKUP, DELETE):
        print("  %-16s %3d present  %s" % (c, cnt[c], fmt(tot[c])))
    print("  missing paths    %3d" % missing)

    if not a.delete:
        print("\nDry-run only. Nothing was modified.")
        return 0
    if not a.yes_really:
        print("REFUSED: --delete also requires --yes-really")
        return 2
    if not a.group:
        print("REFUSED: deletion requires at least one explicit --group")
        return 2

    todo = [r for r in rows if r["cls"] in (DELETE, BACKUP) and r["exists"]]
    try:
        for r in todo:
            if r["err"]:
                raise Refused(r["rel"] + ": " + r["err"])
        warns = check_worktree(git_status(), [r["rel"] for r in todo])
        for r in todo:
            if r["cls"] == BACKUP:
                check_backup(r["rel"], a.backup_dir)
    except Refused as e:
        print("REFUSED:", e)
        return 2
    for w in warns:
        print("warning:", w)
    print("\nAbout to delete %d trees, %s:" % (len(todo), fmt(sum(r["size"] for r in todo))))
    for r in todo:
        print("   ", r["rel"])

    def onerr(func, path, exc):
        os.chmod(path, stat.S_IWRITE)
        func(path)

    for r in todo:
        try:
            p = check_path(r["rel"])
            print("deleting", r["rel"], flush=True)
            shutil.rmtree(p, onerror=onerr)
        except (Refused, OSError) as e:
            print("FAILED at %s: %s -- stopping." % (r["rel"], e))
            return 3
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
