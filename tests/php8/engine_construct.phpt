--TEST--
Templix\Engine can be constructed with array configuration
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$engine = new Templix\Engine([
    'mode' => 'dev',
    'path' => __DIR__,
    'cache' => sys_get_temp_dir(),
]);
var_dump($engine instanceof Templix\Engine);
?>
--EXPECT--
bool(true)
