const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');

/** Executes the actual emitted/decompiled clip-load handler with the display objects it controls. */
function loadRow(source, controller, label) {
    const start = source.indexOf('{');
    const end = source.lastIndexOf('}');
    assert.ok(start >= 0 && end > start, 'Apply row handler missing');
    const row = {Label: label, ItemText: {text: 'old status', _visible: true},
        LeftClicker: {_visible: true}, RightClicker: {_visible: true}};
    const context = vm.createContext({_parent: {GraphicsOptionsController: controller}, row});
    vm.runInContext('(function(){' + source.slice(start + 1, end) + '}).call(row)', context);
    return row;
}

const source = fs.readFileSync(process.argv[2], 'utf8');
let dirty = false;
let activations = 0;
const controller = {
    CanApply: () => dirty,
    GetApplyStatusText: () => 'Apply Integrity Uncertain',
    ApplyChanges: () => { activations++; }
};
for (const label of [{Label: {Text: {text: ''}}}, {Text: {text: ''}}, {text: ''}]) {
    const row = loadRow(source, controller, label);
    const text = label.Label?.Text ?? label.Text ?? label;
    assert.equal(text.text, 'Apply Changes');
    assert.equal(row.ItemText._visible, false, 'Apply status column remains visible');
    assert.equal(row.ItemText.text, '', 'Hidden Apply column retains stale status text');
    assert.equal(row.Label._alpha, 40, 'Clean Apply label must remain dimmed');
    assert.equal(row.LeftClicker._visible, false);
    assert.equal(row.RightClicker._visible, false);
    dirty = true;
    row.Update();
    assert.equal(row.Label._alpha, 100, 'Dirty Apply label must become active');
    assert.equal(row.ItemText._visible, false, 'Update restored the second column');
    row.RunAction();
    dirty = false;
}
assert.equal(activations, 3, 'Apply action wiring changed');
const unbound = loadRow(source, undefined, {text: ''});
assert.equal(unbound.ItemText._visible, false, 'Unbound row shows the second column');
assert.equal(unbound.Label._alpha, 40);
console.log('BATMAN_APPLY_ROW_PASS');
