"""Safety tests for cleanup_builds.py. They never delete anything."""
import contextlib
import hashlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cleanup_builds as cb


class PathSafety(unittest.TestCase):
    def test_manifest_consistent(self):
        self.assertTrue(cb.validate_manifest())
        classes = {c: sum(1 for m in cb.MANIFEST if m[1] == c) for c in (cb.KEEP, cb.BACKUP, cb.DELETE)}
        self.assertEqual(classes[cb.KEEP], 13)
        self.assertEqual(classes[cb.BACKUP], 3)

    def test_protected_refused(self):
        for p in cb.PROTECTED + ["native/core/build-shops/flow.txt", "native/core", "native/core/fixtures/x.txt"]:
            with self.assertRaises(cb.Refused, msg=p):
                cb.check_path(p)

    def test_boundaries(self):
        for p in [".", ".git", ".git/config", "native/../original", "native/core/../../x",
                  str(Path(tempfile.gettempdir()) / "outside-repo-dir")]:
            with self.assertRaises(cb.Refused, msg=p):
                cb.check_path(p)

    def test_historical_allowed_and_missing_tolerated(self):
        cb.check_path("native/core/build-a4-ui4")
        cb.check_path("native/core/build-does-not-exist")

    def test_delete_entries_not_protected(self):
        for rel, cls, _, _ in cb.MANIFEST:
            if cls != cb.KEEP:
                cb.check_path(rel)

    def test_symlink_refused(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            (t / "elsewhere").mkdir()
            (t / "native" / "core").mkdir(parents=True)
            try:
                os.symlink(t / "elsewhere", t / "native" / "core" / "build-x", target_is_directory=True)
            except OSError:
                self.skipTest("cannot create symlinks here")
            with self.assertRaises(cb.Refused):
                cb.check_path("native/core/build-x", repo=t)


class Worktree(unittest.TestCase):
    def test_clean_and_allowed(self):
        self.assertEqual(cb.check_worktree(["?? native/core/a4-release-ctest.log"], ["native/core/build-a3-01"]), [])

    def test_tracked_modified_refused(self):
        for code in (" M", "M ", "A ", "D ", "R "):
            with self.assertRaises(cb.Refused):
                cb.check_worktree([code + " README.md"], [])

    def test_untracked_inside_candidate_refused(self):
        with self.assertRaises(cb.Refused):
            cb.check_worktree(["?? native/core/build-a3-01/notes.txt"], ["native/core/build-a3-01"])

    def test_untracked_outside_warns(self):
        self.assertEqual(len(cb.check_worktree(["?? docs/new.md"], ["native/core/build-a3-01"])), 1)


class Backup(unittest.TestCase):
    def test_missing_marker_refused(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(cb.Refused):
                cb.check_backup("native/core/build-a3-hf5", t)

    def test_valid_marker_accepted_and_tamper_refused(self):
        rel = "native/core/build-a3-hf5"
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            lines = []
            for n in cb.UNIQUE[rel]:
                data = n.encode()
                (t / "build-a3-hf5").mkdir(exist_ok=True)
                (t / "build-a3-hf5" / n).write_bytes(data)
                lines.append("%s *%s" % (hashlib.sha256(data).hexdigest(), n))
            (t / "build-a3-hf5.sha256").write_text("\n".join(lines) + "\n")
            empty_repo = t / "repo"
            empty_repo.mkdir()
            cb.check_backup(rel, t, repo=empty_repo)
            (t / "build-a3-hf5" / "wire_runtime.py").write_bytes(b"tampered")
            with self.assertRaises(cb.Refused):
                cb.check_backup(rel, t, repo=empty_repo)


class Cli(unittest.TestCase):
    def run_cli(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = cb.main(list(args))
        return rc, out.getvalue()

    def test_default_dry_run(self):
        rc, out = self.run_cli()
        self.assertEqual(rc, 0)
        self.assertIn("Nothing was modified", out)

    def test_delete_needs_both_flags_and_group(self):
        self.assertEqual(self.run_cli("--delete")[0], 2)
        self.assertEqual(self.run_cli("--delete", "--yes-really")[0], 2)

    def test_backup_group_delete_refused_without_marker(self):
        rc, out = self.run_cli("--group", "backup-required", "--delete", "--yes-really",
                               "--backup-dir", str(Path(tempfile.gettempdir()) / "no-such-backup-dir"))
        self.assertEqual(rc, 2)
        self.assertIn("REFUSED", out)

    def test_check_command(self):
        self.assertEqual(self.run_cli("--check", "native/core/build-shops")[0], 1)
        self.assertEqual(self.run_cli("--check", "native/core/build-a4-ui4")[0], 0)


if __name__ == "__main__":
    unittest.main()
