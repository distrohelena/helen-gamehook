const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');

/** Extracts actual generated or decompiled methods, preserving their bodies instead of reimplementing the controller. */
function methods(source) {
    const result = new Map();
    const pattern = /(?:function\s+(\w+)|\.(\w+)\s*=\s*function)\s*\(([^)]*)\)/g;
    for (let match; (match = pattern.exec(source));) {
        const name = match[1] || match[2];
        const start = source.indexOf('{', pattern.lastIndex);
        let end = start + 1;
        let depth = 1;
        for (; depth && end < source.length; end++) {
            if (source[end] === '{') depth++;
            if (source[end] === '}') depth--;
        }
        assert.equal(depth, 0, 'Unbalanced generated method ' + name);
        if (!result.has(name)) result.set(name, '(function(' + match[3] + '){' + source.slice(start + 1, end - 1) + '})');
        pattern.lastIndex = end;
    }
    return result;
}

/** Supplies native-boundary fixtures and runs the actual frontend initialization. */
function initialize(source, overrides = {}) {
    const calls = [];
    const scalars = [0, 1, 5, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1600, 900, 1920, 1080, 1];
    const catalogs = [[[1280, 720], [1600, 900]], [[1280, 720], [1920, 1080]]];
    const context = vm.createContext({
        // Legacy polling must fail the test, not accidentally receive plausible responses.
        getTimer: () => { throw new Error('Graphics still depends on a timer'); },
        flash: {external: {ExternalInterface: {call(name, ...args) {
            calls.push([name, ...args]);
            if (Object.hasOwn(overrides, name)) {
                const value = overrides[name];
                return typeof value === 'function' ? value(...args) : value;
            }
            switch (name) {
            case 'Helen_Graphics_OpenV1': return 23;
            case 'Helen_Graphics_GetV1': return scalars[args[1]];
            case 'Helen_Graphics_ModeCountV1': return catalogs[args[1]].length;
            case 'Helen_Graphics_ModeWidthV1': return catalogs[args[1]][args[2]][0];
            case 'Helen_Graphics_ModeHeightV1': return catalogs[args[1]][args[2]][1];
            case 'Helen_Graphics_BeginApplyV1': return 42;
            case 'Helen_Graphics_CommitV1': return 0;
            case 'Helen_Graphics_EndReadV1':
            case 'Helen_Graphics_SetFieldV1':
            case 'Helen_Graphics_SetResolutionV1':
            case 'Helen_Graphics_CancelApplyV1':
            case 'Helen_Graphics_CloseV1': return true;
            case 'FE_PlaySoundFromString': return undefined;
            default: throw new Error('Unexpected graphics callback: ' + name);
            }
        }}}}
    });
    const controller = {Screen: {BlockInput() {}, ReUpdate() {}}, DetailLeafRows: [6,7,8,9,10,11,12]};
    for (const [name, body] of methods(source)) controller[name] = vm.runInContext(body, context);
    controller.CreateSettings();
    controller.BeginInitialization();
    return {controller, calls};
}

const source = fs.readFileSync(process.argv[2], 'utf8');
const {controller, calls} = initialize(source);
assert.equal(controller.InitializationComplete, true);
assert.equal(controller.Settings[0].InitialIndex, 0);
assert.equal(controller.Settings[2].InitialIndex, 4, 'Normalized 16x MSAA must select display index four');
assert.equal(controller.GetResolutionLabel(), '1600 x 900');
assert.equal(calls.filter(c => c[0] === 'Helen_Graphics_OpenV1').length, 1);
assert.equal(calls.filter(c => c[0] === 'Helen_Graphics_EndReadV1').length, 1);
controller.DecrementSetting(3);
assert.equal(controller.Settings[1].DraftIndex, 0, 'Left arrow did not decrease VSync');
assert.equal(controller.CanApply(), true);
controller.ApplyChanges();
assert.equal(controller.CanApply(), false, 'Successful Apply must become clean');
assert.equal(controller.Settings[1].InitialIndex, 0);
assert.equal(calls.filter(c => c[0] === 'Helen_Graphics_CommitV1').length, 1);
assert.ok(calls.some(c => c[0] === 'Helen_Graphics_SetFieldV1' && c[3] === 2 && c[4] === 5), 'Apply sent MSAA display index rather than normalized state');
assert.equal(controller.Destroy(), true);
assert.equal(calls.filter(c => c[0] === 'Helen_Graphics_CloseV1').length, 1);

