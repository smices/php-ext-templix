PHP_ARG_ENABLE(templix, whether to enable templix support,
[  --enable-templix       Enable templix support])

if test "$PHP_TEMPLIX" != "no"; then
  PHP_NEW_EXTENSION(templix, templix.c engine/compiler.c engine/renderer.c, $ext_shared)
fi
