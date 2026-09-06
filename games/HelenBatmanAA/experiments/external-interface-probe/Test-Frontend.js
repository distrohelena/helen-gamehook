const fs = require('node:fs');
const assert = require('node:assert/strict');
const vm = require('node:vm');

/** Extracts a generated controller method for execution, preserving its emitted behavior. */
function methodSource(source, name) {
    const match = new RegExp('(?:function\\s+' + name + '|\\.' + name + '\\s*=\\s*function)\\s*\\(([^)]*)\\)').exec(source);
    assert.ok(match, 'Missing controller method ' + name);
    const brace = source.indexOf('{', match.index + match[0].length);
    let depth = 1;
    let end = brace + 1;
    for (; depth !== 0 && end < source.length; ++end) {
        if (source[end] === '{') ++depth;
        if (source[end] === '}') --depth;
    }
    assert.equal(depth, 0);
    return '(function(' + match[1] + '){' + source.slice(brace + 1, end - 1) + '})';
}

const source = fs.readFileSync(process.argv[2], 'utf8');
const begin = methodSource(source, 'BeginInitialization');
for (const reply of [true, false, undefined, null, 1, 'false']) {
    const calls = [];
    const nextRequests = [];
    const context = vm.createContext({
        getTimer: () => 100,
        flash: { external: { ExternalInterface: { call: name => { calls.push(name); return reply; } } } }
    });
    const screen = { BlockInput: () => {} };
    const controller = {
        Screen: screen,
        Settings: [{ InitialIndex: -1, DraftIndex: -1 }],
        ResetResolutionCatalogState() {},
        RefreshRows() {},
        SendInitializationRequest() { nextRequests.push(this.InitializationIndex); }
    };
    const initialize = vm.runInContext(begin, context);
    initialize.call(controller);
    assert.deepEqual(calls, ['Helen_ProbeGetFullscreen'], 'First read must be one direct call, not carrier traffic.');
    assert.deepEqual(nextRequests, [1], 'Polling must start after Fullscreen, even when its direct read fails.');
    if (typeof reply === 'boolean') {
        assert.equal(controller.Settings[0].InitialIndex, reply ? 1 : 0);
        assert.equal(controller.Settings[0].DraftIndex, reply ? 1 : 0);
    } else {
        assert.equal(controller.Settings[0].InitialIndex, -1, 'Invalid direct reply must not become Off.');
        assert.equal(controller.FullscreenReadFailure, 'Direct ' + typeof reply);
    }
}
console.log('DIRECT_FRONTEND_CONTRACT_PASS');
const canEdit = vm.runInNewContext(methodSource(source, 'CanEdit'));
const editable = {
    GetSettingForRow: () => ({ InitialIndex: 0, DraftIndex: 0 }),
    AreSettingsLoaded: () => true, InitializationComplete: true,
    InitializationFailed: false, InteractionBlocked: false, RollbackLocked: false
};
assert.equal(canEdit.call(editable, 1), false, 'Probe Fullscreen must stay read-only.');
assert.equal(canEdit.call(editable, 3), true, 'Existing scalar edits must remain enabled.');
