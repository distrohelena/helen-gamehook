# Graphics Options Row Stack Center Design

## Goal

Move the `Graphics Options` row stack so the options read as centered inside the enlarged dark panel, while leaving the header and background panel transforms unchanged.

## Scope

This change affects only the fixed graphics option rows in `ScreenOptionsGraphics`:

- left-side option labels
- selection arrows
- right-side option values
- row hit/selection clips that share each row depth

The header pair, large background panel, Batman background art, main Options menu, Audio Options, and Game Options screens stay unchanged.

## Placement

Use the exported `MainV2` stage size already documented by the graphics-panel retune:

```text
stage width  = 20480
stage height = 15360
```

The requested move is:

```text
15% left  = 20480 * 0.15 = 3072
30% up    = 15360 * 0.30 = 4608
```

The current row X anchor is `-781`, so the target row X anchor is:

```text
-781 - 3072 = -3853
```

Each current graphics row Y coordinate moves up by `4608` matrix units.

## Implementation

Update `GraphicsOptionsXmlPatcher.cs` to:

- replace the row anchor constant with a graphics-specific row translate X value of `-3853`
- subtract `4608` from every entry in `GraphicsRowTranslateY`
- keep using the existing `NormalizeGraphicsRowPlacements` path so all timeline placements at each row depth move together

Update `Test-BatmanGraphicsOptionsLayout.ps1` to lock:

- every row depth has the new `translateY`
- every row depth has `translateX = -3853`

## Verification

Run the existing graphics layout and package regressions:

```powershell
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1
```

After implementation, rebuild and deploy the graphics-options pack, then verify the live `Graphics Options` screen visually.
