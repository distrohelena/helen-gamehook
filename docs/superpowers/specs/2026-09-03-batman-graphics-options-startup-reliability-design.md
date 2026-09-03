# Batman Graphics Options Startup Reliability Design

## Goal

Make the four active Batman graphics settings initialize reliably and display the values persisted by the launcher-owned configuration after every relaunch.

## Observed Failures

Live testing exposed two independent startup races:

- one launch exhausted the ActionScript initialization deadline before any graphics observer resolved the shared carrier;
- a later relaunch resolved the carrier but returned `On / Off / Normal / Off`, while both INI files contained `Off / 8x / Normal / On`.

The runtime log showed every unresolved graphics observer performing its own full `0x10000000..0x30000000` scan. It also showed the relaunch responses using state read before Batman finished regenerating `BmEngine.ini`. The persisted UTF-16LE `UserEngine.ini` already contained the correct values and is the launcher-owned authority.

## Chosen Changes

### Authoritative startup state

`BatmanGraphicsConfigService::LoadIntoDispatcher` will read the sibling `UserEngine.ini` rather than the generated `BmEngine.ini`. The file is required: a missing, unreadable, incomplete, or invalid launcher file fails the startup command instead of falling back to defaults or stale generated state.

Apply behavior remains unchanged. A successful Apply writes the complete validated graphics draft to `UserEngine.ini` first and `BmEngine.ini` second, preserving each document's existing encoding and unrelated content.

### One unresolved scan per address group

One polling pass may perform at most one broad scan for each unresolved `addressGroup`. The first due observer in a group is the scan leader for that pass. If it resolves and structurally validates a carrier, later group members reuse the cached address during the same pass. If it finds nothing, later group members skip their duplicate broad scans until the next pass.

Observers without an address group keep their current behavior. A stale resolved group still clears its shared cache; the next eligible pass performs one replacement scan and shares the replacement address with every member.

This preserves the existing carrier signature, protocol values, polling intervals, and ten-second ActionScript initialization deadline. Extending the deadline is intentionally excluded because it would only conceal redundant scanning.

## Tests

Test-driven implementation will add:

- a config-service test with conflicting `UserEngine.ini` and `BmEngine.ini` values, proving startup publishes only the launcher-owned values;
- failure coverage for missing or invalid `UserEngine.ini`, proving no generated-file/default fallback occurs;
- an observer-service test proving one unresolved grouped scan per polling pass;
- a late-carrier test proving the next pass resolves the carrier and shares its address across all group members;
- existing grouped relocation, acknowledgement, config persistence, package, and shell tests as regression coverage.

## Live Verification

After native tests and package validators pass, rebuild and install only the current Release hook and graphics package. On two consecutive launches:

1. open Graphics Options and confirm the four rows resolve before the deadline;
2. confirm the displayed values match `UserEngine.ini`;
3. change one value, Apply, and verify acknowledgements plus both INI files;
4. relaunch and confirm the applied values return without an intermittent `Unavailable` state.

## Non-Goals

- Changing the carrier memory signature or scan address range
- Adding baked/default UI values
- Delaying startup with sleeps or retries in configuration code
- Activating additional graphics rows
- Changing subtitle-pack behavior
