--TEST--
Templix preserves native PHP foreach blocks in templates
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_native_php_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/items.blade.php', '<?php foreach ($items as $item): ?>{{ $item }} <?php endforeach ?>');

$engine = new Templix\Engine(['path' => $views, 'cache' => $cache]);
echo $engine->render('items', ['items' => ['A', 'B']]);
?>
--EXPECT--
A B 
