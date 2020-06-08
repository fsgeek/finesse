# Build Notes

Building Finesse for full debugging:


```
meson configure --werror --warnlevel 2 --debug -Db_sanitize=address -Dc_args=-Og build
```


