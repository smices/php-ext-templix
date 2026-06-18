--TEST--
Templix prod mode renders from compiled cache without re-reading changed source
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_volt_cache_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/card.blade.php', 'Cached {{ $name }}');

$engine = new Templix\Engine(['mode' => 'prod', 'path' => $views, 'cache' => $cache]);
echo $engine->render('card', ['name' => 'Volt']) . "\n";
file_put_contents($views . '/card.blade.php', 'Changed {{ $name }}');
echo $engine->render('card', ['name' => 'Volt']) . "\n";
echo count(glob($cache . '/*.php'));
?>
--EXPECT--
Cached Volt
Cached Volt
1
