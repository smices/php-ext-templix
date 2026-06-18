--TEST--
Templix benchmark compares Laravel and Volt render paths
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$php = PHP_BINARY;
$ext = dirname(__DIR__, 2) . '/modules/templix.so';
$bench = dirname(__DIR__, 2) . '/bench/templix_vs_laravel_volt.php';
$cmd = escapeshellarg($php)
    . ' -d extension=' . escapeshellarg($ext)
    . ' ' . escapeshellarg($bench)
    . ' --iterations=25 --json';
$json = shell_exec($cmd);
$data = json_decode($json, true);
var_dump(isset($data['templix_prod_volt_component']));
var_dump(isset($data['laravel_blade_compiled_include']));
var_dump(isset($data['volt_compiled_include']));
var_dump(isset($data['user_render_basic_templix']));
var_dump($data['templix_prod_volt_component']['iterations'] === 25);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
