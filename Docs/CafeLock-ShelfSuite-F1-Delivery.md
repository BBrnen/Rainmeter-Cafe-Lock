# ShelfSuite F1 compatibility update contract

F1 records a future delivery path for the adaptive tab patch. It is not implemented or shipped by F1.
The normal Rainmeter Cafe Lock installer does
not deploy this patch.

The future owner-visible artifact is named
`ShelfSuite-Cafe-Lock-F1-Compatibility.zip`. It must be generated only from
the committed `0001-cafe-lock-adaptive-tabs.patch` and the exact ShelfSuite v2.1
base `d4f186ba0b5c262c7559b80841132f5fd3884f3c`, retaining the upstream MIT
licence and attribution.

Before modifying anything, the future updater must ask the owner to back up the
existing `Shelf Suite` directory. It must verify the upstream engine and
variables file hashes, then make a timestamped backup of every file it will
replace. It updates the same `Shelf Suite` directory only; it must not install a
second skin or profile.

For existing shelves, the future updater may add `DynamicWindowSize=1` only to
the recognised `[Rainmeter]` section of `ShelfN/Shelf.ini`. It must stop with an
actionable message when it finds an unrecognised or customised file. It must
never read, rewrite, or upload `config.lua`.

A later release task must implement, test, package, and review this contract
before the compatibility ZIP is offered to an owner.
