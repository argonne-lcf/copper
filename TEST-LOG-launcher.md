# Launcher/tool strict TDD evidence

## Vertical slice 1: independent scale checker, delayed registration

RED command:

```text
python3 -m unittest tests/test_copper_scale_check.py -v
```

RED result: exit 1, `FileNotFoundError: .../scripts/copper-scale-check` (1 test, FAILED/errors=1).

GREEN command:

```text
chmod +x scripts/copper-scale-check && python3 -m unittest tests/test_copper_scale_check.py -v
```

GREEN result: exit 0, 1 test, `OK`.

## Vertical slice 2: launcher autosizing/summary, malformed address book, fail-closed malformed readiness

RED command:

```text
python3 -m unittest tests/test_launch_copper_aurora.py -v
```

RED result: exit 1, 6 tests; 3 expected failures. Launcher rejected new `-R` option, emitted no auto-size preflight/summary, accepted malformed address book, and accepted malformed readiness output.

GREEN command:

```text
python3 -m unittest tests/test_launch_copper_aurora.py tests/test_copper_scale_check.py -v
```

GREEN result: exit 0, 7 tests, `OK`.

## Final verification

Command:

```text
bash -n scripts/launch_copper_aurora.sh && python3 -m py_compile scripts/copper-scale-check tests/test_copper_scale_check.py tests/test_launch_copper_aurora.py && python3 -m unittest tests/test_launch_copper_aurora.py tests/test_copper_scale_check.py -v && cmake -S . -B /tmp/copper-launcher-build -DCOPPER_BUILD_RUNTIME=OFF -DBUILD_TESTING=ON && ctest --test-dir /tmp/copper-launcher-build -R 'launch_copper_aurora|copper_scale_check' --output-on-failure
```

Result: exit 0. Python owned suite: 10 tests, `OK`. CTest wiring: 2/2 tests passed (`launch_copper_aurora`, `copper_scale_check`). Shell syntax and Python compilation passed.
