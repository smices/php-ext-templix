--TEST--
Templix rejects unsafe template names
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_template_name_safety_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/safe.blade.php', 'OK');
file_put_contents($root . '/secret.blade.php', 'SECRET');

$engine = new Templix\Engine([
    'mode' => 'dev',
    'path' => $views,
    'cache' => $cache,
]);

foreach (['../secret', '/secret', 'php://filter', 'foo//bar', "foo\0bar", 'foo\\bar'] as $name) {
    try {
        $engine->render($name);
        echo "allowed:$name\n";
    } catch (Throwable $e) {
        echo get_class($e) . ':' . $e->getMessage() . "\n";
    }
}

echo $engine->render('safe') . "\n";
?>
--EXPECT--
Error:Invalid Templix template name
Error:Invalid Templix template name
Error:Invalid Templix template name
Error:Invalid Templix template name
Error:Invalid Templix template name
Error:Invalid Templix template name
OK
