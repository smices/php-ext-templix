--TEST--
Templix compiles Blade HTML attribute directives
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_blade_attributes_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/attrs.blade.php', <<<'BLADE'
<tr @class(['row', 'up' => $up, 'down' => !$up]) @style(['color: green' => $up, 'display: none' => false])>
<option @selected($selected)>{{ $label }}</option>
<input @checked($checked) @disabled($disabled) @readonly($readonly) @required($required)>
BLADE);

$engine = new Templix\Engine(['mode' => 'dev', 'path' => $views, 'cache' => $cache]);
echo $engine->render('attrs', [
    'up' => true,
    'selected' => true,
    'checked' => true,
    'disabled' => false,
    'readonly' => true,
    'required' => true,
    'label' => '<Buy>',
]);
?>
--EXPECT--
<tr class="row up" style="color: green;">
<option selected>&lt;Buy&gt;</option>
<input checked  readonly required>
