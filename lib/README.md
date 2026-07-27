# lib

Third-party JARs on the compile/run classpath.

## Expected file

| File | Role |
|------|------|
| `sqlite-jdbc-3.53.2.0.jar` | Xerial SQLite JDBC driver |

Referenced by the root [`Makefile`](../Makefile) as:

```text
CP = lib/sqlite-jdbc-3.53.2.0.jar
```

Used by `Color/colorMatch` and `Pack/PlateCatalog` to open `data/bricks.db`.

## Notes

- On newer JDKs you may see native-access warnings from the driver; they are harmless for this project. Optional JVM flag: `--enable-native-access=ALL-UNNAMED`.
- If you upgrade the JAR, update the filename in the `Makefile` to match.
