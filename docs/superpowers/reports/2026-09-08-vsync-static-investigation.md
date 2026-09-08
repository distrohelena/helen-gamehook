# VSync live-apply investigation (static evidence only)

Read-only investigation approved after fullscreen commit 9dc4358. No runtime
code, installed files, process memory or configuration was changed. No game was
launched. Addresses below are preferred VAs in the SHA-256-pinned retail image
4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028.

## Established leads

- UTF-16 UseVsync keys occur at 0x0203DAB0, 0x0203ED00, 0x0203FEA0 and
  0x02041188. The first two referenced tables associate the key with settings-data
  offset 0x21C (e.g. 0x00C20226/0x00C20337 and 0x00C3F7A5/0x00C3F8B6).
  The full settings owner includes a four-byte prefix, giving owner+0x220.
- Renderer code reads global 0x026C0D58 at 0x00EA65F9, consistent with settings
  owner 0x026C0B38 + 0x220. The arithmetic through 0x00EA663F produces 1 for
  nonzero and 0x80000000 for zero, then stores it at offset 0x34 within the
  0x38-byte presentation-parameter block initialized at 0x00EA65AE.
- The enclosing renderer routine starts at 0x00EA6310. Its existing-device path
  at 0x00EA6529–0x00EA65A8 can return without rebuilding parameters when mode,
  window and dimensions do not require a rebuild. Merely changing the setting
  and calling the same-size viewport resize is therefore not a proven VSync apply.
- 0x00C40090 is a candidate high-level settings apply wrapper. Stock callers pass
  a full settings-data pointer and another argument; its return is ret 8. It calls
  0x00C3FDE0 when the global at 0x026C3CDC is present and performs a 0xAB-dword
  copy on the alternative path. Nonzero second argument gates calls to
  0x00C3F6B0 and later 0x00C24710. The inner routine also gates the settings
  serialization call at 0x00C3FF66–0x00C3FF7D. This is evidence for a save-control
  argument, not proof that zero suppresses every transitive engine-owned write.
- The settings apply route calls 0x00C24B50, which has a thread check and a
  queued-command path. Its complete effects and command execution contract still
  require tracing before binding it from Helen's menu callback.
- A stock settings reader writes the VSync global at 0x01517291, compares its
  previous value, and writes 1 to 0x026B221C at 0x015172A7 when changed. Another
  stock path saves/disables/restores VSync around 0x016D4C6A–0x016D4CB4 and also
  writes that flag. No consumer of this flag was established by this investigation.
  It must not be treated as a verified refresh request.
- Renderer owner+0x28 is set after device errors at 0x00EA7C2D, 0x00EAA28E and
  0x00EAA29F. Do not repurpose this device-error state to force VSync application.

## Reproduction and recommendation

Search evidence in `output/ShippingPC-BmGame.disasm.txt`; verify critical sequences
against the executable using the existing `Inspect-Viewport.py`. The latter was
run successfully for VA 0x00EA65F9, length 0x4D and checked the whole-file identity.

VSync is not ready to enable from these findings alone. The next required work is
to establish the stock settings-to-renderer refresh boundary and its scheduling,
then verify the actual presentation interval changed without changing resolution
or fullscreen. Preserve the no-save experiment and the working viewport path.
Do not write global flags, spoof device loss, use timers, or claim that a stored
setting proves the current device adopted it.

## Follow-up: queued update and refresh boundary

The queued command from 0x00C24B50 copies nine dwords, not the complete settings
block. Its single-thread path calls 0x00C20B30 directly with the same payload.
That consumer compares/updates globals 0x026C0DE8 through 0x026C0E08 and calls
0x00715940 on change. It does not update/compare VSync at 0x026C0D58. Consequently
this command is not the missing VSync invalidation mechanism.

A raw whole-image search for the little-endian absolute address 0x026B221C found
only the three previously identified writer operands. This does not exclude an
indirect reader, but provides no usable live-refresh contract. The renderer's
owner+0x28 stores were checked and are device-error handling, not a setting API.

The existing-device refresh decision at 0x00EA659C–0x00EA65AE is a concrete
candidate for a narrowly scoped hook: it skips to 0x00EA7388 when its accumulated
rebuild decision is false. The actual engine reset path at 0x00EA664C visits
resource-release callbacks, invokes device Reset at 0x00EA66BE with its own
presentation block, then visits resource-recreation callbacks at 0x00EA67AB.
It includes the engine's pre-existing failure retry behavior; we must not claim
that binding this path adds no possible engine-side wait or guarantees success.

Recommended next design is an explicit, scoped presentation-settings invalidation
request honored at the engine's own rebuild decision, not spoofed device loss or
temporary resolution/fullscreen changes. Apply should use a full current-settings
snapshot with only VSync changed and the candidate engine save-control flag off,
then enter through a verified higher-level viewport synchronization route. Read
back the real swap-chain interval after return; any failure/uncertainty must remain
visible. The exact same-size call chain, calling convention, hook register/flags
preservation, save side effects and reset failure handling need implementation-time
verification. This is a new hook design, not an already safe drop-in binding.

No implementation or deployment was performed during this follow-up.
