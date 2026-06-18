--TEST--
Templix\Engine renders escaped and raw Blade-compatible output
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_render_basic_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/hello.blade.php', 'Hello {{ $name }} {!! $html !!}');

$engine = new Templix\Engine([
    'mode' => 'dev',
    'path' => $views,
    'cache' => $cache,
]);
echo $engine->render('hello', [
    'name' => '<Ada>',
    'html' => '<strong>OK</strong>',
]);
?>
--EXPECT--
Hello &lt;Ada&gt; <strong>OK</strong>
