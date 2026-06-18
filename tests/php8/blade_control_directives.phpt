--TEST--
Templix compiles Laravel-compatible Blade control directives
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_blade_control_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/users.blade.php', "@if (\$enabled)@foreach (\$users as \$user){{ \$user['name'] }};@endforeach@else Disabled @endif");

$engine = new Templix\Engine(['mode' => 'dev', 'path' => $views, 'cache' => $cache]);
echo $engine->render('users', [
    'enabled' => true,
    'users' => [
        ['name' => '<Ada>'],
        ['name' => 'Lin'],
    ],
]);
?>
--EXPECT--
&lt;Ada&gt;;Lin;
