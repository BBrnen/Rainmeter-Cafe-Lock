# Task 3 file-replacement safety decision

Status: proposal awaiting owner approval; not an amendment to the approved specification or plan.

## Verified checkpoint

Task 2 passed the complete Windows workflow at `b735be4b`:
https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36872130507

The native inspection tests cover all 64 approved INI variants, additional shelves recognized by contents, already-compatible no-op, known partial updates, customized/missing shelves, linked files and redirected ancestors, safe relative error messages, and altered manifest rejection. Locked disposable configuration/icon/theme sentinels remain unopened by inspection.

Task 3 tests are committed, but application remains an unsupported stub. Windows run https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36872832896 compiles the application test harness and fails at the expected `Updated` assertion. No compatibility write or recovery implementation has been added.

## The problem in the planned primitive

Task 3 Step 3 specifies checking file identity/hash immediately before `ReplaceFileW`. That API takes filenames rather than the verified file handle or expected identity. A separate check does not make the subsequent replacement conditional on that identity. If another process renames a different file onto the target path in between, replacement can operate on that different file.

Holding the original open without delete sharing blocks the rename, but also blocks the filename-based replacement. Permitting delete sharing allows the rename race again. The existing editor mitigates this by retaining the actual displaced version in a separate recovery copy. Copying that pattern without explaining its semantics would not establish the stronger refusal/no-overwrite promise requested for this compatibility operation.

References:

- [ReplaceFileW parameters, sharing and partial failures](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
- [CreateFileW sharing rules](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)
- [Handle-based file information operations](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle)
- [Rename with ReplaceIfExists false refuses an occupied destination](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)

## Recommended narrow adjustment

Keep the approved preflight, authorization, exact hashes, timestamped verified backups, protected payload, and recovery record. Change only the replacement/recovery primitive:

1. Hold each verified target by its Windows file handle, denying outside writes and renames. If this cannot be done, refuse safely.
2. Stage and verify the approved output and verify the declared backup before moving any original.
3. Move the verified original by its handle to a unique holding location associated with the backup. Use a no-overwrite destination.
4. Move the verified staged output by its handle into the now-vacant target path with replacement disabled. If another file appears there, fail without overwriting it.
5. Apply the same identity-bound, no-overwrite rule during recovery. If safe recovery cannot finish, retain the backup/holding files and identify the affected approved path for manual recovery.
6. Recheck Maintenance authorization before every mutation, including recovery. Do not continue recovery writes after authorization is revoked.

This does not introduce a transaction framework, a new program, dependency, elevation, unlock route, or user-data migration. It changes neither the F1 patch nor its hashes. No config.lua, icon, theme, launcher-data or position file is inspected or modified.

The trade-off must be explicit: moving the original and placing the new file are two operations. A crash or interruption between them can briefly leave that target absent. The verified backup and phase record must support manual restoration. Do not claim an atomic per-file replacement. Leave running skins untouched and instruct the owner to reload only after completion, as already specified.

## Verification needed before accepting implementation

Add Windows RED/GREEN tests for a competing rename before target acquisition, a competing file creation after the original move, interruption after the original move, failed placement, denied write/delete sharing, authorization revocation, and safe/manual recovery. Confirm that an unexpected competing file is never overwritten and all originals remain recoverable. Retain the existing success/no-op/backup/refusal tests, live-skin tests, standard-user tests, and complete x64 workflow.

Scope of the amendment: Task 3 replacement/recovery internals and their tests only; update the approved plan/spec wording about per-file atomicity only after owner approval. Tasks 4–6 remain pending. No merge, release, deployment, PowerShell delivery integration or F2 work is authorized by this proposal.
