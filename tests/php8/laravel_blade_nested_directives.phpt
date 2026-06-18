--TEST--
Templix compiles nested Blade directive expressions
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_nested_directives_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/nested.blade.php', '@if (in_array($role, ["admin", "ops"], true)){{ $label }}@elseNO@endif');

$engine = new Templix\Engine([
    'mode' => 'dev',
    'path' => $views,
    'cache' => $cache,
]);
echo $engine->render('nested', [
    'role' => 'ops',
    'label' => '<Live>',
]);
?>
--EXPECT--
&lt;Live&gt;
