# Rainmeter Cafe Lock

## Stage 1: upstream import

This repository starts from the official Rainmeter stable release 4.5.26,
revision 3894, without changes to application source or build scripts.

- Upstream: https://github.com/rainmeter/rainmeter
- Release tag: `v4.5.26.3894`
- Baseline commit: `5a124b6a09e2f7f67f8be9232718c489100e6173`
- Upstream history: all 3,894 commits reachable from that baseline are retained.
- Historical tags reachable from the baseline are preserved under `upstream/`
  in this repository, pointing to the original Git objects. The prefix avoids
  confusing upstream versions with Cafe Lock releases or triggering upstream
  release automation.

Inherited workflows are preserved byte-for-byte in `.github/upstream-workflows/`
instead of `.github/workflows/`. They are intentionally inactive: the upstream
signing and WinGet publishing identities belong to Rainmeter, not this project.
The build workflow will be enabled/adapted separately for baseline validation.
No Windows build has been validated at this stage.

Original licensing and attribution remain in place. Cafe Lock is an independent
derivative, not an official Rainmeter release.

## Approved implementation stages

1. Import the stable source and history (this stage).
2. Prove an unmodified Windows build works.
3. Implement Cafe Lock, preserving ordinary skin left-click actions while
   blocking dragging, modifier-key overrides, menus, and management commands.
4. Implement UAC-authorized Maintenance Mode for the current running instance,
   with a manual **Lock now** action and automatic locking on Rainmeter restart.
   No timeout or Windows session-lock behavior is planned for the first version.
5. Produce Cafe Lock installer and x64 application artifacts automatically.
6. Test compatibility, especially ShelfSuite.

Each stage has a separate focused commit and a user-visible checkpoint before
the next stage. Implementation has not started in this import commit.

ShelfSuite itself must remain unchanged initially. Its settings/configurator
launch should be blocked by Rainmeter while locked and work normally during
Maintenance Mode. A ShelfSuite modification is a fallback only if this cannot
be implemented cleanly within Rainmeter.

## Future upstream updates

Fetch the official repository as the `upstream` remote. Select a reviewed stable
release and merge it into a dedicated update branch; preserve its ancestry.
Do not replace this history with a source ZIP. Keep Cafe Lock policy, elevation,
packaging, and compatibility changes in focused commits to simplify review.

Inherited release workflows must stay inactive when integrating updates.
