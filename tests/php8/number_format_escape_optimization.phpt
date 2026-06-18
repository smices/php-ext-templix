--TEST--
Templix skips escaping safe number_format output only when separators are default
--EXTENSIONS--
templix
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_number_format_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);

file_put_contents($views . '/numbers.blade.php', <<<'BLADE'
safe={{ number_format(1234.5, 1) }}
custom={{ number_format(1234.5, 1, '<', '&') }}
BLADE);

$engine = new Templix\Engine(['mode' => 'prod', 'path' => $views, 'cache' => $cache]);
echo $engine->render('numbers');
--EXPECT--
safe=1,234.5
custom=1&amp;234&lt;5