for (const outcome of [1, 2, 3, undefined]) {
    const test = initialize(source, {'Helen_Graphics_CommitV1': outcome});
    test.controller.DecrementSetting(3);
    test.controller.ApplyChanges();
    assert.equal(test.controller.ApplyInProgress, false);
    if (outcome === 1) {
        assert.equal(test.controller.Settings[1].InitialIndex, 1, 'Failed Apply replaced committed baseline');
        assert.equal(test.controller.CanApply(), true, 'Verified NotApplied should allow correcting/retrying the draft');
    } else if (outcome === 3) {
        assert.equal(test.controller.Settings[1].InitialIndex, 0, 'Cleanup-failed commit was reported as unapplied');
        assert.equal(test.controller.CanApply(), false);
        assert.ok(test.controller.UiStatus.length > 0);
    } else {
        assert.equal(test.controller.CanApply(), false, 'Unknown/uncertain persistence must lock Apply');
    }
}

const partial = initialize(source, {'Helen_Graphics_GetV1': (session, field) => field === 1 ? 'false' : [0,1,5,1,1,0,0,0,1,0,1,0,1600,900,1920,1080,1][field]});
assert.equal(partial.controller.Settings[1].InitialIndex, -1, 'Wrong type became a valid setting');
assert.equal(partial.controller.Settings[2].InitialIndex, 4, 'Bad VSync erased valid MSAA');
assert.equal(partial.calls.filter(c => c[0] === 'Helen_Graphics_EndReadV1').length, 1);
const stageFailure = initialize(source, {'Helen_Graphics_SetFieldV1': undefined});
stageFailure.controller.DecrementSetting(3);
stageFailure.controller.ApplyChanges();
assert.equal(stageFailure.calls.filter(c => c[0] === 'Helen_Graphics_CancelApplyV1').length, 1);
assert.equal(stageFailure.calls.filter(c => c[0] === 'Helen_Graphics_CommitV1').length, 0);
assert.ok(!calls.some(c => c[0] === 'FE_SetControlType' || c[0] === 'FE_GetControlType'));

const displayChange = initialize(source);
displayChange.controller.IncrementSetting(1);
assert.equal(displayChange.controller.GetResolutionLabel(), '1920 x 1080');
displayChange.controller.ApplyChanges();
assert.ok(displayChange.calls.some(c => c[0] === 'Helen_Graphics_SetResolutionV1' && c[3] === 1 && c[4] === 1), 'Fullscreen used the wrong immutable catalog index');
displayChange.controller.DecrementSetting(1);
assert.equal(displayChange.controller.GetResolutionLabel(), '1600 x 900', 'Returning to windowed lost the remembered exact size');
displayChange.controller.DecrementResolution();
assert.equal(displayChange.controller.GetResolutionLabel(), '1280 x 720');

const missingOpen = initialize(source, {'Helen_Graphics_OpenV1': undefined});
assert.equal(missingOpen.controller.InitializationFailed, true);
assert.equal(missingOpen.calls.length, 1, 'Missing direct runtime invoked a fallback transport');
const missingBegin = initialize(source, {'Helen_Graphics_BeginApplyV1': undefined});
missingBegin.controller.DecrementSetting(3);
missingBegin.controller.ApplyChanges();
assert.equal(missingBegin.calls.filter(c => c[0] === 'Helen_Graphics_CommitV1').length, 0);
assert.equal(missingBegin.controller.ApplyInProgress, false);
const failedResolution = initialize(source, {'Helen_Graphics_SetResolutionV1': undefined});
failedResolution.controller.IncrementSetting(1);
failedResolution.controller.ApplyChanges();
assert.equal(failedResolution.calls.filter(c => c[0] === 'Helen_Graphics_CancelApplyV1').length, 1);
assert.equal(failedResolution.calls.filter(c => c[0] === 'Helen_Graphics_CommitV1').length, 0);
const missingCatalog = initialize(source, {'Helen_Graphics_ModeCountV1': (session, kind) => kind === 0 ? undefined : 2});
assert.equal(missingCatalog.controller.WindowedResolutionAvailable, false);
assert.equal(missingCatalog.controller.FullscreenResolutionAvailable, true);
assert.equal(missingCatalog.controller.Settings[2].InitialIndex, 4);
const failedClose = initialize(source, {'Helen_Graphics_CloseV1': undefined});
assert.equal(failedClose.controller.Destroy(), false);
assert.equal(failedClose.controller.SessionId, 23, 'Rejected close discarded native ownership');

const endReadException = initialize(source, {'Helen_Graphics_EndReadV1': () => { throw new Error('Native boundary unavailable'); }});
assert.equal(endReadException.controller.NativeCanApply, false);
assert.equal(endReadException.controller.Settings[2].InitialIndex, 4);
console.log('BATMAN_DIRECT_GRAPHICS_FRONTEND_PASS');
