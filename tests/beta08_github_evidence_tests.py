"""Synthetic API fixtures test refusal paths; they do NOT certify GitHub runs."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import beta08_github_evidence as gate


SHA = "a" * 40


def fixture(branch=gate.BRANCH, event="pull_request"):
    data = {}
    data[f"/branches/{branch}"] = {"name": branch, "commit": {"sha": SHA}}
    runs = []
    required = ({**gate.WORKFLOWS, **gate.RC_WORKFLOWS} if branch in
                ("beta-0.9.x-stage-full", gate.FIX_BRANCH, "main") else gate.WORKFLOWS)
    for index, (workflow, names) in enumerate(required.items(), start=1):
        run_id, attempt = 700 + index, 2
        runs.append({"id": run_id, "run_attempt": attempt,
                     "path": f".github/workflows/{workflow}@refs/pull/241/merge",
                     "head_sha": SHA, "head_branch": branch, "event": event,
                     "repository": {"full_name": gate.REPO},
                     "status": "completed", "conclusion": "success"})
        jobs = []
        for name in sorted(names):
            image = next((i for i in gate.IMAGES if name.endswith(i)), "ubuntu-24.04")
            jobs.append({"name": name, "head_sha": SHA, "run_id": run_id,
                         "started_at": "2026-11-24T20:00:00Z",
                         "completed_at": "2026-11-24T20:05:00Z",
                         "status": "completed", "conclusion": "success", "labels": [image]})
        data[f"/actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100"] = {
            "total_count": len(jobs), "jobs": jobs}
        artifacts = [
            {"id": run_id * 100 + artifact_index, "name": name,
             "digest": "sha256:" + "b" * 64, "expired": False,
             "size_in_bytes": 100, "created_at": "2026-11-24T20:04:00Z",
             "workflow_run": {"id": run_id, "head_sha": SHA, "head_branch": branch}}
            for artifact_index, name in enumerate(
                sorted(gate._expected_artifacts(workflow, run_id, attempt)), start=1)]
        data[f"/actions/runs/{run_id}/artifacts?per_page=100"] = {
            "total_count": len(artifacts), "artifacts": artifacts}
    data[f"/actions/runs?head_sha={SHA}&per_page=100"] = {
        "total_count": len(runs), "workflow_runs": runs}
    return data


class AuthenticatedEvidenceTests(unittest.TestCase):
    def test_fix_pass_requires_all_hosted_storage_jobs_and_rejects_wrong_event(self):
        data = fixture(gate.FIX_BRANCH, "push")
        result = gate.evaluate(lambda key: data[key], SHA, branch=gate.FIX_BRANCH, event="push")
        self.assertIn("beta09-stage4-storage.yml", result["verified_workflows"])
        data["/actions/runs/705/attempts/2/jobs?per_page=100"]["jobs"][0]["conclusion"] = "skipped"
        with self.assertRaises(ValueError):
            gate.evaluate(lambda key: data[key], SHA, branch=gate.FIX_BRANCH, event="push")
        data = fixture(gate.FIX_BRANCH, "pull_request")
        with self.assertRaises(ValueError):
            gate.evaluate(lambda key: data[key], SHA, branch=gate.FIX_BRANCH, event="push")

    def test_complete_synthetic_records_describe_only_read_only_evidence(self):
        data = fixture()
        report = gate.evaluate(lambda key: data[key], SHA)
        self.assertEqual(set(report["verified_workflows"]), set(gate.WORKFLOWS))
        self.assertTrue(report["all_required_hosted_checks_successful"])
        self.assertFalse(report["owner_release_approval"])
        self.assertFalse(report["merge_authorized"])
        self.assertFalse(report["artifact_archive_contents_independently_verified"])
        self.assertNotIn("token", str(report).lower())

    def test_wrong_head_attempt_artifact_or_required_job_fail_closed(self):
        def pending(data):
            data[f"/actions/runs?head_sha={SHA}&per_page=100"]["workflow_runs"][0]["status"] = "in_progress"
        def missing_job(data):
            jobs = data["/actions/runs/701/attempts/2/jobs?per_page=100"]
            jobs["jobs"].pop(); jobs["total_count"] -= 1
        def wrong_attempt(data):
            data["/actions/runs/701/artifacts?per_page=100"]["artifacts"][0]["name"] += "-1"
        def no_digest(data):
            data["/actions/runs/701/artifacts?per_page=100"]["artifacts"][0]["digest"] = None
        def wrong_head(data):
            data["/actions/runs/702/attempts/2/jobs?per_page=100"]["jobs"][0]["head_sha"] = "c" * 40
        def self_hosted(data):
            data["/actions/runs/703/attempts/2/jobs?per_page=100"]["jobs"][0]["labels"] = ["self-hosted"]
        def incomplete_page(data):
            data[f"/actions/runs?head_sha={SHA}&per_page=100"]["total_count"] += 1
        def stale_branch(data):
            data[f"/branches/{gate.BRANCH}"]["commit"]["sha"] = "c" * 40
        def wrong_artifact_branch(data):
            data["/actions/runs/701/artifacts?per_page=100"]["artifacts"][0]["workflow_run"]["head_branch"] = "main"
        def old_attempt_artifact(data):
            data["/actions/runs/701/artifacts?per_page=100"]["artifacts"][0]["created_at"] = "2026-11-23T20:04:00Z"
        def duplicate_artifact_id(data):
            artifacts = data["/actions/runs/701/artifacts?per_page=100"]["artifacts"]
            artifacts[1]["id"] = artifacts[0]["id"]
        for damage in (pending, missing_job, wrong_attempt, no_digest, wrong_head,
                       self_hosted, incomplete_page, stale_branch,
                       wrong_artifact_branch, old_attempt_artifact, duplicate_artifact_id):
            data = copy.deepcopy(fixture())
            damage(data)
            with self.subTest(damage=damage.__name__), self.assertRaises(ValueError):
                gate.evaluate(lambda key: data[key], SHA)

    def test_branch_advancing_during_validation_invalidates_old_green_matrix(self):
        data = fixture()
        reads = 0

        def get(key):
            nonlocal reads
            if key == f"/branches/{gate.BRANCH}":
                reads += 1
                if reads == 2:
                    return {"name": gate.BRANCH, "commit": {"sha": "c" * 40}}
            return data[key]

        with self.assertRaisesRegex(ValueError, "current development branch HEAD"):
            gate.evaluate(get, SHA)

    def test_latest_failed_run_overrides_older_green_run(self):
        data = fixture()
        runs = data[f"/actions/runs?head_sha={SHA}&per_page=100"]
        latest = copy.deepcopy(runs["workflow_runs"][0])
        latest["id"] = 901
        latest["status"] = "completed"
        latest["conclusion"] = "failure"
        runs["workflow_runs"].append(latest)
        runs["total_count"] += 1
        with self.assertRaisesRegex(ValueError, "Pending/failed"):
            gate.evaluate(lambda key: data[key], SHA)


if __name__ == "__main__":
    unittest.main()
