--TEST--
Templix compiles Laravel Blade isset and empty directives
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_laravel_conditions_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/conditions.blade.php', '@isset($user)<b>{{ $user->name }}</b>@endisset @empty($alerts)<i>No alerts</i>@endempty');

$engine = new Templix\Engine(['mode' => 'dev', 'path' => $views, 'cache' => $cache]);
echo $engine->render('conditions', [
    'user' => (object) ['name' => '<Taylor>'],
    'alerts' => [],
]);
?>
--EXPECT--
<b>&lt;Taylor&gt;</b> <i>No alerts</i>
