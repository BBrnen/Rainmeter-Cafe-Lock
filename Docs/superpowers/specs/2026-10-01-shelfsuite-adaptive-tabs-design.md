# ShelfSuite adaptive tabs design

Date: 2026-10-01  
Branch: `feature/shelfsuite-layout-usability`  
Base: `e4796c37ecd5d0f0de12d2ddbec267c6a91f463b`

## Purpose

ShelfSuite shelf tabs must keep their current appearance while their coloured
background grows to fit a long shelf name. A normal short shelf layout remains
480 px wide. A name such as `SCHOOL/WORK` must not extend beyond its tab, and
later tabs must not overlap it.

This is F1 only. F2 grid snapping, Cafe Lock password and Locked Mode policy,
launcher actions, the browser editor, and icon importing are out of scope.

## Baseline and delivery decision

Cafe Lock currently uses ShelfSuite v2.1 at
`d4f186ba0b5c262c7559b80841132f5fd3884f3c`. Its installer does not bundle
ShelfSuite, and its compatibility test intentionally proves that the upstream
engine is not changed. The tab layout is calculated by that engine, so a
Rainmeter-only change cannot make an installed ShelfSuite tab wider.

F1 therefore needs one reviewed, version-locked Cafe Lock compatibility update
to the *same* `Shelf Suite` skin. It is an update to the existing installation,
not a second skin or profile. The update is limited to the tab-layout engine,
the three supplied shelf INI files, and the new-shelf template. It must retain
upstream attribution and its MIT licence. The Rainmeter installer remains
unchanged in this feature; release/deployment work will decide how the
compatibility update is presented to an owner.

The project will record the exact upstream base and an auditable F1 patch in
the repository. CI will prepare the disposable ShelfSuite fixture from that
approved patch rather than silently treating modified files as unchanged
upstream source.

## Behaviour

`TabWidth=85` remains the minimum tab width. A new
`TabHorizontalPadding=12` variable supplies twelve pixels of space on both
sides of every measured label. The active font, font size, tab height,
colours, rounded rectangle, centre alignment, hover action, and selection
logic do not change.

When ShelfSuite updates tabs, it sets a tab label, refreshes that text meter,
and reads its actual measured width from Rainmeter. It chooses the larger of
85 px and the measured text width plus 24 px. It places each following tab
after the preceding chosen width plus the existing `TabSpacing`.

ShelfSuite calculates the right edge of the final tab. Its runtime widget width
is the larger of the existing 480 px base width and that edge plus the normal
outer padding. The background and settings gear use that same runtime width.
`DynamicWindowSize=1` lets Rainmeter resize the shelf window after this
calculation. No position, shelf name, tab, item, or `config.lua` data is
rewritten.

## Compatibility and safety

Existing Shelf1, Shelf2, and Shelf3 configurations require no migration. A
new shelf produced by Cafe Lock uses the same INI setting and shared engine, so
it receives the layout automatically. Short-label shelves keep the 480 px
width and their 85 px tabs. Long labels may make a shelf wider than 480 px;
Rainmeter's existing screen placement remains responsible for keeping a window
usable.

The layout calculation is in ShelfSuite Lua and runs only after Cafe Lock has
already admitted normal skin/Lua activity. It introduces no bang, IPC route,
configuration write, unlock route, or bypass of Locked Mode. Maintenance Mode
rules remain owned by Cafe Lock and are not changed.

## Tests and acceptance evidence

The ShelfSuite CI fixture will test actual loaded meters, not only source text:

- short labels use at least 85 px, do not overlap, retain a 480 px shelf, and
  leave the gear at the expected right edge;
- a long `SCHOOL/WORK` label has a wider background with equal 12 px-side
  padding, moves later tabs, expands the shelf only when needed, and moves the
  gear with it;
- existing Shelf1--Shelf3 fixture configurations load unchanged;
- a newly created shelf template includes dynamic window sizing;
- existing Locked Mode launcher, hover, tab, and gear-blocking regressions
  still pass.

The full Windows x64 workflow remains required before F1 is called ready.
Manual checking on a spare PC/VM will cover visibly short and long names,
five long tabs, gear placement, reload/restart, and Locked versus Maintenance
Mode behaviour.

## Exclusions

F1 does not add a maximum label length, truncation, a new settings screen,
automatic modifications of a user's ShelfSuite directory, a ShelfSuite version
upgrade, F2 snapping, or installer/release changes.
