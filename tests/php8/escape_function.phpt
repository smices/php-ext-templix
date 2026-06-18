--TEST--
Templix escape function and compiled output escape HTML special characters
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_escape_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/quote.blade.php', '{{ $symbol }}');

$value = '<EQ&"001\'>';
$engine = new Templix\Engine([
    'mode' => 'prod',
    'path' => $views,
    'cache' => $cache,
]);

echo Templix\escape($value) . "\n";
echo $engine->render('quote', ['symbol' => $value]) . "\n";
?>
--EXPECT--
&lt;EQ&amp;&quot;001&#039;&gt;
&lt;EQ&amp;&quot;001&#039;&gt;
